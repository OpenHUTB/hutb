#!/bin/bash
# ==============================================================================
# setup.sh — Ubuntu/Linux adaptation of Windows setup.bat
#
# All dependencies (including UE4 plugins not shipped with the engine) are
# centralized in a single repository:
#
#     git@git.code.tencent.com:OpenHUTB/dependencies_u.git
#
# The repo contains Linux-compatible builds of every package (plugins,
# installers and C++ source packages).  This setup.sh only orchestrates:
#
#   1. Ensure system tools (git, curl, 7z, gcc, etc.)
#   2. Clone dependencies_u  →  Build/dependencies/
#   3. Extract UE4 plugins + UnrealRoboticsLab third-party deps
#   4. Install miniconda3 from the dependencies_u installer
#   5. Install system packages via apt
#   6. Optionally invoke Util/BuildTools/Setup.sh for the full C++ build
#
# Note: the conda envs hutb_3.14 ~ hutb_3.7 are created on demand by
# Util/BuildTools/BuildPythonAPI.sh before building the Python wheels.
#
# setup.bat 的 Windows 专属步骤有意不移植（Linux 替代方案见相应注释）：
#   便携 git 下载 / DirectX / vcvars64 / CMake、dotnet、GnuWin32 压缩包
#   → 由 apt 提供（Step 0 / Step 5）
#   src/*.zip 源码包预解压到 Build/
#   → Linux 的 Util/BuildTools/Setup.sh 自行下载源码并编译后清理，预解压会被删除
#
# Usage:
#   ./setup.sh [-h] [-s|--skip-prerequisites] [-i|--interactive]
#              [-g|--generate-project-files] [-l|--launch]
#              [-d|--direct-launch] [-p|--package]
#              [--python-root=PATH] [--download-only]
# ==============================================================================

set -e

# ==============================================================================
# -- Parse command-line arguments -----------------------------------------------
# ==============================================================================

DOC_STRING="Download and install all dependencies and UE4 plugins for Ubuntu.
All packages come from the dependencies_u repository (Linux builds)."

USAGE_STRING="Usage: $0 [-h] [-s|--skip-prerequisites] [-i|--interactive]
              [-g|--generate-project-files] [-l|--launch] [-d|--direct-launch]
              [-p|--package] [--python-root=PATH] [--download-only]"

SKIP_PREREQUISITES=false
DOWNLOAD_ONLY=false
LAUNCH=false
DIRECT_LAUNCH=false
GENERATE_PROJECT_FILES=false
PACKAGE=false
INTERACTIVE=false
PYTHON_ROOT=

OPTS=$(getopt -o hisgldp --long help,interactive,skip-prerequisites,generate-project-files,launch,direct-launch,package,python-root:,pyroot:,download-only -n 'parse-options' -- "$@")
eval set -- "$OPTS"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -s | --skip-prerequisites )
      SKIP_PREREQUISITES=true
      shift ;;
    -i | --interactive )
      # setup.bat 中仅设置未消费，Linux 同样只接受不处理
      INTERACTIVE=true
      shift ;;
    -g | --generate-project-files )
      GENERATE_PROJECT_FILES=true
      shift ;;
    -l | --launch )
      LAUNCH=true
      shift ;;
    -d | --direct-launch )
      DIRECT_LAUNCH=true
      shift ;;
    -p | --package )
      PACKAGE=true
      shift ;;
    --python-root | --pyroot )
      PYTHON_ROOT="$2"
      shift 2 ;;
    --download-only )
      DOWNLOAD_ONLY=true
      shift ;;
    -h | --help )
      echo "$DOC_STRING"
      echo "$USAGE_STRING"
      exit 0
      ;;
    * )
      shift ;;
  esac
done

# setup.bat 用 --python-root 给 install_prerequisites.bat 指定 Python 路径；
# Linux 的系统依赖走 apt（Step 5），该参数仅保留以兼容命令行，不实际使用。

