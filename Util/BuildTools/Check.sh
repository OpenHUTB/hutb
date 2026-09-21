#! /bin/bash

# ==============================================================================
# -- Parse arguments -----------------------------------------------------------
# ==============================================================================

DOC_STRING="Run unit tests."

USAGE_STRING=$(cat <<- END
Usage: $0 [-h|--help] [--gdb] [--xml] [--gtest_args=ARGS] [--python-version=VERSION]

Then either ran all the tests

    [--all]

Or choose one or more of the following

    [--libcarla-release] [--libcarla-debug] [--smoke] [--air] [--vr] [--upload]
    [--python-api] [--benchmark] [--debug]

You can also set the command-line arguments passed to GTest on a ".gtest"
config file in the Carla project main folder. E.g.

    # Contents of ${CARLA_ROOT_FOLDER}/.gtest
    gtest_shuffle
    gtest_filter=misc*
END
)

IS_DEBUG=false
GDB=
XML_OUTPUT=false
GTEST_ARGS=
LIBCARLA_RELEASE=false
LIBCARLA_DEBUG=false
SMOKE_TESTS=false
VR_TESTS=false
PYTHON_API=false
RUN_BENCHMARK=false
AIR_TESTS=false
MEASURE_TIME=true
UPLOAD_DOWNLOAD=false

OPTS=`getopt -o h --long help,gdb,xml,gtest_args:,all,libcarla-release,libcarla-debug,python-api,smoke,air,vr,upload,debug,benchmark,python-version:, -n 'parse-options' -- "$@"`

eval set -- "$OPTS"

source $(dirname "$0")/Environment.sh

if [ -f "${CARLA_ROOT_FOLDER}/.gtest" ]; then
  GTEST_ARGS=`sed -e 's/#.*$//g' ${CARLA_ROOT_FOLDER}/.gtest | sed -e '/^[[:space:]]*$/!s/^/--/g' | sed -e ':a;N;$!ba;s/\n/ /g'`
fi

# 与 Windows Check.bat 对齐：默认测试 hutb_3.14 ~ hutb_3.7 全部版本
PY_VERSION_LIST=3.14,3.13,3.12,3.11,3.10,3.9,3.8,3.7

while [[ $# -gt 0 ]]; do
  case "$1" in
    --gdb )
      GDB="gdb --args";
      shift ;;
    --xml )
      XML_OUTPUT=true;
      # Create the folder for the test-results
      mkdir -p "${CARLA_TEST_RESULTS_FOLDER}"
      shift ;;
    --gtest_args )
      GTEST_ARGS="$2";
      shift 2 ;;
    --all )
      # 与 Check.bat 对齐：完整端到端流程
      # (上传/下载发行包 → 启动 CarlaUE4 → 装测试依赖与包内 whl → 单测 + smoke)
      # SMOKE_TESTS=true;
      LIBCARLA_RELEASE=true;
      LIBCARLA_DEBUG=true;
      PYTHON_API=true;
      # UPLOAD_DOWNLOAD=true;
      shift ;;
    --libcarla-release )
      LIBCARLA_RELEASE=true;
      shift ;;
    --libcarla-debug )
      LIBCARLA_DEBUG=true;
      shift ;;
    --smoke )
      SMOKE_TESTS=true;
      shift ;;
    --air )
      AIR_TESTS=true;
      shift ;;
    --vr )
      VR_TESTS=true;
      shift ;;
    --upload )
      UPLOAD_DOWNLOAD=true;
      shift ;;
    --debug )
      IS_DEBUG=true;
      shift ;;
    --python-api )
      PYTHON_API=true;
      shift ;;
    --benchmark )
      LIBCARLA_RELEASE=true;
      RUN_BENCHMARK=true;
      GTEST_ARGS="--gtest_filter=benchmark*";
      shift ;;
    --python-version )
      PY_VERSION_LIST="$2"
      shift 2 ;;
    -h | --help )
      echo "$DOC_STRING"
      echo -e "$USAGE_STRING"
      exit 1
      ;;
    * )
      shift ;;
  esac
done

# 优先使用环境中的Python系统，以便构建的wheel文件与Boost库所匹配的Python版本一致。
export PATH=/usr/bin:/bin:$PATH

if ! { ${LIBCARLA_RELEASE} || ${LIBCARLA_DEBUG} || ${PYTHON_API} || ${SMOKE_TESTS} || ${AIR_TESTS} || ${VR_TESTS}; }; then
  fatal_error "Nothing selected to be done."
fi

