#! /bin/bash

# ==============================================================================
# -- Parse arguments -----------------------------------------------------------
# ==============================================================================

DOC_STRING="Build and package CARLA Python API."

USAGE_STRING="Usage: $0 [-h|--help] [--rebuild] [--clean] [--python-version=VERSION] [--target-wheel-platform=PLATFORM]"

REMOVE_INTERMEDIATE=false
BUILD_RSS_VARIANT=false
BUILD_PYTHONAPI=true
INSTALL_PYTHONAPI=true

OPTS=`getopt -o h --long help,config:,rebuild,clean,rss,carsim,python-version:,build-wheel,target-wheel-platform:,packages:,clean-intermediate,all,xml,target-archive:, -n 'parse-options' -- "$@"`

eval set -- "$OPTS"

# 与 Windows BuildPythonAPI.bat 对齐：默认构建 hutb_3.14 ~ hutb_3.7 全部版本
PY_VERSION_LIST="3.14,3.13,3.12,3.11,3.10,3.9,3.8,3.7"
TARGET_WHEEL_PLATFORM=

while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild )
      REMOVE_INTERMEDIATE=true;
      BUILD_PYTHONAPI=true;
      INSTALL_PYTHONAPI=true;
      shift ;;
    --python-version )
      PY_VERSION_LIST="$2"
      shift 2 ;;
    --build-wheel )
      BUILD_PYTHONAPI=true;
      INSTALL_PYTHONAPI=false;
      shift ;;
    --target-wheel-platform )
      TARGET_WHEEL_PLATFORM="$2"
      shift 2 ;;
    --rss )
      BUILD_RSS_VARIANT=true;
      shift ;;
    --clean )
      REMOVE_INTERMEDIATE=true;
      BUILD_PYTHONAPI=false;
      INSTALL_PYTHONAPI=false;
      shift ;;
    -h | --help )
      echo "$DOC_STRING"
      echo "$USAGE_STRING"
      exit 1
      ;;
    * )
      shift ;;
  esac
done


export CC="$UE4_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang"
export CXX="$UE4_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang++"
export PATH="$UE4_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin:$PATH"

source $(dirname "$0")/Environment.sh

if ! { ${REMOVE_INTERMEDIATE} || ${BUILD_PYTHONAPI} || ${BUILD_PYTHONAPI_WHEEL} ; }; then
  fatal_error "Nothing selected to be done."
fi

# Convert comma-separated string to array of unique elements.
IFS="," read -r -a PY_VERSION_LIST <<< "${PY_VERSION_LIST}"

pushd "${CARLA_PYTHONAPI_SOURCE_FOLDER}" >/dev/null

# ==============================================================================
# -- Clean intermediate files --------------------------------------------------
# ==============================================================================

if ${REMOVE_INTERMEDIATE} ; then

  log "Cleaning intermediate files and folders."

  rm -Rf build dist source/carla.egg-info

  find source -name "*.so" -delete
  find source -name "__pycache__" -type d -exec rm -rf "{}" \;

fi

# ==============================================================================
# -- Build API -----------------------------------------------------------------
# ==============================================================================

if ${BUILD_RSS_VARIANT} ; then
  export BUILD_RSS_VARIANT=${BUILD_RSS_VARIANT}
fi