# ==============================================================================
# -- Color helpers --------------------------------------------------------------
# ==============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
NC='\033[0m'

log()    { echo -e "${CYAN}[setup.sh]${NC} $1"; }
success(){ echo -e "${GREEN}[setup.sh]${NC} \xe2\x9c\x85 $1"; }
warn()   { echo -e "${YELLOW}[setup.sh]${NC} \xe2\x9a\xa0\xef\xb8\x8f  $1"; }
error()  { echo -e "${RED}[setup.sh]${NC} \xe2\x9d\x8c $1"; exit 1; }

# ==============================================================================
# -- Paths ---------------------------------------------------------------------
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/Build"
PLUGINS_DIR="$PROJECT_ROOT/Unreal/CarlaUE4/Plugins"

# ------------------------------------------------------------------------------
# dependencies_u — Ubuntu edition of the Windows dependencies repo.
#
# Repo layout (see README.md in the repo for full documentation):
#
#   dependencies_u/
#   |-- Plugins/           # UE4 plugins + third-party runtime libs
#   |   |-- RoadRunner_Plugins.zip
#   |   |-- CesiumForUnreal-426-v1.18.0-ue4.zip
#   |   |-- mujoco-3.7.0-linux-x86_64.tar.gz
#   |   |-- CoACD.zip
#   |   |-- glTFForUE4.zip
#   |   `-- libzmq-linux.zip
#   |-- prerequisites/     # Pre-packaged toolchain
#   |   `-- Miniconda3-py313_25.11.1-1-Linux-x86_64.sh
#   |-- src/               # C++ source packages (cross-platform)
#   |   |-- boost-1_86_0.zip
#   |   |-- chrono-src.zip
#   |   |-- eigen-3.3.7.zip
#   |   |-- ... (13 packages total)
#   |   `-- zlib-source.zip
#   |-- .gitattributes     # LFS tracking rules
#   `-- README.md
# ------------------------------------------------------------------------------

# DEPENDENCIES_REPO="git@git.code.tencent.com:OpenHUTB/dependencies_u.git"
# Fallback if SSH is not configured:
DEPENDENCIES_REPO="https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/dependencies_u.git"

DEPENDENCIES_DIR="$BUILD_DIR/dependencies"

URLAB_DIR="$PLUGINS_DIR/UnrealRoboticsLab"
URLAB_THIRD_PARTY="$URLAB_DIR/third_party/install"

# ------------------------------------------------------------------------------
# Mirrors setup.bat: prepend prerequisite tool dirs to PATH.
# Windows 的 CMake/dotnet/GnuWin32 在 Linux 上由 apt 提供（Step 0/Step 5），
# 这里只前置 miniconda 相关目录（目录存在才加，避免 PATH 里出现死路径）。
# ------------------------------------------------------------------------------
for p in \
    "Build/dependencies/prerequisites/miniconda3/bin" \
    "Build/dependencies/prerequisites/miniconda3/envs/hutb_3.8/bin" ; do
    if [ -d "$PROJECT_ROOT/$p" ]; then
        export PATH="$PROJECT_ROOT/$p:$PATH"
        log "Prepended to PATH: $PROJECT_ROOT/$p"
    fi
done

# ==============================================================================
# -- Mirrors setup.bat :main: -g 生成工程文件 / -d 直接启动编辑器 ----------------
# ==============================================================================

if [ "$GENERATE_PROJECT_FILES" = true ]; then
    log "Generating project files..."
    if [ -z "${UE4_ROOT:-}" ]; then
        error "UE4_ROOT is not set — run 'source ./setEnv64.sh' first."
    fi
    # 
    "$UE4_ROOT/GenerateProjectFiles.sh" \
        -project="$PROJECT_ROOT/Unreal/CarlaUE4/CarlaUE4.uproject" -game -engine -progress || \
        error "GenerateProjectFiles.sh failed."