# 将逗号分隔的字符串转换为元素的数组
# IFS="," —— 临时给本条命令改分隔符
# read —— 读一行，按 IFS 切字段
# -r —— 不让 read 处理反斜杠转义
# IFS="," read    -a a <<< "$s"   # 无 -r → a=(a,b)，\ 被当转义吃掉，逗号没切开
# IFS="," read -r -a a <<< "$s"   # 有 -r → a=(a\ b)，原样保留
# -a 表示赋给索引数组（下标 0 开始）而不是普通变量。
IFS="," read -r -a PY_VERSION_LIST <<< "${PY_VERSION_LIST}"

# 衡量测试的总时间
T_START_OVERALL=$(date +%s)

# ==============================================================================
# -- Download Content need it by the tests -------------------------------------
# ==============================================================================

if { ${LIBCARLA_RELEASE} || ${LIBCARLA_DEBUG}; }; then

  CONTENT_TAG=0.1.4

  mkdir -p ${LIBCARLA_TEST_CONTENT_FOLDER}
  pushd "${LIBCARLA_TEST_CONTENT_FOLDER}" >/dev/null

  if [ "$(get_git_repository_version)" != "${CONTENT_TAG}" ]; then
    pushd .. >/dev/null
    rm -Rf ${LIBCARLA_TEST_CONTENT_FOLDER}
    git clone -b ${CONTENT_TAG} https://github.com/carla-simulator/opendrive-test-files.git ${LIBCARLA_TEST_CONTENT_FOLDER}
    popd >/dev/null
  fi

  popd >/dev/null

fi

# ==============================================================================
# -- Run LibCarla tests --------------------------------------------------------
# ==============================================================================

if ${LIBCARLA_DEBUG} ; then

  if ${XML_OUTPUT} ; then
    EXTRA_ARGS="--gtest_output=xml:${CARLA_TEST_RESULTS_FOLDER}/libcarla-debug.xml"
  else
    EXTRA_ARGS=
  fi

  log "Running LibCarla.server unit tests (debug)."
  echo "Running: ${GDB} libcarla_test_server_debug ${GTEST_ARGS} ${EXTRA_ARGS}"
  LD_LIBRARY_PATH=${LIBCARLA_INSTALL_SERVER_FOLDER}/lib ${GDB} ${LIBCARLA_INSTALL_SERVER_FOLDER}/test/libcarla_test_server_debug ${GTEST_ARGS} ${EXTRA_ARGS}

  log "Running LibCarla.client unit tests (debug)."
  echo "Running: ${GDB} libcarla_test_client_debug ${GTEST_ARGS} ${EXTRA_ARGS}"
  ${GDB} ${LIBCARLA_INSTALL_CLIENT_FOLDER}/test/libcarla_test_client_debug ${GTEST_ARGS} ${EXTRA_ARGS}

fi

if ${LIBCARLA_RELEASE} ; then

  if ${XML_OUTPUT} ; then
    EXTRA_ARGS="--gtest_output=xml:${CARLA_TEST_RESULTS_FOLDER}/libcarla-release.xml"
  else
    EXTRA_ARGS=
  fi

  log "Running LibCarla.server unit tests (release)."
  echo "Running: ${GDB} libcarla_test_server_release ${GTEST_ARGS} ${EXTRA_ARGS}"
  LD_LIBRARY_PATH=${LIBCARLA_INSTALL_SERVER_FOLDER}/lib ${GDB} ${LIBCARLA_INSTALL_SERVER_FOLDER}/test/libcarla_test_server_release ${GTEST_ARGS} ${EXTRA_ARGS}

  if ! { ${RUN_BENCHMARK} ; }; then

    log "Running LibCarla.client unit tests (release)."
    echo "Running: ${GDB} libcarla_test_client_release ${GTEST_ARGS} ${EXTRA_ARGS}"
    ${GDB} ${LIBCARLA_INSTALL_CLIENT_FOLDER}/test/libcarla_test_client_release ${GTEST_ARGS} ${EXTRA_ARGS}

  fi

fi

# ==============================================================================
# -- Upload / download distribution package ------------------------------------
# ==============================================================================

# 发行包版本目录名：git 短哈希（工作区有改动时带 -dirty 后缀，与打包脚本一致）
CARLA_VERSION=$(get_git_repository_version)

# 发行包安装目录。
INSTALLATION_DIR=${INSTALLATION_DIR:-${CARLA_BUILD_FOLDER}}

# if ${UPLOAD_DOWNLOAD} ; then

#   # 下载器脚本（Windows 的 hutb_downloader.exe 即它的 pyinstaller 打包版）
#   DOWNLOADER="${CARLA_UTIL_FOLDER}/download_from_git.py"
#   DOWNLOADER_PY=$(get_conda_env_python 3.8)

