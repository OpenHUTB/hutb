#!/usr/bin/env python3
import shutil
import subprocess
import sys
from pathlib import Path

import yaml

CONFIG_FILE = "submodule_cfg.yaml"


def run(cmd, cwd=None):
    print(f"Running: {cmd}")
    try:
        subprocess.run(cmd, shell=True, cwd=cwd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Command failed with error: {e}")


def load_config():
    with open(CONFIG_FILE) as f:
        return yaml.safe_load(f)


def install_submodules(selected=None):
    config = load_config()
    for name, info in config.items():
        print(f"\n----- Installing submodule: {name} -----")
        install = info.get("install", False)
        if (selected is None and not install) or (selected is not None and name not in selected):
            print(f"Skipping submodule: {name}")
            continue

        path = Path(info["path"])
        patches = info.get("patches", [])
        addons = info.get("addons", [])

        print(f"Initializing submodule '{name}'...")
        # If submodule already initialized, clean it first
        if (path / ".git").exists():
            print(f"Cleaning existing submodule '{name}'...")
            run(f"cd {path} && git reset --hard && git clean -fd")
        run(f"git submodule update --init {path}")

        if not path.exists():
            print(f"Path {path} does not exist. Skipping {name}.")
            continue

        for patch in patches:
            patch_path = path / patch
            if patch_path.exists():
                print(f"Applying patch {patch} for '{name}'...")
                run(f"cd {path} && git apply {patch}")
            else:
                print(f"Patch {patch} not found for '{name}', skipping.")

        for addon in addons:
            addon_path = path / addon
            if addon_path.exists():
                print(f"Adding addon '{addon_path}' to '{name}'...")
                shutil.copytree(addon_path, path, dirs_exist_ok=True)
            else:
                print(f"Addon path {addon} does not exist, skipping.")

        # install Python package
        packages = [path]  # main package
        if (extra_packages := info.get("extra_packages", None)) is not None:
            packages += extra_packages
        for pkg in packages:
            run(f"pip install -e {pkg}")


if __name__ == "__main__":
    selected_modules = sys.argv[1:] if len(sys.argv) > 1 else None
    # selected_modules will override install cfg if provided
    install_submodules(selected_modules)

    # Usage example:
    # python submodule_install.py mujoco_viewer unitree_cpp