fi

# 跳过所有安装步骤，直接启动编辑器
if [ "$DIRECT_LAUNCH" = true ]; then
    log "Directly launching Unreal Editor, log to launch.log..."
    if [ -z "${UE4_ROOT:-}" ]; then
        error "UE4_ROOT is not set — run 'source ./setEnv64.sh' first."
    fi
    UE4_EDITOR="$UE4_ROOT/Engine/Binaries/Linux/UE4Editor"
    UPROJECT="$PROJECT_ROOT/Unreal/CarlaUE4/CarlaUE4.uproject"
    if [ ! -f "$UE4_EDITOR" ]; then
        error "UE4Editor not found at $UE4_EDITOR, please check if the build step is finished and the file exists."
    fi
    log "Found UE4Editor at $UE4_EDITOR, launching..."
    nohup "$UE4_EDITOR" "$UPROJECT" >launch.log 2>&1 &
    exit 0
fi

# ==============================================================================
# -- Banner --------------------------------------------------------------------
# ==============================================================================

log "=============================================="
log "  HUTB Ubuntu Setup Script"
log "  (adapted from setup.bat)"
log "=============================================="
log "Project root     : $PROJECT_ROOT"
log "Build dir        : $BUILD_DIR"
log "Plugins dir      : $PLUGINS_DIR"
log "Dependencies repo: $DEPENDENCIES_REPO"
log ""

# ==============================================================================
# -- Step 0: Ensure essential system tools -------------------------------------
# ==============================================================================
# Mirrors the Windows .bat which downloads git + 7zip first, then uses them to
# obtain everything else.  On Ubuntu these are native system packages.

log "=============================================="
log "  Step 0 — Essential System Tools"
log "=============================================="

MISSING_TOOLS=()
for cmd in git curl wget unzip make gcc g++; do
    if ! command -v "$cmd" &>/dev/null; then
        MISSING_TOOLS+=("$cmd")
    fi
done

if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    warn "Installing missing tools: ${MISSING_TOOLS[*]}"
    if sudo -n true 2>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq "${MISSING_TOOLS[@]}"
    else
        warn "  sudo 不可用（需要密码）— 请手动执行: sudo apt-get install ${MISSING_TOOLS[*]}"
    fi
fi

# 7z — used extensively in .bat for extracting archives
if ! command -v 7z &>/dev/null; then
    log "Installing p7zip-full..."
    if sudo -n true 2>/dev/null; then
        sudo apt-get install -y -qq p7zip-full
    else
        warn "  sudo 不可用（需要密码）— 请手动执行: sudo apt-get install p7zip-full"
    fi
fi

success "Essential tools ready."
log ""

# ==============================================================================
# -- Step 1: Ensure Build directory ---------------------------------------------
# ==============================================================================
# Mirrors: if not exist "%cd%\Build" mkdir "%cd%\Build"

if [ ! -d "$BUILD_DIR" ]; then
    log "Creating Build/ directory..."
    mkdir -p "$BUILD_DIR"
else
    log "Build/ directory already exists."
fi

# ==============================================================================
# -- Step 2: Clone dependencies_u repository -----------------------------------
# ==============================================================================
# Mirrors:
#   git clone https://.../dependencies.git Build\dependencies
#   cd dependencies && git lfs pull
#
# The Windows .bat targets "dependencies" (Windows builds).
# We target "dependencies_u" — the Ubuntu counterpart.

log "=============================================="
log "  Step 2 — Clone dependencies_u"
log "=============================================="

export GIT_LFS_SKIP_SMUDGE=1

