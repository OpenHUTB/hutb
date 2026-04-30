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
Prepare MuJoCo MJCF meshes for Unreal import.

Parses an MJCF XML, converts all referenced meshes (OBJ, STL) to GLB,
resolves naming conflicts (e.g., link1.obj and link1.stl both becoming link1.glb),
and writes an updated XML ready for drag-and-drop import into Unreal.

Installation:
    pip install trimesh numpy

Usage:
    python clean_meshes_trimesh.py <path_to_xml>

Example:
    python clean_meshes_trimesh.py "path/to/mujoco_menagerie/franka_emika_panda/panda.xml"
"""

import trimesh
import numpy as np
from pathlib import Path
from xml.etree import ElementTree as ET
import sys
import copy


def clean_mesh(mesh):
    """Clean up a mesh using trimesh."""
    print(f"  Original: {len(mesh.vertices)} vertices, {len(mesh.faces)} faces")

    mesh.merge_vertices(merge_tex=False, merge_norm=False)
    mesh.remove_unreferenced_vertices()
    mesh.fix_normals()

    # Rotate -90 degrees around X for GLTF Y-up -> Unreal Z-up
    rotation_matrix = trimesh.transformations.rotation_matrix(-np.radians(90), [1, 0, 0])
    mesh.apply_transform(rotation_matrix)

    print(f"  Cleaned:  {len(mesh.vertices)} vertices, {len(mesh.faces)} faces")
    return mesh


def convert_mesh(input_path: Path, output_path: Path) -> bool:
    """Convert a single mesh file to GLB."""
    print(f"\n  Converting: {input_path.name} -> {output_path.name}")

    try:
        mesh = trimesh.load(str(input_path), force='mesh')

        if not isinstance(mesh, trimesh.Trimesh):
            print(f"  x Not a valid mesh: {input_path.name}")
            return False

        cleaned_mesh = clean_mesh(mesh)

        # Strip embedded materials/textures to prevent Unreal's Interchange importer
        # from creating a Texture2D instead of a StaticMesh.
        # Preserve UV coordinates so textures can be applied via material instances.
        if hasattr(cleaned_mesh.visual, 'uv') and cleaned_mesh.visual.uv is not None:
            uv = cleaned_mesh.visual.uv.copy()
            cleaned_mesh.visual = trimesh.visual.TextureVisuals(uv=uv)
        else:
            cleaned_mesh.visual = trimesh.visual.ColorVisuals()

        if len(cleaned_mesh.vertices) == 0 or len(cleaned_mesh.faces) == 0:
            print(f"  x Mesh became empty after cleaning")
            return False

        bounds = cleaned_mesh.bounds
        size = bounds[1] - bounds[0]
        print(f"  Bounds: {size}")

        if np.allclose(size, 0):
            print(f"  Warning: Mesh has zero size!")

        cleaned_mesh.export(str(output_path))
        print(f"  -> Saved: {output_path.name}")
        return True

    except Exception as e:
        print(f"  x Error: {e}")
        return False


def process_xml(xml_path: Path):
    """Parse MJCF XML, convert meshes, resolve conflicts, write updated XML."""

    if not xml_path.exists():
        print(f"Error: XML file not found: {xml_path}")
        return

    xml_dir = xml_path.parent

    print(f"XML: {xml_path}")
    print(f"Dir: {xml_dir}")
    print("=" * 60)

    # Parse XML
    tree = ET.parse(str(xml_path))
    root = tree.getroot()

    # Find meshdir from compiler
    meshdir = ""
    for compiler in root.iter("compiler"):
        md = compiler.get("meshdir", "")
        if md:
            meshdir = md

    mesh_base = xml_dir / meshdir if meshdir else xml_dir
    print(f"Mesh directory: {mesh_base}")

    # Collect all mesh elements
    mesh_elements = list(root.iter("mesh"))

    # Also convert meshes referenced by <flexcomp file="...">
    for flexcomp in root.iter("flexcomp"):
        file_attr = flexcomp.get("file")
        if file_attr:
            source_path = mesh_base / file_attr
            if source_path.exists():
                output_glb = source_path.with_suffix(".glb")
                if not output_glb.exists() or output_glb.stat().st_mtime < source_path.stat().st_mtime:
                    print(f"\n[flexcomp] Converting mesh: {source_path.name} -> {output_glb.name}")
                    convert_mesh(source_path, output_glb)
                else:
                    print(f"\n[flexcomp] Mesh up to date: {output_glb.name}")

    print(f"Found {len(mesh_elements)} mesh assets in XML\n")

    # Phase 1: Plan output filenames, detect conflicts
    # Map: glb_stem -> list of (mesh_element, source_path)
    stem_usage = {}

    for mesh_el in mesh_elements:
        file_attr = mesh_el.get("file")
        if not file_attr:
            continue

        source_path = mesh_base / file_attr
        glb_stem = source_path.stem  # e.g., "link1" from "link1.stl"

        # Ensure explicit name attribute exists — MuJoCo defaults to filename stem
        # if omitted. We must preserve it before changing the file attribute.
        if mesh_el.get("name") is None:
            implicit_name = Path(file_attr).stem
            mesh_el.set("name", implicit_name)
            print(f"  Set explicit name='{implicit_name}' on mesh with file='{file_attr}'")

        if glb_stem not in stem_usage:
            stem_usage[glb_stem] = []
        stem_usage[glb_stem].append((mesh_el, source_path))

    # Phase 2: Assign unique output names and resolve conflicts
    # For each mesh: keep the original format (OBJ/STL) in the XML (MuJoCo needs it),
    # but rename conflicting files so their stems are unique. Then convert each to GLB
    # alongside — Unreal's importer will find the GLB via "higher priority" fallback.
    #
    # output_plan: list of (mesh_element, source_path, renamed_source_path, output_glb_path, new_file_attr)
    import shutil
    output_plan = []
    conflicts_found = 0

    for glb_stem, entries in stem_usage.items():
        if len(entries) == 1:
            # No conflict
            mesh_el, source_path = entries[0]
            output_glb = source_path.with_suffix(".glb")
            # file attr stays unchanged in the XML
            output_plan.append((mesh_el, source_path, source_path, output_glb))
        else:
            # Conflict — multiple source files share the same stem
            conflicts_found += len(entries) - 1
            print(f"  CONFLICT: {len(entries)} files map to stem '{glb_stem}':")
            for i, (mesh_el, source_path) in enumerate(entries):
                mesh_name = mesh_el.get("name", "?")
                print(f"    [{i}] mesh name='{mesh_name}' <- {source_path.name}")

            # First one keeps original name, rest get a counter suffix
            for i, (mesh_el, source_path) in enumerate(entries):
                if i == 0:
                    # No rename needed
                    output_glb = source_path.with_suffix(".glb")
                    output_plan.append((mesh_el, source_path, source_path, output_glb))
                else:
                    # Rename: link1.stl -> link1_1.stl, link1_1.glb
                    # Save to meshdir root (not the source subdirectory) so the
                    # XML file attribute stays a simple filename relative to meshdir.
                    new_stem = f"{glb_stem}_{i}"
                    ext = source_path.suffix  # .stl, .obj, etc.
                    renamed_source = mesh_base / f"{new_stem}{ext}"
                    output_glb = mesh_base / f"{new_stem}.glb"

                    # Update XML file attribute (relative to meshdir)
                    new_file_attr = f"{new_stem}{ext}"
                    mesh_el.set("file", new_file_attr)

                    mesh_name = mesh_el.get("name", "?")
                    print(f"    -> Renaming '{mesh_name}': {source_path.name} -> {new_stem}{ext}")

                    # Copy the source file to the new name
                    if not source_path.exists():
                        print(f"    x Source file missing: {source_path} — skipping")
                        continue
                    if not renamed_source.exists() or renamed_source.stat().st_mtime < source_path.stat().st_mtime:
                        shutil.copy2(str(source_path), str(renamed_source))
                        print(f"    -> Copied {source_path.name} -> {renamed_source.name}")

                    output_plan.append((mesh_el, source_path, renamed_source, output_glb))

    print(f"\n{len(output_plan)} meshes to convert, {conflicts_found} naming conflicts resolved")
    print("=" * 60)

    # Phase 3: Convert meshes to GLB (alongside the originals)
    success_count = 0
    for mesh_el, original_source, actual_source, output_glb in output_plan:
        mesh_name = mesh_el.get("name", "?")

        if not actual_source.exists():
            print(f"\n  x Source not found: {actual_source}")
            continue

        # Skip if GLB already exists and is newer than source
        if output_glb.exists() and output_glb.stat().st_mtime > actual_source.stat().st_mtime:
            print(f"\n  Skipping '{mesh_name}' (GLB up to date): {output_glb.name}")
            success_count += 1
            continue

        print(f"\n[{mesh_name}] {actual_source.name} -> {output_glb.name}")
        if convert_mesh(actual_source, output_glb):
            success_count += 1
        else:
            print(f"  x FAILED to convert {actual_source.name}")

    # Phase 4: Write updated XML
    output_xml = xml_path.parent / f"{xml_path.stem}_ue.xml"
    tree.write(str(output_xml), encoding="unicode", xml_declaration=True)

    print("\n" + "=" * 60)
    print(f"Processed {success_count}/{len(output_plan)} meshes successfully")
    print(f"Conflicts resolved: {conflicts_found}")
    print(f"Updated XML: {output_xml}")
    print(f"\nDrag '{output_xml.name}' into Unreal Content Browser to import.")


def main():
    if len(sys.argv) < 2:
        print("Error: No XML file specified")
        print("Usage: python clean_meshes_trimesh.py <path_to_xml>")
        print('Example: python clean_meshes_trimesh.py "C:/mujoco_menagerie/franka_emika_panda/panda.xml"')
        return

    xml_path = Path(sys.argv[1])

    if xml_path.suffix.lower() == ".xml":
        process_xml(xml_path)
    else:
        print(f"Error: Expected an .xml file, got '{xml_path.suffix}'")
        print("Usage: python clean_meshes_trimesh.py <path_to_xml>")


if __name__ == "__main__":
    main()
