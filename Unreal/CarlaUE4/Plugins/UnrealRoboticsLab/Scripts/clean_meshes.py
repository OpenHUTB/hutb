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
准备 MuJoCo MJCF 网格以供 Unreal 导入。

解析 MJCF XML（展开 <include> 文件），将所有引用的网格文件（OBJ、STL）转换为 GLB 格式，
解决命名冲突（例如 link1.obj 和 link1.stl 均被转为 link1.glb），并生成更新后的 XML 文件，
可直接拖拽导入 Unreal.

安装：
    pip install trimesh numpy

用法：
    python clean_meshes_trimesh.py <path_to_xml>

示例：
    python clean_meshes_trimesh.py "path/to/mujoco_menagerie/franka_emika_panda/panda.xml"
"""


import trimesh
import numpy as np
from pathlib import Path
from xml.etree import ElementTree as ET
import sys
import copy


def clean_mesh(mesh):
    """使用 trimesh 清理网格。"""
    print(f"  Original: {len(mesh.vertices)} vertices, {len(mesh.faces)} faces")

    mesh.merge_vertices(merge_tex=False, merge_norm=False)
    mesh.remove_unreferenced_vertices()
    mesh.fix_normals()

    # 绕 X 轴旋转 -90 度，将 GLTF 的 Y 轴向上坐标系转换为 Unreal 的 Z 轴向上坐标系
    rotation_matrix = trimesh.transformations.rotation_matrix(-np.radians(90), [1, 0, 0])
    mesh.apply_transform(rotation_matrix)

    print(f"  Cleaned:  {len(mesh.vertices)} vertices, {len(mesh.faces)} faces")
    return mesh


def convert_mesh(input_path: Path, output_path: Path) -> bool:
    """将单个网格文件转换为 GLB。"""
    print(f"\n  Converting: {input_path.name} -> {output_path.name}")

    try:
        mesh = trimesh.load(str(input_path), force='mesh')

        if not isinstance(mesh, trimesh.Trimesh):
            print(f"  x Not a valid mesh: {input_path.name}")
            return False

        cleaned_mesh = clean_mesh(mesh)

        # 剥离内嵌的材质/纹理，防止 Unreal 的 Interchange 导入器
        # 将其创建为 Texture2D 而不是 StaticMesh。
        # 保留 UV 坐标，以便之后通过材质实例来应用纹理。
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
    """解析MJCF XML，转换网格，解决冲突，生成更新后的XML。"""

    if not xml_path.exists():
        print(f"Error: XML file not found: {xml_path}")
        return

    xml_dir = xml_path.parent

    print(f"XML: {xml_path}")
    print(f"Dir: {xml_dir}")
    print("=" * 60)

    # ET.parse() 打开文件并解析整个 XML，返回一个 ElementTree 对象。
    # tree.getroot() 取出根元素。对于 MJCF 文件，根就是 <mujoco> 标签。
    tree = ET.parse(str(xml_path))
    root = tree.getroot()

    # 从 <compiler> 元素中查找 meshdir 属性
    meshdir = ""
    for compiler in root.iter("compiler"):
        md = compiler.get("meshdir", "")
        if md:
            meshdir = md

    # Path 的 / 运算符是路径拼接，不是算术除法。
    mesh_base = xml_dir / meshdir if meshdir else xml_dir
    print(f"Mesh directory: {mesh_base}")

    # ------------------------------------------------------------------
    # 收集网格，递归展开 <include>
    # 相对的网格路径会相对于声明它们的文件所在目录进行解析（即 MuJoCo 的解析方式）。
    # <compiler meshdir="..."> 具有优先权，且相对于主 XML 目录。
    # ------------------------------------------------------------------
    mesh_elements = []  # (mesh_element, source_path) 列表
    # 递归展开 <include> 时的**“已访问文件”集合**，用来防止死循环和重复解析
    seen_includes = {xml_path.resolve()}

    # 递归函数：从给定节点收集网格和 <include>，并解析包含的文件
    def collect_from_include(node_root: ET.Element, owner_dir: Path):
        base_dir = mesh_base if meshdir else owner_dir

        for mesh_el in node_root.iter("mesh"):
            file_attr = mesh_el.get("file")
            if not file_attr:
                continue

            # 确保显式名称属性存在——如果省略，MuJoCo 默认使用文件名前缀。
            # 在更改文件属性之前，我们必须保留它。
            if mesh_el.get("name") is None:
                implicit_name = Path(file_attr).stem
                mesh_el.set("name", implicit_name)
                print(f"  Set explicit name='{implicit_name}' on mesh with file='{file_attr}'")

            # 添加网格条目：包括 mesh 元素和源文件路径
            mesh_elements.append((mesh_el, base_dir / file_attr))

        for include_el in node_root.iter("include"):
            file_attr = include_el.get("file")
            if not file_attr:
                continue
            # resolve() 是 pathlib.Path 的方法：把路径规范化成唯一的绝对路径。
            # 它做三件事：
            # 1.变成绝对路径（相对路径拼上当前工作目录）
            # 2.消掉 . 和 ..（纯字符串/路径层面的规整）
            # 3.解析符号链接（symlink 指向哪，就得到真实路径）
            include_path = (owner_dir / file_attr).resolve()
            if include_path in seen_includes:
                continue
            seen_includes.add(include_path)
            if not include_path.exists():
                print(f"  x Included file not found: {include_path}")
                continue
            print(f"  Expanding include: {include_path}")
            try:
                # 得到include的文件中的根节点
                include_root = ET.parse(str(include_path)).getroot()
            except ET.ParseError as e:
                print(f"  x Failed to parse {include_path}: {e}")
                continue
            # 再看include文件中的 <include>，递归展开
            collect_from_include(include_root, include_path.parent)

    collect_from_include(root, xml_dir)

    # 转换由 <flexcomp file="..."> 引用的网格（也包括来自 include 文件的网格）。
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

    # 阶段 1：规划输出文件名，检测冲突。
    # 映射：glb_stem -> (mesh_element, source_path) 列表
    stem_usage = {}

    for mesh_el, source_path in mesh_elements:
        glb_stem = source_path.stem  # 例如，从 "/…/assets/geometry/link1.stl" 得到 "link1"
        if glb_stem not in stem_usage:
            stem_usage[glb_stem] = []
        stem_usage[glb_stem].append((mesh_el, source_path))

    # 阶段 2：分配唯一的输出名称并解决冲突
    # 对于每个网格：在 XML 中保留原始格式（OBJ/STL）（MuJoCo 需要它），
    # 但重命名冲突的文件，使其文件名主干唯一。然后将每个网格一并转换为 GLB——
    # Unreal 的导入器会通过“更高优先级”的回退机制找到 GLB。
    # output_plan：由 (mesh_el, original_source, actual_source, output_glb) 组成的列表
    import shutil
    output_plan = []
    conflicts_found = 0

    for glb_stem, entries in stem_usage.items():
        if len(entries) == 1:
            # 无冲突
            mesh_el, source_path = entries[0]
            output_glb = source_path.with_suffix(".glb")
            # XML 中的 file 属性保持不变
            output_plan.append((mesh_el, source_path, source_path, output_glb))
        else:
            # 冲突——多个源文件共用同一主干名
            conflicts_found += len(entries) - 1
            print(f"  CONFLICT: {len(entries)} files map to stem '{glb_stem}':")
            for i, (mesh_el, source_path) in enumerate(entries):
                mesh_name = mesh_el.get("name", "?")
                print(f"    [{i}] mesh name='{mesh_name}' <- {source_path.name}")

            # 第一个保留原始名称，其余添加计数后缀
            for i, (mesh_el, source_path) in enumerate(entries):
                if i == 0:
                    # 无需重命名
                    output_glb = source_path.with_suffix(".glb")
                    output_plan.append((mesh_el, source_path, source_path, output_glb))
                else:
                    # 重命名：link1.stl -> link1_1.stl，link1_1.glb
                    # 保存到 meshdir 根目录（而不是源文件所在子目录），
                    # 这样 XML 的 file 属性可以保持为相对于 meshdir 的简单文件名。
                    new_stem = f"{glb_stem}_{i}"
                    ext = source_path.suffix  # .stl, .obj, etc.
                    renamed_source = mesh_base / f"{new_stem}{ext}"
                    output_glb = mesh_base / f"{new_stem}.glb"

                    # 更新 XML 的 file 属性（相对于 meshdir）
                    new_file_attr = f"{new_stem}{ext}"
                    mesh_el.set("file", new_file_attr)

                    mesh_name = mesh_el.get("name", "?")
                    print(f"    -> Renaming '{mesh_name}': {source_path.name} -> {new_stem}{ext}")

                    # 将源文件复制到新名称
                    if not source_path.exists():
                        print(f"    x Source file missing: {source_path} — skipping")
                        continue
                    if not renamed_source.exists() or renamed_source.stat().st_mtime < source_path.stat().st_mtime:
                        shutil.copy2(str(source_path), str(renamed_source))
                        print(f"    -> Copied {source_path.name} -> {renamed_source.name}")

                    output_plan.append((mesh_el, source_path, renamed_source, output_glb))

    print(f"\n{len(output_plan)} meshes to convert, {conflicts_found} naming conflicts resolved")
    print("=" * 60)

    # 阶段 3：将网格转换为 GLB（与原文件放在同一目录）
    success_count = 0
    for mesh_el, original_source, actual_source, output_glb in output_plan:
        mesh_name = mesh_el.get("name", "?")

        if not actual_source.exists():
            print(f"\n  x Source not found: {actual_source}")
            continue

        # 如果 GLB 已存在且比源文件新，则跳过
        if output_glb.exists() and output_glb.stat().st_mtime > actual_source.stat().st_mtime:
            print(f"\n  Skipping '{mesh_name}' (GLB up to date): {output_glb.name}")
            success_count += 1
            continue

        print(f"\n[{mesh_name}] {actual_source.name} -> {output_glb.name}")
        if convert_mesh(actual_source, output_glb):
            success_count += 1
        else:
            print(f"  x FAILED to convert {actual_source.name}")

    # 阶段 4：写出更新后的 XML
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