if [ ! -d "$DEPENDENCIES_DIR" ]; then
    log "Cloning $DEPENDENCIES_REPO ..."
    # 这里使用 pushd "$BUILD_DIR" 是为了让 dependencies_u 仓库直接克隆到 Build/dependencies/ 下，而不是 Build/ 下。
    pushd "$BUILD_DIR" >/dev/null

    git clone "$DEPENDENCIES_REPO" dependencies 2>/dev/null || {
        warn ""
        warn "============================================"
        warn "  Failed to clone dependencies_u."
        warn "  Please check:"
        warn "    1. SSH key is added to git.code.tencent.com"
        warn "    2. The repo exists at:"
        warn "       git@git.code.tencent.com:OpenHUTB/dependencies_u.git"
        warn "  Or use HTTPS fallback (edit setup.sh)."
        warn "============================================"
        warn ""
        warn "  Continuing without dependencies — some"
        warn "  features will be unavailable."
    }

    if [ -d "$DEPENDENCIES_DIR" ]; then
        cd "$DEPENDENCIES_DIR"
        git lfs pull 2>/dev/null || warn "git lfs pull failed (non-critical)."
        success "dependencies_u cloned."
    fi

    popd >/dev/null
else
    log "dependencies/ already exists, skipping clone."
    log "  (To force re-clone: rm -rf $DEPENDENCIES_DIR && re-run)"
fi

log ""

# ==============================================================================
# -- Step 3: Extract UE4 plugins + third-party deps -----------------------------
# ==============================================================================
# Mirrors setup.bat 的 "Unzip Plugins" + "UnrealRoboticsLab dependencies" 段，
# 提取逻辑与 CI 工作流保持一致（含符号链接修正与 CoACD 目录嵌套修正）。
#
#   - DirectX / DirectX_Runtime / 7zip / CMake / dotnet / git / GnuWin32 压缩包
#     → Linux 由 apt 提供（Step 0 / Step 5）
#   - src/*.zip 源码包解压到 Build/
#     → Linux 的 Util/BuildTools/Setup.sh 会自行下载源码并在编译后清理，
#       预解压反而会被 Setup.sh 内部的 rm -Rf *-source 删掉