if ${BUILD_PYTHONAPI} ; then
  # Add patchelf to the path. Auditwheel relies on patchelf to repair ELF files.
  export PATH="${LIBCARLA_INSTALL_CLIENT_FOLDER}/bin:${PATH}"

  #检查 conda 是否已安装（目前仅提示）
  if command -v conda >/dev/null 2>&1 || [[ -x "${MINICONDA_DIR}/bin/conda" ]] ; then
    echo "Conda is already installed."
  else
    echo "TODO: Installing miniconda with silent mode"
  fi

  # boost 编译产物位于 ${CARLA_BUILD_FOLDER} 下，路径与 Setup.sh 中的
  # BOOST_VERSION / CXX_TAG 保持一致。
  BOOST_VERSION=1.90.0
  CXX_TAG=c10
  BOOST_BASENAME="boost-${BOOST_VERSION}-${CXX_TAG}"
  BOOST_INSTALL_FOLDER="${CARLA_BUILD_FOLDER}/${BOOST_BASENAME}-install"
  BOOST_SOURCE_FOLDER="${CARLA_BUILD_FOLDER}/${BOOST_BASENAME}-source"

  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
    log "Building Python API wheel for Python ${PY_VERSION}."

    # 使用项目中的 conda 环境 Python（hutb_3.X）而非系统 Python。
    CONDA_PY=$(get_conda_env_python ${PY_VERSION})
    export PATH="$(dirname ${CONDA_PY}):${PATH}"

    # 确保轮子工具在环境中可用（幂等）
    # -m pip = 用"这个解释器"运行它自己环境里的 pip 模块。
    ${CONDA_PY} -m pip install --quiet setuptools wheel build 2>/dev/null || true

    echo "Current Python path:"
    command -v python

    # 删除上一个 Python 版本编译的 boost
    # 迫使 setup 为本 Python 版本重新编译 boost（libboost_python 与 Python 版本绑定）
    echo "BOOST_VERSION: ${BOOST_VERSION}"
    echo "BOOST_INSTALL_FOLDER: ${BOOST_INSTALL_FOLDER}"
    if [[ -d "${BOOST_INSTALL_FOLDER}" ]] ; then
      echo "Delete all boost files: ${BOOST_INSTALL_FOLDER}"
      rm -Rf "${BOOST_INSTALL_FOLDER}"
      echo "Delete boost source code: ${BOOST_SOURCE_FOLDER}"
      rm -Rf "${BOOST_SOURCE_FOLDER}"
    fi

    # 先编译 LibCarla 和 osm2odr
    if ${BUILD_RSS_VARIANT} ; then
      # -C 是 make 的"先切换目录再执行"选项
      make -C ${CARLA_ROOT_FOLDER} LibCarla.client.rss ARGS="--python-version=${PY_VERSION}"
    else
      make -C ${CARLA_ROOT_FOLDER} LibCarla ARGS="--python-version=${PY_VERSION}"
    fi
    make -C ${CARLA_ROOT_FOLDER} osm2odr

    # Building the RSS variant adds files to SOURCES.txt we do not want included in a normal build
    rm -Rf source/carla.egg-info 
    ${CONDA_PY} -m build --wheel --outdir dist/.tmp .

    if ${INSTALL_PYTHONAPI} ; then
      log "Installing Python API for Python ${PY_VERSION}."
      ${CONDA_PY} -m pip install --force-reinstall dist/.tmp/$(ls dist/.tmp | grep .whl)
    fi

    if [[ -z ${TARGET_WHEEL_PLATFORM} ]] ; then
      cp dist/.tmp/$(ls dist/.tmp | grep .whl) dist
    else
      log "Tagging Python API wheel to ${TARGET_WHEEL_PLATFORM} for Python ${PY_VERSION}."
      ${CONDA_PY} -m wheel tags --platform-tag ${TARGET_WHEEL_PLATFORM} dist/.tmp/$(ls dist/.tmp | grep .whl)
      ${CONDA_PY} -m auditwheel repair --plat ${TARGET_WHEEL_PLATFORM} --wheel-dir dist dist/.tmp/$(ls dist/.tmp | grep ${TARGET_WHEEL_PLATFORM}.whl)
    fi
    rm -rf dist/.tmp

    # 构建失败（dist 目录不存在）时报错
    if [[ ! -d dist ]] ; then
      fatal_error "An error occurred while building the wheel file."
    fi
  done
fi

# ==============================================================================
# -- ...and we are done --------------------------------------------------------
# ==============================================================================

popd >/dev/null

log "Success!"
