#!/bin/bash
# ==============================================================================
# setup.sh — Ubuntu/Linux adaptation of Windows setup.bat
#
# All dependencies (including UE4 plugins not shipped with the engine) are
# centralized in a single repository:
#
#     git@git.code.tencent.com:OpenHUTB/dependencies_u.git
#
# The repo contains Linux-compatible builds of every package AND an init.sh
# script that handles extraction.  This setup.sh only orchestrates:
#
#   1. Ensure system tools (git, curl, 7z, gcc, etc.)
#   2. Clone dependencies_u  →  Build/dependencies/
#   3. Run init.sh            →  extract everything to the right places
#   4. Install system packages via apt
#   5. Set up Python environment (conda or venv)
#   6. Optionally invoke Util/BuildTools/Setup.sh for the full C++ build
#
# Usage:
#   ./setup.sh [--skip-prerequisites] [--download-only] [--help]
# ==============================================================================

set -e

# ==============================================================================
# -- Parse command-line arguments -----------------------------------------------
# ==============================================================================

DOC_STRING="Download and install all dependencies and UE4 plugins for Ubuntu.
All packages come from the dependencies_u repository (Linux builds)."

USAGE_STRING="Usage: $0 [--skip-prerequisites] [--download-only] [--help]"

SKIP_PREREQUISITES=false
DOWNLOAD_ONLY=false

OPTS=$(getopt -o h --long help,skip-prerequisites,download-only -n 'parse-options' -- "$@")
eval set -- "$OPTS"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-prerequisites )
      SKIP_PREREQUISITES=true
      shift ;;
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
#   |   |-- mujoco-3.3.5-linux-x86_64.tar.gz
#   |   |-- CoACD.zip
#   |   `-- libzmq-linux.tar.gz
#   |-- prerequisites/     # Pre-packaged toolchain
#   |   `-- Miniconda3-latest-Linux-x86_64.sh
#   |-- src/               # C++ source packages (cross-platform)
#   |   |-- boost-1_86_0.zip
#   |   |-- chrono-src.zip
#   |   |-- eigen-3.3.7.zip
#   |   |-- ... (13 packages total)
#   |   `-- zlib-source.zip
#   |-- init.sh            # Extraction / initialization script
#   |-- .gitattributes     # LFS tracking rules
#   `-- README.md
# ------------------------------------------------------------------------------

# DEPENDENCIES_REPO="git@git.code.tencent.com:OpenHUTB/dependencies_u.git"
# Fallback if SSH is not configured:
DEPENDENCIES_REPO="https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/dependencies_u.git"

DEPENDENCIES_DIR="$BUILD_DIR/dependencies"

URLAB_DIR="$PLUGINS_DIR/UnrealRoboticsLab"
URLAB_THIRD_PARTY="$URLAB_DIR/third_party/install"

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
    sudo apt-get update -qq
    sudo apt-get install -y -qq "${MISSING_TOOLS[@]}"
fi

# 7z — used extensively in .bat for extracting archives
if ! command -v 7z &>/dev/null; then
    log "Installing p7zip-full..."
    sudo apt-get install -y -qq p7zip-full
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
# -- Step 3: Run dependencies_u/init.sh ----------------------------------------
# ==============================================================================
# All extraction / initialization logic lives inside the dependencies_u repo.
# setup.sh only orchestrates — init.sh does the actual work of unzipping
# plugins, extracting source packages, installing miniconda3, etc.

if [ ! -d "$DEPENDENCIES_DIR" ]; then
    warn "dependencies_u not available — skipping extraction."
else
    log "=============================================="
    log "  Step 3 — Run dependencies_u/init.sh"
    log "=============================================="

    INIT_SCRIPT="$DEPENDENCIES_DIR/init.sh"
    if [ -f "$INIT_SCRIPT" ]; then
        log "Delegating to $INIT_SCRIPT ..."
        log ""
        bash "$INIT_SCRIPT" "$PROJECT_ROOT"
        success "dependencies_u initialization complete."
    else
        error "init.sh not found in dependencies_u!"
        error "Expected at: $INIT_SCRIPT"
        error "The dependencies_u repo may be incomplete or corrupted."
        exit 1
    fi
fi

log ""

# ==============================================================================
# -- Step 4: System packages via apt -------------------------------------------
# ==============================================================================

if [ "$SKIP_PREREQUISITES" = false ]; then
    log "=============================================="
    log "  Step 4 — System Prerequisites (apt)"
    log "=============================================="

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
    log "Skipping system prerequisites (--skip-prerequisites)."
fi

log ""

# ==============================================================================
# -- Step 5: Python environment ------------------------------------------------
# ==============================================================================
# init.sh already installed miniconda3 to Build/dependencies/prerequisites/.
# Here we create the conda environment and install Python requirements.

log "=============================================="
log "  Step 5 — Python Environment"
log "=============================================="

MINICONDA_DIR="$BUILD_DIR/dependencies/prerequisites/miniconda3"

if [ -d "$MINICONDA_DIR" ] && [ -f "$MINICONDA_DIR/bin/conda" ]; then
    log "Setting up conda environment 'hutb_3.8'..."

    # Create environment if not already present
    if ! "$MINICONDA_DIR/bin/conda" env list 2>/dev/null | grep -q hutb_3.8; then
        "$MINICONDA_DIR/bin/conda" create -n hutb_3.8 python=3.8 -y 2>/dev/null || \
            warn "  Conda env creation failed."
    fi

    # Install Python requirements
    if [ -f "$PROJECT_ROOT/requirements.txt" ]; then
        log "  Installing Python requirements (requirements.txt)..."
        "$MINICONDA_DIR/envs/hutb_3.8/bin/pip" install -r "$PROJECT_ROOT/requirements.txt" 2>/dev/null || \
            warn "  pip install failed (non-critical)."
    fi

    if [ -f "$PROJECT_ROOT/PythonAPI/carla/requirements.txt" ]; then
        log "  Installing Python requirements (PythonAPI)..."
        "$MINICONDA_DIR/envs/hutb_3.8/bin/pip" install -r "$PROJECT_ROOT/PythonAPI/carla/requirements.txt" 2>/dev/null || true
    fi

    success "conda env 'hutb_3.8' ready."
else
    # Fallback: system python3 venv
    VENV_DIR="$BUILD_DIR/venv"
    if [ ! -d "$VENV_DIR" ] && command -v python3 &>/dev/null; then
        log "No conda found — creating Python venv as fallback..."
        python3 -m venv "$VENV_DIR" 2>/dev/null || true
        if [ -f "$VENV_DIR/bin/pip" ]; then
            "$VENV_DIR/bin/pip" install --upgrade pip 2>/dev/null || true
            [ -f "$PROJECT_ROOT/requirements.txt" ] && \
                "$VENV_DIR/bin/pip" install -r "$PROJECT_ROOT/requirements.txt" 2>/dev/null || true
        fi
        success "Python venv created at $VENV_DIR"
    fi
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
