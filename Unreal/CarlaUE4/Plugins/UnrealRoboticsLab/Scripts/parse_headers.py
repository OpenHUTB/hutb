# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# --- LEGAL DISCLAIMER ---
# UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
# endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
# trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
#
# This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
# CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

"""
parse_headers.py  —  UE-aware C++ header parser for URLab plugin docs.

Uses regex as primary strategy (libclang can't resolve UE types like TArray, FString 
without the full Unreal build environment). Regex extraction is fast, reliable for 
well-structured UE headers, and preserves the original type spellings exactly.

Outputs: list of ParsedFile dicts (or JSON to stdout/file).
"""

import os
import re
import json
import sys
from dataclasses import dataclass, field, asdict
from typing import Optional, List

# ──────────────────────────────────────────────────────────────────────────────
# Data structures
# ──────────────────────────────────────────────────────────────────────────────

@dataclass
class ParsedParam:
    name: str = ""
    type_str: str = ""
    brief: str = ""

@dataclass
class ParsedFunction:
    name: str = ""
    return_type: str = ""
    params: list = field(default_factory=list)
    specifiers: dict = field(default_factory=dict)
    macro: str = ""
    brief: str = ""
    description: str = ""
    is_blueprint_callable: bool = False
    is_blueprint_pure: bool = False
    is_call_in_editor: bool = False
    category: str = ""
    access: str = "public"

@dataclass
class ParsedProperty:
    name: str = ""
    type_str: str = ""
    default_value: str = ""
    specifiers: dict = field(default_factory=dict)
    macro: str = ""
    brief: str = ""
    is_edit_anywhere: bool = False
    is_visible_anywhere: bool = False
    is_blueprint_read_write: bool = False
    is_blueprint_read_only: bool = False
    category: str = ""
    edit_condition: str = ""
    is_override_toggle: bool = False
    override_guarded_property: str = ""
    access: str = "public"

@dataclass
class ParsedEnumValue:
    name: str = ""
    display_name: str = ""
    brief: str = ""

@dataclass
class ParsedEnum:
    name: str = ""
    specifiers: dict = field(default_factory=dict)
    brief: str = ""
    description: str = ""
    is_blueprint_type: bool = False
    values: list = field(default_factory=list)

@dataclass
class ParsedClass:
    name: str = ""
    kind: str = "class"
    macro: str = ""
    specifiers: dict = field(default_factory=dict)
    parent: str = ""
    brief: str = ""
    description: str = ""
    is_blueprint_spawnable: bool = False
    properties: list = field(default_factory=list)
    functions: list = field(default_factory=list)

@dataclass
class ParsedFile:
    file: str = ""
    relative_path: str = ""
    classes: list = field(default_factory=list)
    enums: list = field(default_factory=list)

# ──────────────────────────────────────────────────────────────────────────────
# Specifier string parser
# ──────────────────────────────────────────────────────────────────────────────

def _split_specifiers(s: str) -> list:
    """Split on top-level commas (respecting parens and quotes)."""
    parts = []
    depth = 0
    in_quote = False
    quote_char = None
    current = []
    for ch in s:
        if in_quote:
            current.append(ch)
            if ch == quote_char:
                in_quote = False
        elif ch in ('"', "'"):
            in_quote = True
            quote_char = ch
            current.append(ch)
        elif ch == '(':
            depth += 1
            current.append(ch)
        elif ch == ')':
            depth -= 1
            current.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(current))
            current = []
        else:
            current.append(ch)
    if current:
        parts.append(''.join(current))
    return parts

def _parse_specifiers(args: str) -> dict:
    result = {}
    for token in _split_specifiers(args.strip()):
        token = token.strip()
        if not token:
            continue
        if '=' in token:
            k, _, v = token.partition('=')
            result[k.strip()] = v.strip().strip('"').strip("'")
        else:
            result[token] = True
    return result

def _extract_meta_value(specs: dict, key: str, default="") -> str:
    """Extract a value from the meta=(...) string inside specifiers."""
    meta = specs.get('meta', '')
    if not meta:
        return default
    m = re.search(rf'{re.escape(key)}\s*=\s*["\']?([^,\)\'"]+)["\']?', meta)
    return m.group(1).strip() if m else default