#   # download_from_git.py 依赖 gitpython
#   ${DOWNLOADER_PY} -c "import git" 2>/dev/null || ${DOWNLOADER_PY} -m pip install -q gitpython

#   # 上传分支固定查找 cwd/../Build/UE4Carla 下修改时间最新的 .zip；
#   # 先在 Dist 或 Build/UE4Carla 里找最新的 tar.gz，缺的话从 Dist 拷过去。
#   mkdir -p "${INSTALLATION_DIR}/UE4Carla"
#   LATEST_TAR=$(find "${CARLA_DIST_FOLDER}" "${INSTALLATION_DIR}/UE4Carla" -maxdepth 1 -name "*.tar.gz" -printf "%T@ %p\n" 2>/dev/null | sort -n | tail -1 | cut -d" " -f2- || true)
#   if [[ -z "${LATEST_TAR}" ]] ; then
#     fatal_error "No tar.gz file found in ${CARLA_DIST_FOLDER} or ${INSTALLATION_DIR}/UE4Carla to upload."
#   fi
#   if [[ "$(dirname ${LATEST_TAR})" != "${INSTALLATION_DIR}/UE4Carla" ]] ; then
#     echo "Copying latest tar.gz to upload folder: ${LATEST_TAR}"
#     cp "${LATEST_TAR}" "${INSTALLATION_DIR}/UE4Carla/"
#   fi

#   pushd "${CARLA_UTIL_FOLDER}" >/dev/null
#     log "Uploading distribution package..."
#     ${DOWNLOADER_PY} -u release
#   popd >/dev/null

#   # 下载回来并解压（解压到 Util/dist/hutb/），顺带测试下载分发流程本身
#   mkdir -p "${CARLA_UTIL_FOLDER}/dist"
#   pushd "${CARLA_UTIL_FOLDER}/dist" >/dev/null
#     log "Downloading distribution package..."
#     ${DOWNLOADER_PY}
#   popd >/dev/null

# fi

# ==============================================================================
# -- Locate CarlaUE4 packaged build --------------------------------------------
# ==============================================================================

# 打包产物可能在多个位置：
# 1) Windows 式布局  Build/UE4Carla/<version>/（Package.bat 的直接输出目录）
# 2) 下载解压目录    Util/dist/hutb/UE4Carla/<version>/
# 3) Package.sh 布局 Dist/CARLA_<version>/ 与 Dist/CARLA_<config>_<version>/
#    （默认 config=Shipping，如 Dist/CARLA_Shipping_<version>，tar.gz 解压后）
if ${IS_DEBUG} ; then
  PACKAGE_ROOTS=("${INSTALLATION_DIR}/UE4Carla/debug")
else
  PACKAGE_ROOTS=(
    "${INSTALLATION_DIR}/UE4Carla/${CARLA_VERSION}"
    "${CARLA_UTIL_FOLDER}/dist/hutb/UE4Carla/${CARLA_VERSION}"
    "${CARLA_DIST_FOLDER}/CARLA_${CARLA_VERSION}"
    "${CARLA_DIST_FOLDER}"/CARLA_*_"${CARLA_VERSION}"
  )
fi

EXE_DIR=
PACKAGE_ROOT=
for ROOT in "${PACKAGE_ROOTS[@]}" ; do
  # 通配符未展开时跳过
  if [[ -f "${ROOT}/LinuxNoEditor/CarlaUE4.sh" ]] ; then
    PACKAGE_ROOT="${ROOT}"
    EXE_DIR="${ROOT}/LinuxNoEditor"
    break
  fi
done

if [[ -n "${EXE_DIR}" ]] ; then
  log "Using packaged build: ${EXE_DIR}"
else
  log "warning: packaged build not found — will skip packaged wheel install; simulator tests (smoke/air/vr) need it."
fi

# ==============================================================================
# -- Install Python packages ---------------------------------------------------
# ==============================================================================

