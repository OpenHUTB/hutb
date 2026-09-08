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
    fatal_error "conda env 'hutb_3.${ENV_MINOR}' not found (${ENV_PY}).
    Run ./setup.sh first, or create it manually with:
    ${MINICONDA_DIR}/bin/conda create -n hutb_3.${ENV_MINOR} python=3.${ENV_MINOR} --yes"
  fi
  echo "${ENV_PY}"
}