# ──────────────────────────────────────────────────────────────────────────────
# Doc comment parser
# ──────────────────────────────────────────────────────────────────────────────

def _clean_doc_comment(raw: str) -> tuple:
    """
    Parse a /** ... */ comment block.
    Returns (brief: str, description: str).
    Strips leading * and processes @brief, @param, @return tags.
    """
    lines = raw.replace('\r', '').split('\n')
    brief = ""
    desc_lines = []
    for line in lines:
        l = line.strip().lstrip('*').strip()
        if not l:
            continue
        if l.startswith('@brief'):
            brief = l[len('@brief'):].strip()
        elif l.startswith('@class') or l.startswith('@struct') or l.startswith('@enum'):
            continue  # skip redundant tags
        elif l.startswith('@param'):
            parts = l.split(maxsplit=2)
            if len(parts) >= 3:
                desc_lines.append(f'- **`{parts[1]}`**: {parts[2]}')
            elif len(parts) == 2:
                desc_lines.append(f'- **`{parts[1]}`**')
        elif l.startswith('@return'):
            desc_lines.append(f'*Returns*: {l[7:].strip()}')
        elif l.startswith('@note'):
            desc_lines.append(f'> **Note:** {l[5:].strip()}')
        elif l.startswith('@warning'):
            desc_lines.append(f'> **Warning:** {l[8:].strip()}')
        elif l.startswith('@'):
            continue  # skip unknown tags
        else:
            if brief:
                desc_lines.append(l)
            # If no @brief yet, first plain line becomes brief
            if not brief:
                brief = l
    return brief, '\n'.join(desc_lines)

# ──────────────────────────────────────────────────────────────────────────────
# Balanced-paren macro argument extractor
# ──────────────────────────────────────────────────────────────────────────────

def _extract_macro_args(text: str, start: int) -> str:
    """
    Given text and the position of '(' after a macro name, 
    return the content between the balanced outer parens.
    """
    assert text[start] == '('
    depth = 0
    for i in range(start, len(text)):
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
            if depth == 0:
                return text[start+1:i]
    return text[start+1:]

# ──────────────────────────────────────────────────────────────────────────────
# Type string cleaner
# ──────────────────────────────────────────────────────────────────────────────

def _clean_type(raw: str) -> str:
    """Clean a raw type string from header parsing."""
    t = raw.strip()
    # Remove SYNTHY_API and similar export macros
    t = re.sub(r'\b\w+_API\b\s*', '', t)
    # Remove trailing/leading whitespace runs
    t = re.sub(r'\s+', ' ', t).strip()
    return t

# ──────────────────────────────────────────────────────────────────────────────
# Full regex-based parser
# ──────────────────────────────────────────────────────────────────────────────

# Pattern that finds doc block then optional UE macro then declaration
# We consume the file sequentially to handle scope correctly.

# Matches: /** doc comment */
_DOC_RE = re.compile(r'/\*\*(.*?)\*/', re.DOTALL)

# Matches a UE macro with balanced parens (simplified — we use _extract_macro_args for the args)
_UE_MACRO_START_RE = re.compile(
    r'\b(UCLASS|USTRUCT|UENUM|UPROPERTY|UFUNCTION|UENUM)\s*\('
)

# Matches class/struct declaration (after preprocessing macro)
_CLASS_RE = re.compile(
    r'\b(class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?::\s*(?:public\s+)?(\w+))?'
)

# Matches enum class declaration  
_ENUM_CLASS_RE = re.compile(
    r'\benum\s+class\s+(\w+)\s*(?::\s*\w+)?'
)

# Property declaration: type followed by name followed by ;/=/{
_PROP_DECL_RE = re.compile(
    r'((?:TArray\s*<[^>]+>|TMap\s*<[^>]+>|TSet\s*<[^>]+>|[\w:<>\*&\s,]+?))\s+(\w+)\s*(?:[;={]|UMETA)',
    re.DOTALL
)

# Function declaration: return_type FuncName(args)
_FUNC_DECL_RE = re.compile(
    r'((?:static\s+)?(?:virtual\s+)?(?:FORCEINLINE\s+)?(?:[\w:<>\*&\s]+?))\s+(\w+)\s*\(([^)]*)\)',
    re.DOTALL
)