# 与 Check.bat:190-205 的 Install Python packages 段对齐：
# 测试依赖（含 nose2）由 check 自己装、每个版本只在这里装一次，
# 构建侧不负责（BuildPythonAPI 重建 conda 环境后靠这里补全）。
# 非 debug 时额外卸载 hutb 并安装“发行包”里的 whl（测试打包产物本身）。
if ! ${IS_DEBUG} ; then

  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
    CONDA_PY=$(get_conda_env_python ${PY_VERSION})
    MINOR="${PY_VERSION#3.}"

    log "Installing Python test requirements for Python ${PY_VERSION}."
    ${CONDA_PY} -m pip install -r "${CARLA_PYTHONAPI_ROOT_FOLDER}/test/requirements.txt" \
        -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com \
        || log "warning: failed to install test requirements for Python ${PY_VERSION}"

    # 包内 whl 命名含版本/平台标签。与 Check.bat 对齐：
    # Python 3.7 的 whl 带 cp37m 标签（cp3X-cp3Xm），其余版本为 cp3X-cp3X；
    # 版本号用通配符匹配（Windows 用 API_VERSION，Linux 侧无此变量）。
    if [[ "${PY_VERSION}" == "3.7" ]] ; then
      WHL=$(ls "${EXE_DIR}"/PythonAPI/carla/dist/hutb-*-cp3${MINOR}-cp3${MINOR}m-linux_x86_64.whl 2>/dev/null | head -1 || true)
    else
      WHL=$(ls "${EXE_DIR}"/PythonAPI/carla/dist/hutb-*-cp3${MINOR}-cp3${MINOR}-linux_x86_64.whl 2>/dev/null | head -1 || true)
    fi
    if [[ -n "${WHL}" ]] ; then
      log "Installing packaged wheel for Python ${PY_VERSION}: $(basename ${WHL})"
      # wheel 版本号不随构建变化（如 2.10.1），已装同版本时 pip install 会直接
      # 跳过，包内 whl 就没被真正测试到；先卸载保证每次都从发行包重新安装。
      ${CONDA_PY} -m pip uninstall --yes hutb || true
      ${CONDA_PY} -m pip install "${WHL}"
    else
      log "warning: no packaged wheel found for Python ${PY_VERSION}, using locally installed one."
    fi
  done

fi

# ==============================================================================
# -- Launch CarlaUE4 service for tests -----------------------------------------
# ==============================================================================

if { ${SMOKE_TESTS} || ${AIR_TESTS} || ${VR_TESTS}; }; then

  if [[ -z "${EXE_DIR}" ]] ; then
    fatal_error "CarlaUE4.sh not found (looked under Build/UE4Carla/${CARLA_VERSION}, Util/dist/hutb/UE4Carla/${CARLA_VERSION} and Dist/CARLA_${CARLA_VERSION}). Build and package first, or set INSTALLATION_DIR."
  fi

  # 杀掉占用 3654 端口的旧服务进程
  fuser -k 3654/tcp 2>/dev/null || pkill -f "CarlaUE4.sh" 2>/dev/null || true

  # 后台启动服务（对应 bat 的 start；否则会卡住）。
  # 注意：release 版加 -RenderOffscreen，debug 版不加（与 bat 一致）。
  if ! ${IS_DEBUG} ; then
    SIM_ARGS="-RenderOffscreen --carla-rpc-port=3654 --carla-streaming-port=0 -nosound"
  else
    SIM_ARGS="--carla-rpc-port=3654 --carla-streaming-port=0 -nosound"
  fi
  log "Unreal service is launching with command: CarlaUE4.sh ${SIM_ARGS}"
  pushd "${EXE_DIR}" >/dev/null
    nohup ./CarlaUE4.sh ${SIM_ARGS} >/dev/null 2>&1 &
  popd >/dev/null

fi

# ==============================================================================
# -- Run Carla-Air example tests -----------------------------------------------
# ==============================================================================

if ${AIR_TESTS} ; then
  log "Running Carla-Air example tests..."
  pushd "${CARLA_PYTHONAPI_ROOT_FOLDER}/examples/air" >/dev/null
  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
    CONDA_PY=$(get_conda_env_python ${PY_VERSION})
    log "Running Carla-Air Python API for Python ${PY_VERSION} example tests."
    # 与 bat 相同跑 4 个脚本；失败即中止
    for SCRIPT in 01_hello_world.py 02_weather_control.py 03_spawn_traffic.py 04_sensor_capture.py ; do
      ${CONDA_PY} ${SCRIPT} --port 3654 || fatal_error "AIR test failed: ${SCRIPT} (Python ${PY_VERSION})."
    done
    log "Finished Carla-Air Python API for Python ${PY_VERSION} example tests."
  done
  popd >/dev/null
fi

# ==============================================================================
# -- Run Python API unit tests -------------------------------------------------
# ==============================================================================

pushd "${CARLA_PYTHONAPI_ROOT_FOLDER}/test/unit" >/dev/null

if ${XML_OUTPUT} ; then
  EXTRA_ARGS="-X"
else
  EXTRA_ARGS=
fi