if [ -d "$DEPENDENCIES_DIR" ]; then
    log "=============================================="
    log "  Step 3 — Extract UE4 plugins"
    log "=============================================="

    # --- RoadRunner / Cesium / glTFForUE4 插件（目标目录已存在则跳过）---
    if [ ! -d "$PLUGINS_DIR/RoadRunnerRuntime" ]; then
        log "Unzipping RoadRunner Plugins ..."
        7z x "$DEPENDENCIES_DIR/Plugins/RoadRunner_Plugins.zip" -o"$PLUGINS_DIR/" -y >/dev/null || \
            warn "  RoadRunner_Plugins.zip 解压失败"
    else
        log "RoadRunner Plugins already exists."
    fi

    if [ ! -d "$PLUGINS_DIR/CesiumForUnreal" ]; then
        log "Unzipping CesiumForUnreal Plugin ..."
        7z x "$DEPENDENCIES_DIR/Plugins/CesiumForUnreal-426-v1.18.0-ue4.zip" -o"$PLUGINS_DIR/" -y >/dev/null || \
            warn "  CesiumForUnreal 解压失败"
    else
        log "CesiumForUnreal Plugin already exists."
    fi

    if [ ! -d "$PLUGINS_DIR/glTFForUE4" ]; then
        log "Unzipping glTFForUE4 Plugin ..."
        7z x "$DEPENDENCIES_DIR/Plugins/glTFForUE4.zip" -o"$PLUGINS_DIR/" -y >/dev/null || \
            warn "  glTFForUE4.zip 解压失败"
    else
        log "glTFForUE4 Plugin already exists."
    fi

    # --- UnrealRoboticsLab 第三方依赖 ---
    log "Initial UnrealRoboticsLab dependencies..."
    if [ ! -d "$URLAB_THIRD_PARTY" ]; then
        mkdir -p "$URLAB_THIRD_PARTY/MuJoCo"
        tar -xzf "$DEPENDENCIES_DIR/Plugins/mujoco-3.7.0-linux-x86_64.tar.gz" \
            -C "$URLAB_THIRD_PARTY/MuJoCo" --strip-components=1 2>/dev/null || \
            warn "  mujoco 解压失败"
        unzip -qo "$DEPENDENCIES_DIR/Plugins/CoACD.zip" -d "$URLAB_THIRD_PARTY" 2>/dev/null || \
            warn "  CoACD.zip 解压失败"
        unzip -qo "$DEPENDENCIES_DIR/Plugins/libzmq-linux.zip" -d "$URLAB_THIRD_PARTY" 2>/dev/null || \
            warn "  libzmq-linux.zip 解压失败"
    else
        log "Found UnrealRoboticsLab dependencies at $URLAB_THIRD_PARTY."
    fi

    # --- 幂等修正（与 CI 工作流一致）---
    # mujoco 符号链接 → 真实文件（打包时要求真实 .so）
    MUJOCO_LIB="$URLAB_THIRD_PARTY/MuJoCo/lib"
    if [ -L "$MUJOCO_LIB/libmujoco.so" ]; then
        REAL=$(readlink -f "$MUJOCO_LIB/libmujoco.so")
        rm "$MUJOCO_LIB/libmujoco.so" && cp "$REAL" "$MUJOCO_LIB/libmujoco.so"
    fi
    # CoACD 目录嵌套修正: CoACD/CoACD/include -> CoACD/include
    if [ -d "$URLAB_THIRD_PARTY/CoACD/CoACD/include" ] && [ ! -d "$URLAB_THIRD_PARTY/CoACD/include" ]; then
        mv "$URLAB_THIRD_PARTY/CoACD/CoACD/include" "$URLAB_THIRD_PARTY/CoACD/include"
    fi
    # libzmq 符号链接 → 真实 .so
    ZMQ_LIB="$URLAB_THIRD_PARTY/libzmq/lib"
    if [ -L "$ZMQ_LIB/libzmq.so" ]; then
        REAL=$(readlink -f "$ZMQ_LIB/libzmq.so")
        rm "$ZMQ_LIB/libzmq.so" "$ZMQ_LIB/libzmq.so.5" 2>/dev/null || true
        cp "$REAL" "$ZMQ_LIB/" && mv "$ZMQ_LIB/$(basename "$REAL")" "$ZMQ_LIB/libzmq.so"
    fi

    success "UE4 plugins extracted."
else
    warn "dependencies_u not available — skipping plugin extraction."
fi

log ""

# ==============================================================================
# -- Step 4: Miniconda3 ----------------------------------------------------------
# ==============================================================================
# dependencies_u 已不再提供 init.sh，插件解压由上面的 Step 3 完成。
# 这里只负责安装 miniconda3 本体，供 BuildPythonAPI.sh 创建 hutb_3.X 环境。
MINICONDA_DIR="$DEPENDENCIES_DIR/prerequisites/miniconda3"

if [ ! -d "$MINICONDA_DIR" ]; then
    log "Unzipping miniconda..."
    MINICONDA_INSTALLER="$DEPENDENCIES_DIR/prerequisites/Miniconda3-py313_25.11.1-1-Linux-x86_64.sh"
    if [ -f "$MINICONDA_INSTALLER" ]; then
        bash "$MINICONDA_INSTALLER" -b -p "$MINICONDA_DIR" >/dev/null 2>&1 || \
            warn "  miniconda3 extraction failed."
    else
        warn "  miniconda3 archive not found: $MINICONDA_INSTALLER"
    fi
else
    log "miniconda3 folder already exists."
fi

log ""

# ==============================================================================
# -- Step 5: System packages via apt -------------------------------------------
# ==============================================================================