# Enum value line: ValueName UMETA(...) or ValueName,
_ENUM_VALUE_RE = re.compile(
    r'^\s*(\w+)(?:\s+UMETA\s*\(\s*DisplayName\s*=\s*"([^"]+)"[^)]*\))?\s*[,}]',
    re.MULTILINE
)


def parse_header(filepath: str, public_root: str) -> Optional[dict]:
    """Parse a single .h file and return a ParsedFile dict."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            source = f.read()
    except Exception as e:
        print(f"[WARN] Could not read {filepath}: {e}", file=sys.stderr)
        return None

    rel_path = os.path.relpath(filepath, public_root).replace('\\', '/')
    result = ParsedFile(
        file=os.path.basename(filepath),
        relative_path=rel_path,
    )

    _parse_file(source, result)
    return asdict(result)


def _parse_file(source: str, result: ParsedFile):
    """
    Main parse loop. Walks source sequentially, building up ParsedClass/ParsedEnum objects.
    Strategy: find UCLASS/USTRUCT/UENUM blocks, then scan the body for UPROPERTY/UFUNCTION.
    """
    # ── 1. Find all UENUM blocks ──────────────────────────────────────────────
    enum_macro_re = re.compile(r'(?:/\*\*((?:(?!\*/).)*?)\*/\s*)?UENUM\s*\((.*?)\)\s*enum\s+class\s+(\w+)[^{]*\{', re.DOTALL)
    for m in enum_macro_re.finditer(source):
        raw_doc = m.group(1) or ''
        spec_str = m.group(2) or ''
        enum_name = m.group(3)
        brief, desc = _clean_doc_comment(raw_doc)
        specs = _parse_specifiers(spec_str)

        # Extract body
        body_start = m.end()
        body = _extract_to_closing_brace(source, body_start - 1)

        enm = ParsedEnum(
            name=enum_name,
            specifiers=specs,
            brief=brief,
            description=desc,
            is_blueprint_type=specs.get('BlueprintType', False) is True,
        )

        # Extract enum values
        for vm in _ENUM_VALUE_RE.finditer(body):
            vname = vm.group(1)
            if vname in ('GENERATED_BODY', 'GENERATED_USTRUCT_BODY'):
                continue
            display = vm.group(2) or vname
            enm.values.append(asdict(ParsedEnumValue(name=vname, display_name=display)))

        result.enums.append(asdict(enm))

    # ── 2. Find all UCLASS / USTRUCT blocks ───────────────────────────────────
    # Match: optional doc, then UCLASS/USTRUCT macro, then class/struct Name
    class_block_re = re.compile(
        r'(?:/\*\*((?:(?!\*/).)*?)\*/\s*)?'           # optional doc comment
        r'(UCLASS|USTRUCT)\s*\((.*?)\)\s*'  # UCLASS/USTRUCT macro
        r'(?:class|struct)\s+(?:\w+_API\s+)?(\w+)'  # class/struct Name
        r'(?:\s*:\s*(?:public\s+)?(\w+))?'         # optional : public Parent
        r'\s*\{',                                    # opening brace
        re.DOTALL
    )
    for m in class_block_re.finditer(source):
        raw_doc = m.group(1) or ''
        macro = m.group(2)
        spec_str = m.group(3) or ''
        cls_name = m.group(4)
        parent = m.group(5) or ''
        brief, desc = _clean_doc_comment(raw_doc)
        specs = _parse_specifiers(spec_str)

        # Extract class body
        body_start = m.end()
        body = _extract_to_closing_brace(source, body_start - 1)

        kind = 'class' if macro == 'UCLASS' else 'struct'
        cls = ParsedClass(
            name=cls_name,
            kind=kind,
            macro=macro,
            specifiers=specs,
            parent=parent,
            brief=brief,
            description=desc,
            is_blueprint_spawnable='BlueprintSpawnableComponent' in str(specs.get('meta', '')),
        )

        _parse_body(body, cls)
        result.classes.append(asdict(cls))

    # ── 3. Also capture undocumented plain classes with just /** doc */ ────────
    # If no UCLASS/USTRUCT match but there's a documented class, include it too
    plain_class_re = re.compile(
        r'/\*\*(.*?)\*/\s*'
        r'(?:class|struct)\s+(?:\w+_API\s+)?(\w+)'
        r'(?:\s*:\s*(?:public\s+)?(\w+))?'
        r'\s*\{',
        re.DOTALL
    )
    existing_names = {c['name'] for c in result.classes}
    for m in plain_class_re.finditer(source):
        raw_doc = m.group(1) or ''
        cls_name = m.group(2)
        parent = m.group(3) or ''
        if cls_name in existing_names or cls_name.startswith('F') and len(cls_name) > 20:
            continue
        brief, desc = _clean_doc_comment(raw_doc)
        if not brief:
            continue  # skip undocumented plain classes

        body_start = m.end()
        body = _extract_to_closing_brace(source, body_start - 1)
        kind = 'struct' if re.search(r'\bstruct\b', m.group(0)) else 'class'
        cls = ParsedClass(name=cls_name, kind=kind, parent=parent, brief=brief, description=desc)
        _parse_body(body, cls)
        result.classes.append(asdict(cls))
        existing_names.add(cls_name)


def _extract_to_closing_brace(source: str, open_pos: int) -> str:
    """
    Starting at open_pos (which must be the '{'), find the matching '}'.
    Returns the content between them (exclusive).
    """
    assert source[open_pos] == '{'
    depth = 0
    for i in range(open_pos, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return source[open_pos+1:i]
    return source[open_pos+1:]


def _strip_inline_bodies(text: str) -> str:
    """Replaces all content inside { ... } with just {} to prevent capturing local vars."""
    result = []
    depth = 0
    in_comment = False
    in_string = False
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        
        # Strings
        if not in_comment and c == '"':
            if i == 0 or text[i-1] != '\\':
                in_string = not in_string
            result.append(c)
            i += 1
            continue
            
        # Comments
        if not in_string and c == '/' and i + 1 < n:
            if text[i+1] == '*':
                in_comment = True
                result.append('/*')
                i += 2
                continue
            elif text[i+1] == '/':
                # Line comment, skip to newline
                while i < n and text[i] != '\n':
                    if depth == 0: result.append(text[i])
                    i += 1
                if i < n:
                    if depth == 0: result.append('\n')
                    i += 1
                continue
        if in_comment and c == '*' and i + 1 < n and text[i+1] == '/':
            in_comment = False
            result.append('*/')
            i += 2
            continue
            
        if in_comment or in_string:
            if depth == 0:
                result.append(c)
            i += 1
            continue
            
        # Braces
        if c == '{':
            if depth == 0:
                result.append('{')
            depth += 1
            i += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                result.append('}')
            i += 1
        else:
            if depth == 0:
                result.append(c)
            i += 1
            
    return ''.join(result)

def _parse_body(raw_body: str, cls: ParsedClass):
    """
    Parse the interior of a class/struct body for UPROPERTY and UFUNCTION declarations.
    """
    body = _strip_inline_bodies(raw_body)
    
    # Pre-calculate access specifier regions based on string index in body
    access_regions = []
    current_access = 'struct' if cls.kind == 'struct' else 'private'
    if current_access == 'struct': current_access = 'public' # structs default to public
    access_regions.append((0, current_access))
    
    for match in re.finditer(r'\b(public|protected|private)\s*:', body):
        access_regions.append((match.end(), match.group(1)))
        
    def get_access(pos: int) -> str:
        acc = access_regions[0][1]
        for r_pos, r_acc in access_regions:
            if pos >= r_pos:
                acc = r_acc
            else:
                break
        return acc

    # ── Properties ───────────────────────────────────────────────────────────
    # Pattern: optional doc, optional UPROPERTY(...), then type, then name;
    prop_re = re.compile(
        r'(?:/\*\*((?:(?!\*/).)*?)\*/\s*)?'                         # optional doc
        r'(?:UPROPERTY\s*\((.*?)\)\s*)?'               # optional UPROPERTY macro
        r'((?:TArray\s*<[^>]*>|TMap\s*<[^>]*>|TSet\s*<[^>]*>|TScriptInterface\s*<[^>]*>|'
        r'TArray\s*<[^{;]*>|[\w:<>\*&\s,]+?))\s*'  # type
        r'(\w+)\s*'                                      # name
        r'(?:\[[^\]]+\]\s*)?'                            # optional array bounds e.g. [3]
        r'(?:=\s*[^;{]+)?;',                              # optional default value
        re.DOTALL
    )
    for m in prop_re.finditer(body):
        raw_doc = m.group(1) or ''
        spec_str = m.group(2) or ''
        type_str = _clean_type(m.group(3))
        prop_name = m.group(4)

        # Skip generated body artifacts
        if prop_name in ('GENERATED_BODY', 'GENERATED_USTRUCT_BODY', 'BlueprintSpawnableComponent'):
            continue
        if not prop_name or (not prop_name[0].isupper() and not prop_name.startswith('b') and not prop_name[0].islower()):
            continue
            
        # Reject false positives matching inside UFUNCTION macros
        matched_str = m.group(0)
        if matched_str.count(')') > matched_str.count('('):
            continue
        if type_str.strip().endswith(','):
            continue
        if 'BlueprintPure' in type_str or 'BlueprintCallable' in type_str or 'CallInEditor' in type_str:
            continue
            
        has_macro = bool(spec_str)
        brief, desc = _clean_doc_comment(raw_doc)
        
        # If it's a UCLASS, we only care about UPROPERTY.
        # If it's a raw C++ class (no macro), we accept undocumented properties.
        if cls.macro in ('UCLASS', 'USTRUCT', 'UENUM') and not has_macro and not brief:
            continue

        brief, desc = _clean_doc_comment(raw_doc)
        specs = _parse_specifiers(spec_str)

        # Extract EditCondition from meta
        meta = specs.get('meta', '')
        ec_m = re.search(r'EditCondition\s*=\s*["\']?(\w+)["\']?', str(meta))
        edit_cond = ec_m.group(1) if ec_m else str(specs.get('EditCondition', '')).strip('"')

        prop = ParsedProperty(
            name=prop_name,
            type_str=type_str,
            brief=brief,
            macro='UPROPERTY',
            specifiers=specs,
            is_edit_anywhere=specs.get('EditAnywhere', False) is True,
            is_visible_anywhere=specs.get('VisibleAnywhere', False) is True,
            is_blueprint_read_write=specs.get('BlueprintReadWrite', False) is True,
            is_blueprint_read_only=specs.get('BlueprintReadOnly', False) is True,
            category=specs.get('Category', ''),
            edit_condition=edit_cond,
            is_override_toggle='InlineEditConditionToggle' in str(meta),
            access=get_access(m.start())
        )
        cls.properties.append(asdict(prop))

    # ── Functions ──────────────────────────────────────────────────────────────
    # Pattern: optional doc, optional UFUNCTION, return type, name, (args)
    func_re = re.compile(
        r'(?:/\*\*((?:(?!\*/).)*?)\*/\s*)?'                         # optional doc
        r'(?:UFUNCTION\s*\((.*?)\)\s*)?'               # optional UFUNCTION
        r'(?:static\s+)?(?:virtual\s+)?(?:FORCEINLINE\s+)?'
        r'([\w:<>\*&\s]+?)\s+'                           # return type
        r'(\w+)\s*\('                                    # function name
        r'([^)]*)\)',                                     # params
        re.DOTALL
    )
    seen_funcs = set()
    for m in func_re.finditer(body):
        raw_doc = m.group(1) or ''
        spec_str = m.group(2) or ''
        ret_type = _clean_type(m.group(3))
        fn_name = m.group(4)
        params_raw = m.group(5) or ''

        # Skip noise
        if fn_name in ('GENERATED_BODY', 'GENERATED_USTRUCT_BODY', 'if', 'while', 'for',
                       'switch', 'return', 'sizeof', 'alignof', 'static_assert'):
            continue
        # Skip UE macros that leaked into the function match
        if fn_name.startswith(('UFUNCTION', 'UPROPERTY', 'UCLASS', 'USTRUCT', 'UENUM',
                                'GENERATED', 'DECLARE', 'DEFINE', 'CHECK', 'ensure',
                                'checkf', 'UE_LOG')):
            continue
        # Skip non-UFUNCTION entries that have no doc comment in UCLASSes
        has_macro = bool(spec_str)
        brief, desc = _clean_doc_comment(raw_doc)
        if cls.macro in ('UCLASS', 'USTRUCT', 'UENUM') and not has_macro and not brief:
            continue
            
        if fn_name in seen_funcs:
            continue
        seen_funcs.add(fn_name)

        specs = _parse_specifiers(spec_str)

        # Parse params
        params = []
        if params_raw.strip():
            for param_token in _split_specifiers(params_raw):
                param_token = param_token.strip()
                if not param_token or param_token in ('void',):
                    continue
                # Last word is the name, rest is type
                # Use regex to cleanly separate type and name
                # e.g., "const FVector& NewPos = FVector::ZeroVector"
                # -> type: "const FVector&", name: "NewPos"
                pm = re.match(r'(.*?)([*&\s]+)(\w+)\s*(?:=.*)?$', param_token)
                if pm:
                    ptype = _clean_type(pm.group(1).strip() + pm.group(2).rstrip())
                    pname = pm.group(3)
                    params.append({'name': pname, 'type_str': ptype, 'brief': ''})
                else:
                    words = param_token.rsplit(None, 1)
                    if len(words) == 2:
                        ptype = _clean_type(words[0].rstrip('&*'))
                        pname = words[1].lstrip('&*')
                        if '=' in pname:
                            pname = pname[:pname.index('=')].strip()
                        params.append({'name': pname, 'type_str': ptype, 'brief': ''})

        fn = ParsedFunction(
            name=fn_name,
            return_type=ret_type,
            params=params,
            macro='UFUNCTION' if has_macro else '',
            specifiers=specs,
            brief=brief,
            description=desc,
            is_blueprint_callable=specs.get('BlueprintCallable', False) is True,
            is_blueprint_pure=specs.get('BlueprintPure', False) is True,
            is_call_in_editor=specs.get('CallInEditor', False) is True,
            category=specs.get('Category', ''),
            access=get_access(m.start())
        )
        cls.functions.append(asdict(fn))

    # ── Post-process: link override toggles ───────────────────────────────────
    _link_override_pairs(cls)


def _link_override_pairs(cls: ParsedClass):
    """Link bOverride_X toggles to their guarded properties."""
    toggle_props = {p['name']: p for p in cls.properties if p.get('is_override_toggle')}
    for prop in cls.properties:
        ec = prop.get('edit_condition', '')
        if ec and ec in toggle_props:
            prop['override_guarded_property'] = ''  # this prop is guarded by ec
            # Mark the toggle with the guarded name
            toggle_props[ec]['override_guarded_property'] = prop['name']


# ──────────────────────────────────────────────────────────────────────────────
# Scan all headers under a root directory
# ──────────────────────────────────────────────────────────────────────────────

def scan_public_headers(public_root: str) -> list:
    results = []
    for dirpath, _, files in os.walk(public_root):
        for fname in sorted(files):
            if not fname.endswith('.h'):
                continue
            fpath = os.path.join(dirpath, fname)
            parsed = parse_header(fpath, public_root)
            if parsed:
                results.append(parsed)
    return results


if __name__ == '__main__':
    if len(sys.argv) < 2:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        public_root = os.path.join(script_dir, '..', 'Source', 'URLab', 'Public')
    else:
        public_root = sys.argv[1]

    public_root = os.path.abspath(public_root)
    print(f"Scanning: {public_root}", file=sys.stderr)
    data = scan_public_headers(public_root)
    total_c = sum(len(pf.get('classes', [])) for pf in data)
    total_e = sum(len(pf.get('enums', [])) for pf in data)
    print(f"Parsed {len(data)} files → {total_c} classes, {total_e} enums", file=sys.stderr)

    out_path = sys.argv[2] if len(sys.argv) > 2 else None
    if out_path:
        with open(out_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
        print(f"Written to {out_path}", file=sys.stderr)
    else:
        print(json.dumps(data, indent=2))