if ${PYTHON_API} ; then

  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do

    log "Running Python API for Python ${PY_VERSION} unit tests."

    # carla wheel 安装在 conda 环境 hutb_3.X 中，须用对应环境的 python 运行，
    # 否则系统 python3 找不到 carla 模块（ModuleNotFoundError）。
    # 测试依赖已在上方 Install Python packages 段统一安装（同 Check.bat）。
    # 与 Check.bat 对齐：每个版本只跑 test_transform 和 test_vehicle。
    CONDA_PY=$(get_conda_env_python ${PY_VERSION})
    ${CONDA_PY} -m nose2 ${EXTRA_ARGS} test_transform
    ${CONDA_PY} -m nose2 ${EXTRA_ARGS} test_vehicle

  done

  if ${XML_OUTPUT} ; then
    mv test-results.xml ${CARLA_TEST_RESULTS_FOLDER}/python-api-3.xml
  fi

fi

popd >/dev/null

# ==============================================================================
# -- Run smoke tests -----------------------------------------------------------
# ==============================================================================

T_START_DO_TEST=$(date +%s)

if ${SMOKE_TESTS} ; then
  pushd "${CARLA_PYTHONAPI_ROOT_FOLDER}/util" >/dev/null
    log "Checking connection with the simulator."
    for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
      # 与单元测试一致：用 carla wheel 所在的 conda 环境 python 运行。
      CONDA_PY=$(get_conda_env_python ${PY_VERSION})
      ${CONDA_PY} test_connection.py -p 3654 --timeout=60.0
    done
  popd >/dev/null
fi

pushd "${CARLA_PYTHONAPI_ROOT_FOLDER}/test" >/dev/null

if ${XML_OUTPUT} ; then
  EXTRA_ARGS="-c smoke/unittest.cfg -X"
else
  EXTRA_ARGS=
fi

if ${SMOKE_TESTS} ; then
  smoke_list=`cat smoke_test_list.txt`
  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
    log "Running smoke tests for Python ${PY_VERSION}."
    # 与单元测试一致：用 carla wheel 所在的 conda 环境 python 运行。
    # 测试依赖已在上方 Install Python packages 段统一安装（同 Check.bat）。
    CONDA_PY=$(get_conda_env_python ${PY_VERSION})
    ${CONDA_PY} -m nose2 -v ${EXTRA_ARGS} ${smoke_list}
  done

  if ${XML_OUTPUT} ; then
    mv test-results.xml ${CARLA_TEST_RESULTS_FOLDER}/smoke-tests-3.xml
  fi

fi

popd >/dev/null

T_END_DO_TEST=$(date +%s)
if ${MEASURE_TIME} && ${SMOKE_TESTS} ; then
  echo "$(basename $0) [TIME]: Running smoke test took $((T_END_DO_TEST - T_START_DO_TEST)) seconds."
fi

# ==============================================================================
# -- Run VR tests --------------------------------------------------------------
# ==============================================================================

T_START_DO_TEST=$(date +%s)

if ${VR_TESTS} ; then
  pushd "${CARLA_PYTHONAPI_ROOT_FOLDER}/test" >/dev/null
  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
    CONDA_PY=$(get_conda_env_python ${PY_VERSION})
    log "Running VR tests for Python ${PY_VERSION}."
    # 等服务就绪（对应 bat 的 timeout /t 10）
    sleep 10
    log "Switch to VR mode..."
    ${CONDA_PY} ${CARLA_PYTHONAPI_ROOT_FOLDER}/util/config.py -p 3654 --map "Town10HD?GAME=VR"
    ${CONDA_PY} ./function/test_VR_diagram_mode.py || fatal_error "VR test failed (Python ${PY_VERSION})."
  done
  popd >/dev/null
fi

T_END_DO_TEST=$(date +%s)
if ${MEASURE_TIME} && ${VR_TESTS} ; then
  echo "$(basename $0) [TIME]: Running VR test took $((T_END_DO_TEST - T_START_DO_TEST)) seconds."
fi

# ==============================================================================
# -- Kill CarlaUE4 service after tests -----------------------------------------
# ==============================================================================

if { ${SMOKE_TESTS} || ${AIR_TESTS} || ${VR_TESTS}; }; then
  # 等服务就绪再杀（对应 bat：等几秒让测试任务提交，否则可能没有测试任务被杀掉）
  sleep 10
  log "Killing Unreal service process after test..."
  fuser -k 3654/tcp 2>/dev/null || pkill -f "CarlaUE4.sh" 2>/dev/null || true
fi

T_END_OVERALL=$(date +%s)
if ${MEASURE_TIME} ; then
  echo "$(basename $0) [TIME]: Overall testing took $((T_END_OVERALL - T_START_OVERALL)) seconds."
fi

# ==============================================================================
# -- ...and we are done --------------------------------------------------------
# ==============================================================================

log "Success!"