if [ "$SKIP_PREREQUISITES" = false ]; then
    log "=============================================="
    log "  Step 5 — System Prerequisites (apt)"
    log "=============================================="

    # CI runner / 无终端环境下 sudo 可能需要密码：先探测免密 sudo，
    # 不可用则跳过本步骤（warn 提示手动安装），不让整个 setup 失败。
    if sudo -n true 2>/dev/null; then
        log "Installing build dependencies..."
        sudo apt-get update -qq

        sudo apt-get install -y -qq \
            build-essential \
            clang-10 \
            libc++-dev \
            libc++abi-dev \
            ninja-build \
            python3 \
            python3-dev \
            python3-pip \
            python3-venv \
            libomp-dev \
            libssl-dev \
            libncurses5 \
            libncurses5-dev \
            libsdl2-dev \
            libtiff5-dev \
            libjpeg-dev \
            libcurl4-openssl-dev \
            libzmq3-dev \
            doxygen \
            patchelf \
            libxml2-dev \
            libicu-dev \
            2>/dev/null || warn "Some apt packages may have failed to install."

        # clang-10 may not exist on newer Ubuntu (e.g. 24.04); fall back to clang
        if ! command -v clang-10 &>/dev/null && ! command -v clang &>/dev/null; then
            warn "clang not found — installing clang..."
            sudo apt-get install -y -qq clang
        fi

        success "System prerequisites installed."
    else
        warn "sudo 不可用（需要密码）— 跳过 apt 安装。"
        warn "  请手动安装本步骤列出的依赖，或在交互终端中用 sudo 重跑本脚本。"
    fi
else
    log "Skipping system prerequisites (--skip-prerequisites)."
fi

log ""

# ==============================================================================
# -- Step 6: Build C++ dependencies --------------------------------------------
# ==============================================================================

if [ "$DOWNLOAD_ONLY" = false ]; then
    log "=============================================="
    log "  Step 6 — C++ Dependency Build"
    log "=============================================="

    BUILD_TOOLS_SETUP="$PROJECT_ROOT/Util/BuildTools/Setup.sh"

    if [ -f "$BUILD_TOOLS_SETUP" ]; then
        log "Running $BUILD_TOOLS_SETUP --chrono ..."
        log "(This compiles boost, rpclib, gtest, recast, eigen, etc. from source)"
        log ""
        bash "$BUILD_TOOLS_SETUP" --chrono || warn "BuildTools/Setup.sh reported errors."
    else
        warn "$BUILD_TOOLS_SETUP not found — skipping C++ build."
        warn "Run 'make setup' or build dependencies manually."
    fi
else
    log "Skipping C++ build (--download-only)."
fi

log ""

# ==============================================================================
# -- Mirrors setup.bat: optional post-actions ----------------------------------
# ==============================================================================

if [ "$LAUNCH" = true ]; then
    log "Launching Unreal Editor, log to launch.log..."
    make launch ARGS="--chrono" >launch.log 2>&1 || warn "make launch reported errors."
fi

if [ "$PACKAGE" = true ]; then
    log "Packaging HUTB, log to package.log..."
    make package ARGS="--chrono" >package.log 2>&1 || warn "make package reported errors."
fi

# ==============================================================================
# -- Summary -------------------------------------------------------------------
# ==============================================================================

log "=============================================="
log "  Setup Complete!"
log "=============================================="
log ""

# List what's in Plugins
log "Plugins directory: $PLUGINS_DIR"
if [ -d "$PLUGINS_DIR" ]; then
    log "Available plugins:"
    for d in "$PLUGINS_DIR"/*/; do
        [ -d "$d" ] && echo "   - $(basename "$d")"
    done
else
    warn "Plugins directory does not exist!"
fi

log ""
log "Dependencies repo : $DEPENDENCIES_DIR"
log "Third-party (URLab): $URLAB_THIRD_PARTY"
log ""
log "Next steps:"
log "  1. Ensure UE4 engine is at ~/UnrealEngine_4.26"
log "  2. make CarlaUE4Editor"
log "  3. make package"
log ""

success "Done!"
