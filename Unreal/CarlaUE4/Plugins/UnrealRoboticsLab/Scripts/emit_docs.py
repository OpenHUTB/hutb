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
emit_docs.py  —  Markdown emitter for URLab plugin documentation.

Reads parsed header data (list of ParsedFile dicts) and writes:
  - docs/api/<category>/<ClassName>.md  (one per class, struct, enum)
  - docs/api/index.md                   (API index)
  - mkdocs_nav_api.yml                  (nav fragment merged into mkdocs.yml)
"""

import io
import os
import sys
import json
import re
from collections import defaultdict
# Force UTF-8 on Windows stdout
if hasattr(sys.stdout, 'buffer') and sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

# ──────────────────────────────────────────────────────────────────────────────
# Badge helpers
# ──────────────────────────────────────────────────────────────────────────────

_RAW_COMMENT_RE = re.compile(r'/\*+|\*+/|@brief|@param|@class|@struct|@enum|@return')

def _clean_brief(s: str) -> str:
    """Strip any residual raw comment markers from a brief/description field."""
    if not s:
        return ''
    s = _RAW_COMMENT_RE.sub('', s).strip()
    # Remove leading * lines
    s = re.sub(r'^\*+\s*', '', s, flags=re.MULTILINE).strip()
    return s

def _editor_badge(prop: dict) -> str:
    if prop.get('is_edit_anywhere'):
        return '✏️ EditAnywhere'
    if prop.get('is_visible_anywhere'):
        return '👁 VisibleAnywhere'
    return '—'

def _blueprint_prop_badge(prop: dict) -> str:
    if prop.get('is_blueprint_read_write'):
        return '🔵 ReadWrite'
    if prop.get('is_blueprint_read_only'):
        return '🟢 ReadOnly'
    return '—'

def _blueprint_fn_badge(fn: dict) -> str:
    parts = []
    if fn.get('is_blueprint_callable'):
        parts.append('🔵 Callable')
    if fn.get('is_blueprint_pure'):
        parts.append('💎 Pure')
    if fn.get('is_call_in_editor'):
        parts.append('🔧 CallInEditor')
    return ' '.join(parts) if parts else '—'

def _type_link(type_str: str, all_class_names: set) -> str:
    """If a type references a known class, make it a relative markdown link."""
    # Simple heuristic: check if any known name appears in the type string
    for name in sorted(all_class_names, key=len, reverse=True):
        if name in type_str:
            slug = name.lower()
            return type_str.replace(name, f'[{name}]({slug}.md)', 1)
    return f'`{type_str}`'

def _esc(s: str) -> str:
    """Escape pipe characters for Markdown table cells."""
    return s.replace('|', '\\|')

def _brief_or_empty(obj: dict) -> str:
    return _esc(_clean_brief(obj.get('brief', '') or ''))

# ──────────────────────────────────────────────────────────────────────────────
# Path-mirror category system
# ──────────────────────────────────────────────────────────────────────────────
#
# The docs section is derived directly from the source folder path.
# e.g.  Mujoco/Core/MjArticulation.h          → "Core"
#       Mujoco/Components/Actuators/…          → "Components/Actuators"
#       Mujoco/Components/Geometry/Primitives/ → "Components/Geometry/Primitives"
#       Mujoco/Factories/…                     → "Factories"
#       Mujoco/Net/…                           → "Net"
#       (top-level, Replay/, UI/, Utils/)      → "Other"
#
# Nav ordering uses this explicit list; any section not in the list goes last.
NAV_SECTION_ORDER = [
    # Core (most important)
    'Core',
    'Core/Defaults',
    'Core/Spec',
    # Components (main user-facing APIs)
    'Components',
    'Components/Bodies',
    'Components/Actuators',
    'Components/Joints',
    'Components/Sensors',
    'Components/Geometry',
    'Components/Geometry/Primitives',
    'Components/Physics',
    'Components/QuickConvert',
    # Supporting subsystems
    'Factories',
    'Net',
    'Replay',
    'UI',
    'Utils',
    # Catch-all
    'Other',
]

# Human-readable section titles and optional descriptions for the index page
_SECTION_META = {
    'Core':                        ('Core',                        'Main runtime actors: the physics manager, articulation, and robot.'),
    'Core/Defaults':               ('Core — Defaults',             'MuJoCo global default settings (geom, joint, actuator physics defaults).'),
    'Core/Spec':                   ('Core — Spec',                 'Internal spec wrapper types used during compilation.'),
    'Components':                  ('Components',                  'Base component classes shared by all MuJoCo element types.'),
    'Components/Bodies':           ('Components — Bodies',         'MuJoCo rigid body components (e.g., UMjBody).'),
    'Components/Actuators':        ('Components — Actuators',      'Actuator types: Position, Velocity, Motor, Damper, Cylinder, Muscle, Adhesion.'),
    'Components/Joints':           ('Components — Joints',         'Joint subtypes: Hinge, Slide, Ball, Free.'),
    'Components/Sensors':          ('Components — Sensors',        'Sensor types: joint, actuator, touch, rangefinder, tendon, energy, etc.'),
    'Components/Geometry':         ('Components — Geometry',       'Geometry attached to bodies: geoms, sites, cameras.'),
    'Components/Geometry/Primitives': ('Components — Primitives',  'Built-in primitive geom shapes: Box, Sphere, Cylinder.'),
    'Components/Physics':          ('Components — Physics',        'Physics configuration: inertial, contact pairs, contact exclusions.'),
    'Components/QuickConvert':     ('Components — Quick Convert',  'Convert existing UE actors and terrain to MuJoCo physics with one component.'),
    'Factories':                   ('Factories',                   'Asset factory and editor tools for importing MuJoCo XML files.'),
    'Net':                         ('Networking',                  'ZMQ bridge, sensor broadcaster, and control subscriber for external control.'),
    'Replay':                      ('Replay',                      'Record and replay physics simulation frames.'),
    'UI':                          ('UI',                          'Unreal UMG widgets for runtime simulation control.'),
    'Utils':                       ('Utilities',                   'Helper utilities, logging, mesh tools, and fake-robot debugging actors.'),
    'Other':                       ('Other',                       'Miscellaneous top-level types.'),
}


def _section_from_path(rel_path: str) -> str:
    """
    Derive the docs section label directly from the source relative_path.
    Strips the leading 'Mujoco/' prefix and the filename, then joins remaining folders.
    e.g. 'Mujoco/Components/Geometry/MjGeom.h' → 'Components/Geometry'
         'Mujoco/Core/MjArticulation.h'         → 'Core'
         'URLabGameMode.h'                        → 'Other'
    """
    parts = rel_path.replace('\\', '/').split('/')
    # Drop filename (last element)
    folders = parts[:-1]
    # Strip leading 'Mujoco' folder
    if folders and folders[0].lower() == 'mujoco':
        folders = folders[1:]
    # Strip leading 'Public' if present
    if folders and folders[0].lower() == 'public':
        folders = folders[1:]
    if not folders:
        return 'Other'
    return '/'.join(folders)


def _safe_folder(section: str) -> str:
    return section.lower().replace(' ', '_').replace('/', os.sep)


def _section_sort_key(section: str) -> int:
    try:
        return NAV_SECTION_ORDER.index(section)
    except ValueError:
        return len(NAV_SECTION_ORDER)




# ──────────────────────────────────────────────────────────────────────────────
# Per-class Markdown rendering
# ──────────────────────────────────────────────────────────────────────────────


def _link_type(tstr: str, class_map: dict) -> str:
    """Takes a raw type string (e.g. 'const UMjHingeJoint*') and inserts Markdown links for custom types."""
    import re
    res = _esc(tstr)  # escape HTML first so <TArray> doesn't break
    for name, link in class_map.items():
        if name in tstr:
            # We match word boundaries so we don't accidentally match substrings
            # Need to format link relative to the current file (we assume everything is flat for now, but really MkDocs handles absolute links from root if we prepend /api/)
            # Actually, relative to docs root, MkDocs resolves standard markdown links perfectly if we provide the path from docs/
            # E.g. [UMjJoint](components/joints/umjjoint.md)
            pattern = r'\b' + re.escape(name) + r'\b'
            res = re.sub(pattern, f'<a href="/{link.replace(".md", "")}/">{name}</a>', res)
    return res


def _render_class(cls: dict, class_map: dict) -> str:
    lines = []
    kind = cls.get('kind', 'class').capitalize()
    name = cls.get('name', '')
    parent = cls.get('parent', '')
    brief = _clean_brief(cls.get('brief') or '')
    desc = cls.get('description', '')
    macro = cls.get('macro', '')

    lines.append('---')
    lines.append('tags:')
    lines.append(f'  - {kind}')
    if cls.get('is_blueprint_spawnable'):
        lines.append('  - BlueprintSpawnable')
    lines.append('---')

    # Header
    lines.append(f'# {name}')
    if brief:
        lines.append(f'\n> {brief}\n')
    if desc:
        lines.append(f'{desc}\n')

    # Metadata table
    meta_rows = []
    meta_rows.append(f'| Kind | `{kind}` |')
    if macro:
        meta_rows.append(f'| UE Macro | `{macro}` |')
    if parent:
        # Try to link parent
        parent_link = f'[{parent}](/{class_map[parent].replace(".md", "")}/)' if parent in class_map else f'`{parent}`'
        meta_rows.append(f'| Inherits | {parent_link} |')
    if cls.get('is_blueprint_spawnable'):
        meta_rows.append('| Blueprint Spawnable | ✅ Yes |')
    if meta_rows:
        lines.append('| Attribute | Value |')
        lines.append('|---|---|')
        lines.extend(meta_rows)
        lines.append('')

    # Quick Navigation
    sections = []
    has_props = any(not p.get('is_override_toggle') for p in cls.get('properties', []))
    has_funcs = any(f.get('brief') or f.get('macro') == 'UFUNCTION' for f in cls.get('functions', []))
    
    if has_props:
        sections.append(('Properties', '#properties'))
    if has_funcs:
        sections.append(('Functions', '#functions'))
        
    if sections:
        nav_html = '<div style="display: flex; gap: 12px; margin-bottom: 32px; flex-wrap: wrap;">\n'
        for title, link in sections:
            nav_html += f'  <a href="{link}" class="md-button md-button--primary" style="margin: 0;">{title}</a>\n'
        nav_html += '</div>\n'
        lines.append(nav_html)

    # ── Properties ──
    props = [p for p in cls.get('properties', []) if not p.get('is_override_toggle')]
    if props:
        lines.append('## Properties\n')
        for access in ['public', 'protected', 'private']:
            access_props = [p for p in props if p.get('access', 'public') == access]
            if not access_props: continue
            
            lines.append(f'### {access.capitalize()} Properties\n')
            
            # --- Summary Table ---
            lines.append('| Property | Type | Description |')
            lines.append('|---|---|---|')
            for prop in access_props:
                pname = prop.get('name', '')
                ptype_raw = prop.get('type_str', '')
                ptype = _link_type(ptype_raw, class_map)
                brief_p = _clean_brief(prop.get('brief') or '')
                anchor = f"prop-{pname.lower()}"
                
                # If the type linking failed, wrap in code
                if ptype == ptype_raw: ptype = f'`{ptype_raw}`'
                
                lines.append(f'| [`{pname}`](#{anchor}) | {ptype} | {_esc(brief_p)} |')
            lines.append('')
            
            # --- Detailed Headings ---
            for prop in access_props:
                pname = prop.get('name', '')
                ptype = _link_type(prop.get('type_str', ''), class_map)
                editor = _editor_badge(prop)
                bp = _blueprint_prop_badge(prop)
                cat = _esc(prop.get('category') or '')
                brief_p = _clean_brief(prop.get('brief') or '')
                anchor = f"prop-{pname.lower()}"
                
                # Override note
                ec = prop.get('edit_condition', '')
                notes = f'Override-enabled (`{ec}`)' if ec else ''
                
                lines.append(f'<div class="urlab-api-item" markdown="1">')
                lines.append(f'#### {pname} {{: #{anchor} }}\n')
                if brief_p:
                    lines.append(f'> {brief_p}\n')
                elif notes:
                    lines.append(f'> {notes}\n')
                
                meta = []
                meta.append(f'* **Type:** {ptype}')
                if editor != '—': meta.append(f'* **Editor:** {editor}')
                if bp != '—': meta.append(f'* **Blueprint:** {bp}')
                if cat: meta.append(f'* **Category:** {cat}')
                if notes and brief_p: meta.append(f'* **Notes:** {notes}')
                
                lines.extend(meta)
                lines.append('</div>\n')

    # ── Functions ──
    fns = [f for f in cls.get('functions', []) if f.get('brief') or f.get('macro') == 'UFUNCTION']
    if fns:
        lines.append('## Functions\n')
        for access in ['public', 'protected', 'private']:
            access_fns = [f for f in fns if f.get('access', 'public') == access]
            if not access_fns: continue
            
            lines.append(f'### {access.capitalize()} Functions\n')
            
            # Group by category
            cat_groups = defaultdict(list)
            for fn in access_fns:
                cat_groups[fn.get('category', '')].append(fn)

            for cat, group in sorted(cat_groups.items()):
                if cat:
                    lines.append(f'#### {cat}\n')
                else:
                    lines.append(f'#### General\n')
                    
                # --- Summary Table ---
                lines.append('| Function | Returns | Description |')
                lines.append('|---|---|---|')
                
                # To handle potential function overloads, append an index to the anchor
                for i, fn in enumerate(group):
                    fname = fn.get('name', '')
                    ret_str = fn.get('return_type', 'void')
                    ret = _link_type(ret_str, class_map)
                    brief_f = _clean_brief(fn.get('brief') or '')
                    anchor = f"func-{fname.lower()}-{i}"
                    
                    if ret == ret_str and ret != 'void' and ret != '': ret = f'`{ret_str}`'
                    
                    lines.append(f'| [`{fname}()`](#{anchor}) | {ret} | {_esc(brief_f)} |')
                lines.append('')
                
                # --- Detailed Headings ---
                for i, fn in enumerate(group):
                    fname = fn.get('name', '')
                    bp_badge = _blueprint_fn_badge(fn)
                    ret_str = fn.get('return_type', 'void')
                    ret = _link_type(ret_str, class_map)
                    brief_f = _clean_brief(fn.get('brief') or '')
                    anchor = f"func-{fname.lower()}-{i}"
                    
                    # Params inline
                    params = fn.get('params', [])
                    param_strs = []
                    for p in params:
                        orig_type = p.get("type_str", "?")
                        pt_linked = _link_type(orig_type, class_map)
                        if pt_linked == orig_type:
                            pt_linked = f'`{orig_type}`'
                        pn = p.get("name", "")
                        if pn:
                            param_strs.append(f'{pt_linked} {pn}')
                        else:
                            param_strs.append(f'{pt_linked}')
                    
                    if len(params) > 1:
                        param_str = ', '.join(param_strs)
                        sig = f'`{fname}`({param_str})'
                    elif len(params) == 1:
                        sig = f'`{fname}`({param_strs[0]})'
                    else:
                        sig = f'`{fname}`()'
                    
                    lines.append(f'<div class="urlab-api-item" markdown="1">')
                    lines.append(f'##### {fname} {{: #{anchor} }}\n')
                    if brief_f:
                        lines.append(f'> {brief_f}\n')
                    
                    meta = []
                    meta.append(f'* **Signature:** {sig}')
                    if bp_badge != '—': meta.append(f'* **Blueprint:** {bp_badge}')
                    if ret_str != 'void' and ret_str != '' and ret != '`void`': meta.append(f'* **Returns:** {ret}')
                    
                    lines.extend(meta)
                    lines.append('</div>\n')

    return '\n'.join(lines)


def _render_enum(enm: dict, class_map: dict) -> str:
    lines = []
    name = enm.get('name', '')
    brief = _clean_brief(enm.get('brief') or '')
    desc = enm.get('description', '')
    is_bp = enm.get('is_blueprint_type', False)

    lines.append('---')
    lines.append('tags:')
    lines.append('  - Enum')
    if is_bp:
        lines.append('  - BlueprintType')
    lines.append('---')

    lines.append(f'# {name}  *(Enum)*')
    if brief:
        lines.append(f'\n> {brief}\n')
    if desc:
        lines.append(f'{desc}\n')

    lines.append('| Attribute | Value |')
    lines.append('|---|---|')
    lines.append(f'| UE Macro | `UENUM` |')
    if is_bp:
        lines.append('| Blueprint Type | ✅ Yes |')
    lines.append('')

    values = enm.get('values', [])
    if values:
        lines.append('## Values\n')
        lines.append('| Name | Display Name | Description |')
        lines.append('|---|---|---|')
        for v in values:
            vname = v.get('name', '')
            dname = v.get('display_name', '') or vname
            vbrief = _esc(_clean_brief(v.get('brief') or ''))
            lines.append(f'| `{vname}` | {dname} | {vbrief} |')
        lines.append('')

    return '\n'.join(lines)


# ──────────────────────────────────────────────────────────────────────────────
# Main emitter
# ──────────────────────────────────────────────────────────────────────────────

def emit_docs(parsed_files: list, docs_root: str) -> dict:
    """
    Emit Markdown files into docs_root/api/.
    Returns a dict of {section: [(name, rel_md_path), ...]} for nav building.
    """
    api_root = os.path.join(docs_root, 'api')
    os.makedirs(api_root, exist_ok=True)

    # Sections will be created on-demand per file (since sections mirror the source tree)

    # Collect all class/enum names for cross-linking
    class_map = {}  # name -> rel_md_path (assigned during iteration)
    class_parent_map = {} # name -> parent_name
    for pf in parsed_files:
        rel_path = pf.get('relative_path', '')
        section = _section_from_path(rel_path)
        for c in pf.get('classes', []):
            fname = c.get('name', '').lower() + '.md'
            name = c.get('name', '')
            class_map[name] = 'api/' + _safe_folder(section).replace(os.sep, '/') + '/' + fname
            class_parent_map[name] = c.get('parent')
        for e in pf.get('enums', []):
            fname = e.get('name', '').lower() + '.md'
            class_map[e.get('name', '')] = 'api/' + _safe_folder(section).replace(os.sep, '/') + '/' + fname

    nav = defaultdict(list)  # section → [(display_name, rel_path_from_docs)]
    all_entries = []  # for index page

    for pf in parsed_files:
        rel_path = pf.get('relative_path', '')

        for cls in pf.get('classes', []):
            name = cls.get('name', '')
            if not name:
                continue
            section = _section_from_path(rel_path)
            folder = os.path.join(api_root, _safe_folder(section))
            os.makedirs(folder, exist_ok=True)
            md = _render_class(cls, class_map)
            fname = name.lower() + '.md'
            out_path = os.path.join(folder, fname)
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(md)
            rel_md = 'api/' + _safe_folder(section).replace(os.sep, '/') + '/' + fname
            nav[section].append((name, rel_md))
            all_entries.append({
                'name': name, 'kind': cls.get('kind', 'class'),
                'brief': cls.get('brief', ''), 'path': rel_md, 'section': section,
                'parent': cls.get('parent')
            })

        for enm in pf.get('enums', []):
            name = enm.get('name', '')
            if not name:
                continue
            section = _section_from_path(rel_path)
            folder = os.path.join(api_root, _safe_folder(section))
            os.makedirs(folder, exist_ok=True)
            md = _render_enum(enm, class_map)
            fname = name.lower() + '.md'
            out_path = os.path.join(folder, fname)
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(md)
            rel_md = 'api/' + _safe_folder(section).replace(os.sep, '/') + '/' + fname
            nav[section].append((name, rel_md))
            all_entries.append({
                'name': name, 'kind': 'enum',
                'brief': enm.get('brief', ''), 'path': rel_md, 'section': section,
                'parent': None
            })

    _write_api_index(all_entries, api_root, class_parent_map)
    return nav, class_parent_map


def _get_depth(name: str, parent_map: dict, memo: set = None) -> int:
    if memo is None: memo = set()
    if name in memo: return 0
    memo.add(name)
    parent = parent_map.get(name)
    if not parent: return 0
    return 1 + _get_depth(parent, parent_map, memo)


def _write_api_index(entries: list, api_root: str, parent_map: dict):
    """Write docs/api/index.md — master class list, ordered by source folder structure."""
    lines = []
    lines.append('# API Reference\n')
    lines.append(
        'Auto-generated from C++ header files in `Source/URLab/Public/`.\n'
        'All UPROPERTY specifiers, Blueprint access, and override flags are preserved.\n'
        '\n'
        '> Organised to match the source folder structure in `Mujoco/`.\n'
    )

    # Group by section
    section_map = defaultdict(list)
    for e in sorted(entries, key=lambda x: (_get_depth(x['name'], parent_map), x['name'])):
        section_map[e['section']].append(e)

    for section in NAV_SECTION_ORDER:
        if section not in section_map:
            continue
        items = section_map[section]
        title, desc = _SECTION_META.get(section, (section, ''))
        lines.append(f'## {title}\n')
        if desc:
            lines.append(f'{desc}\n')
        lines.append('| Name | Kind | Description |')
        lines.append('|---|---|---|')
        for e in items:
            name = e['name']
            kind = e['kind'].capitalize()
            brief = _esc(_clean_brief(e.get('brief', '') or ''))
            rel = e['path'].replace('api/', '', 1)
            lines.append(f'| [{name}]({rel}) | {kind} | {brief} |')
        lines.append('')

    # Any sections not in NAV_SECTION_ORDER
    for section in sorted(section_map.keys()):
        if section in NAV_SECTION_ORDER:
            continue
        title, desc = _SECTION_META.get(section, (section, ''))
        lines.append(f'## {title}\n')
        lines.append('| Name | Kind | Description |')
        lines.append('|---|---|---|')
        for e in sorted(section_map[section], key=lambda x: (_get_depth(x['name'], parent_map), x['name'])):
            name = e['name']
            kind = e['kind'].capitalize()
            brief = _esc(_clean_brief(e.get('brief', '') or ''))
            rel = e['path'].replace('api/', '', 1)
            lines.append(f'| [{name}]({rel}) | {kind} | {brief} |')
        lines.append('')

    out_path = os.path.join(api_root, 'index.md')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))


def build_nav_yaml(nav: dict, parent_map: dict) -> str:
    """Build the YAML nav fragment, hierarchically."""
    lines = ['  - API Reference:']
    lines.append('    - Overview: api/index.md')

    tree = {}
    
    def add_to_tree(path_parts, items, current_node, full_path):
        if len(path_parts) == 1:
            folder_name = path_parts[0]
            if folder_name not in current_node:
                current_node[folder_name] = {}
            if '__items__' not in current_node[folder_name]:
                current_node[folder_name]['__items__'] = []
            current_node[folder_name]['__items__'].extend(items)
            current_node[folder_name]['__path__'] = full_path
        else:
            folder_name = path_parts[0]
            if folder_name not in current_node:
                current_node[folder_name] = {}
            
            # The full path to this node so far
            current_path = full_path.split(folder_name)[0] + folder_name
            current_node[folder_name]['__path__'] = current_path
                
            add_to_tree(path_parts[1:], items, current_node[folder_name], full_path)

    # Enforce ordering
    sections_to_emit = []
    for sec in NAV_SECTION_ORDER:
        if sec in nav: sections_to_emit.append(sec)
    for sec in sorted(nav.keys()):
        if sec not in NAV_SECTION_ORDER: sections_to_emit.append(sec)

    for sec in sections_to_emit:
        items = sorted(nav[sec], key=lambda x: (_get_depth(x[0], parent_map), x[0]))
        add_to_tree(sec.split('/'), items, tree, sec)

    def write_tree(node, depth):
        indent = '  ' * depth
        
        # Then write subfolders and items
        for k, v in node.items():
            if k in ('__items__', '__path__'):
                continue
                
            # Get the display title for this folder from _SECTION_META if it exists, otherwise use the folder name
            node_path = v.get('__path__', k)
            title, _ = _SECTION_META.get(node_path, (k, ''))
            # If the title contains the parent path (e.g. "Components — Sensors"), strip the parent part off for nested display
            if ' — ' in title:
                title = title.split(' — ')[-1]
                
            lines.append(f'{indent}- {title}:')
            
            # Write items directly inside this folder
            if '__items__' in v:
                for name, path in v['__items__']:
                    lines.append(f'{indent}  - {name}: {path}')
                    
            # Recurse for deeper folders
            write_tree(v, depth + 1)
            
    write_tree(tree, 2)
    return '\n'.join(lines)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: emit_docs.py <parsed.json> <docs_root>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r', encoding='utf-8') as f:
        parsed_files = json.load(f)

    docs_root = sys.argv[2]
    nav, class_parent_map = emit_docs(parsed_files, docs_root)
    print(f"Emitted docs for {sum(len(v) for v in nav.values())} items.", file=sys.stderr)
    print(build_nav_yaml(nav, class_parent_map))
