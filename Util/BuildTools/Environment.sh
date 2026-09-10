#! /bin/bash

# Sets the environment for other shell scripts.

set -e

CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"
source $(dirname "$0")/Vars.mk
unset CURDIR

if [ -n "${CARLA_BUILD_NO_COLOR}" ]; then

  function log {
      echo "`basename "$0"`: $1"
  }

  function fatal_error {
    echo -e >&2 "`basename "$0"`: ERROR: $1"
    exit 2
  }

else

  function log {
    echo -e "\033[1;35m`basename "$0"`: $1\033[0m"
  }

  function fatal_error {
    echo -e >&2 "\033[0;31m`basename "$0"`: ERROR: $1\033[0m"
    exit 2
  }

fi

function get_git_repository_version {
  branch=$(git rev-parse --abbrev-ref HEAD)

  if [[ "$branch" == ue4/* ]]; then
    echo "${branch#ue4/}"
  else
    commit=$(git rev-parse --short HEAD)
    git diff-index --quiet HEAD -- || dirty="-dirty"
    echo "${commit}${dirty}"
  fi
}

function copy_if_changed {
  mkdir -p $(dirname $2)
  rsync -cIr --out-format="%n" $1 $2
}

function move_if_changed {
  copy_if_changed $1 $2
  rm -f $1
}

CARLA_BUILD_CONCURRENCY=`nproc --all`

# ==============================================================================
# -- Conda env python------------
# ==============================================================================
# 在 Linux 上，使用项目中 conda 环境的 Python 解释器

MINICONDA_DIR="${CARLA_BUILD_FOLDER}/dependencies/prerequisites/miniconda3"

function get_conda_env_python {
  local PY_VERSION="$1"
  local ENV_MINOR

  if [[ "${PY_VERSION}" == "3" ]]; then
    # 默认是3.8
    ENV_MINOR="8"
  else
    ENV_MINOR="${PY_VERSION#3.}"
  fi

  local ENV_PY="${MINICONDA_DIR}/envs/hutb_3.${ENV_MINOR}/bin/python"
  if [[ ! -x "${ENV_PY}" ]]; then
    # 环境缺失时自动创建（官方默认源），供 Setup.sh / BuildPythonAPI.sh 复用。
    # 这样新工作区上 make setup（默认 python=3 → hutb_3.8）也能自给自足。
    if [[ -x "${MINICONDA_DIR}/bin/conda" ]]; then
      log "conda env 'hutb_3.${ENV_MINOR}' not found — creating it with python=3.${ENV_MINOR} ..."
      # fix: CondaToSNonInteractiveError — 官方源需先接受 ToS
      "${MINICONDA_DIR}/bin/conda" tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main 2>/dev/null || true
      "${MINICONDA_DIR}/bin/conda" tos accept --override-channels --channel https://repo.anaconda.com/pkgs/r 2>/dev/null || true
      "${MINICONDA_DIR}/bin/conda" create -n "hutb_3.${ENV_MINOR}" python="3.${ENV_MINOR}" --yes || \
        fatal_error "Failed to create conda env 'hutb_3.${ENV_MINOR}'. Run ./setup.sh first, or create it manually with:
    ${MINICONDA_DIR}/bin/conda create -n hutb_3.${ENV_MINOR} python=3.${ENV_MINOR} --yes"
    else
      fatal_error "conda env 'hutb_3.${ENV_MINOR}' not found (${ENV_PY}) and conda not available (${MINICONDA_DIR}/bin/conda).
    Run ./setup.sh first, or create it manually with:
    ${MINICONDA_DIR}/bin/conda create -n hutb_3.${ENV_MINOR} python=3.${ENV_MINOR} --yes"
    fi
  fi
  echo "${ENV_PY}"
}
