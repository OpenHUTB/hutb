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
generate_docs.py  —  URLab plugin documentation orchestrator.

Runs the full pipeline:
  1. parse_headers.py  → scans Public/ headers into parsed.json
  2. emit_docs.py      → writes docs/api/**/*.md  
  3. Updates the API nav section in mkdocs.yml
  4. (Optional) runs mkdocs serve or mkdocs build

Usage:
  python Scripts/generate_docs.py           # parse + emit, update nav
  python Scripts/generate_docs.py --serve   # then launch mkdocs serve
  python Scripts/generate_docs.py --build   # then run mkdocs build
"""

import os
import re
import sys
import json
import tempfile
import subprocess
import argparse
import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8')

import copy
from typing import Dict, List, Any

# ──────────────────────────────────────────────────────────────────────────────
# Paths
# ──────────────────────────────────────────────────────────────────────────────

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
PLUGIN_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..'))
PUBLIC_ROOT = os.path.join(PLUGIN_ROOT, 'Source', 'URLab', 'Public')
DOCS_ROOT   = os.path.join(PLUGIN_ROOT, 'docs')
MKDOCS_YML  = os.path.join(PLUGIN_ROOT, 'mkdocs.yml')
PARSED_JSON = os.path.join(PLUGIN_ROOT, 'docs', '_parsed_headers.json')

# ──────────────────────────────────────────────────────────────────────────────
# Step 1 — Parse headers
# ──────────────────────────────────────────────────────────────────────────────

def step_parse():
    print("\n─── Step 1: Parsing C++ headers ───")
    sys.path.insert(0, SCRIPT_DIR)
    from parse_headers import scan_public_headers  # type: ignore

    data = scan_public_headers(PUBLIC_ROOT)

    # Count totals
    total_classes = sum(len(pf.get('classes', [])) for pf in data)
    total_enums   = sum(len(pf.get('enums', []))   for pf in data)
    print(f"  ✓ Scanned {len(data)} files → {total_classes} classes/structs, {total_enums} enums")

    os.makedirs(DOCS_ROOT, exist_ok=True)
    with open(PARSED_JSON, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
    print(f"  ✓ Saved parsed data → {os.path.relpath(PARSED_JSON, PLUGIN_ROOT)}")
    return data

# ──────────────────────────────────────────────────────────────────────────────
# Step 2 — Emit Markdown
# ──────────────────────────────────────────────────────────────────────────────
def resolve_inheritance(data: List[Dict[str, Any]]):
    """
    Post-process parsed data to push inherited properties and functions down into subclasses.
    This ensures API pages for classes like UMjJointVelSensor actually show their inherited components.
    """
    print("\n─── Step 1.5: Resolving Inheritance ───")
    
    # 1. Build a lookup dict: class_name -> class object
    class_map = {}
    for f in data:
        for c in f.get('classes', []):
            class_map[c['name']] = c

    # 2. Helper to get all inherited members (recursive)
    def get_inherited_members(cls_name: str, memo=None) -> tuple:
        if memo is None: memo = set()
        if cls_name in memo: return [], []  # prevent cyclic loops just in case
        memo.add(cls_name)

        cls = class_map.get(cls_name)
        if not cls: return [], []

        props = copy.deepcopy(cls.get('properties', []))
        funcs = copy.deepcopy(cls.get('functions', []))

        parent_name = cls.get('parent')
        if parent_name and parent_name in class_map:
            p_props, p_funcs = get_inherited_members(parent_name, memo)
            props.extend(p_props)
            funcs.extend(p_funcs)
            
        return props, funcs

    # 3. Apply inherited members to all classes
    resolved_count = 0
    for f in data:
        for c in f.get('classes', []):
            parent_name = c.get('parent')
            if parent_name and parent_name in class_map:
                p_props, p_funcs = get_inherited_members(parent_name)
                
                # Filter out ones we already have (overrides)
                existing_prop_names = {p['name'] for p in c.get('properties', [])}
                existing_func_names = {f_['name'] for f_ in c.get('functions', [])}
                
                added_props = [p for p in p_props if p['name'] not in existing_prop_names]
                added_funcs = [f_ for f_ in p_funcs if f_['name'] not in existing_func_names]
                
                c.setdefault('properties', []).extend(added_props)
                c.setdefault('functions', []).extend(added_funcs)
                
                if added_props or added_funcs:
                    resolved_count += 1
                    
    print(f"  ✓ Inherited members resolved for {resolved_count} subclasses")


def step_emit(data):
    print("\n─── Step 2: Emitting Markdown pages ───")
    sys.path.insert(0, SCRIPT_DIR)
    from emit_docs import emit_docs, build_nav_yaml  # type: ignore

    nav, class_parent_map = emit_docs(data, DOCS_ROOT)
    total_pages = sum(len(v) for v in nav.values())
    print(f"  ✓ Emitted {total_pages} API pages into docs/api/")

    nav_yaml = build_nav_yaml(nav, class_parent_map)
    return nav_yaml

# ──────────────────────────────────────────────────────────────────────────────
# Step 3 — Update mkdocs.yml nav
# ──────────────────────────────────────────────────────────────────────────────

_NAV_SENTINEL = '  # -- AUTO-GENERATED BELOW THIS LINE (do not edit manually) --'
_NAV_API_STUB = '  - API Reference:\n    - api/index.md'

def step_update_nav(nav_yaml: str):
    print("\n─── Step 3: Updating mkdocs.yml nav ───")

    with open(MKDOCS_YML, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find the sentinel comment and replace everything after it (within nav)
    if _NAV_SENTINEL in content:
        before = content[:content.index(_NAV_SENTINEL)]
        new_section = f"{_NAV_SENTINEL}\n{nav_yaml}"
        new_content = before + new_section
    else:
        # Sentinel not found — append nav block
        print("  ⚠ Sentinel not found in mkdocs.yml — appending nav block")
        new_content = content.rstrip() + '\n' + _NAV_SENTINEL + '\n' + nav_yaml + '\n'

    with open(MKDOCS_YML, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"  ✓ Updated mkdocs.yml nav")

# ──────────────────────────────────────────────────────────────────────────────
# Step 4 — Run MkDocs
# ──────────────────────────────────────────────────────────────────────────────

def step_mkdocs(mode: str):
    """mode = 'serve' | 'build'"""
    import copy
    env = copy.copy(os.environ)
    env['PYTHONIOENCODING'] = 'utf-8'  # needed for emoji in generated pages on Windows
    cmd = ['mkdocs', mode]
    if mode == 'build':
        cmd.append('--clean')
    print(f"\n─── Step 4: Running `mkdocs {mode}` ───")
    print(f"  Working dir: {PLUGIN_ROOT}")
    subprocess.run(cmd, cwd=PLUGIN_ROOT, check=True, env=env)

# ──────────────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='Generate URLab plugin documentation.')
    parser.add_argument('--serve', action='store_true', help='Run mkdocs serve after generating')
    parser.add_argument('--build', action='store_true', help='Run mkdocs build after generating')
    parser.add_argument('--skip-parse', action='store_true',
                        help='Skip parsing step and reuse existing _parsed_headers.json')
    args = parser.parse_args()

    print(f"URLab Docs Generator")
    print(f"Plugin root: {PLUGIN_ROOT}")

    # Step 1
    if args.skip_parse and os.path.exists(PARSED_JSON):
        print(f"\n─── Step 1: Skipped (using cached {os.path.relpath(PARSED_JSON, PLUGIN_ROOT)}) ───")
        with open(PARSED_JSON, 'r', encoding='utf-8') as f:
            data = json.load(f)
    else:
        data = step_parse()
        
    resolve_inheritance(data)

    # Step 2
    nav_yaml = step_emit(data)

    # Step 3
    step_update_nav(nav_yaml)

    print("\n✅  Documentation generated successfully!")
    print(f"    Site source: {DOCS_ROOT}")
    print(f"    To preview:  mkdocs serve  (run from {PLUGIN_ROOT})")
    print(f"    To publish:  mkdocs gh-deploy")

    # Step 4 (optional)
    if args.serve:
        step_mkdocs('serve')
    elif args.build:
        step_mkdocs('build')


if __name__ == '__main__':
    main()
