#! /bin/bash
DOC_STRING="Build OSM2ODR."

USAGE_STRING=$(cat <<- END
Usage: $0 [-h|--help]

commands

    [--clean]    Clean intermediate files.
    [--rebuild]  Clean and rebuild both configurations.
END
)

REMOVE_INTERMEDIATE=false
BUILD_OSM2ODR=false
GIT_PULL=true
CURRENT_OSM2ODR_COMMIT=1da3c07e39f3e2e0d97f8f709a7255a0df8c6200
OSM2ODR_BRANCH=aaron/defaultsidewalkwidth
OSM2ODR_REPO=https://github.com/carla-simulator/sumo.git

OPTS=`getopt -o h --long help,rebuild,build,clean,carsim,no-pull,chrono,chrono-path: -n 'parse-options' -- "$@"`

eval set -- "$OPTS"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild )
      REMOVE_INTERMEDIATE=true;
      BUILD_OSM2ODR=true;
      shift ;;
    --build )
      BUILD_OSM2ODR=true;
      shift ;;
    --no-pull )
      GIT_PULL=false
      shift ;;
    --clean )
      REMOVE_INTERMEDIATE=true;
      shift ;;
    --chrono )
      shift ;;
    --chrono-path )
      shift 2 ;;
    -h | --help )
      echo "$DOC_STRING"
      echo "$USAGE_STRING"
      exit 1
      ;;
    * )
      shift ;;
  esac
done

source $(dirname "$0")/Environment.sh

function get_source_code_checksum {
  local EXCLUDE='*__pycache__*'
  find "${OSM2ODR_SOURCE_FOLDER}"/* \! -path "${EXCLUDE}" -print0 | sha1sum | awk '{print $1}'
}

if ! { ${REMOVE_INTERMEDIATE} || ${BUILD_OSM2ODR}; }; then
  fatal_error "Nothing selected to be done."
fi

# ==============================================================================
# -- Clean intermediate files --------------------------------------------------
# ==============================================================================

if ${REMOVE_INTERMEDIATE} ; then

  log "Cleaning intermediate files and folders."

  rm -Rf ${OSM2ODR_BUILD_FOLDER}*

fi

# ==============================================================================
# -- Build library -------------------------------------------------------------
# ==============================================================================

if ${BUILD_OSM2ODR} ; then
  log "Building OSM2ODR."
  if [ ! -d ${OSM2ODR_SOURCE_FOLDER} ] ; then
    cd ${CARLA_BUILD_FOLDER}
    OSM2ODR_PACKAGE=osm2odr-source.zip
    # dependencies_u 里的包（内层目录 osm2odr-source/）
    OSM2ODR_SHA256SUM=04f8924bb1c48e8cef0b41b2b9f6d02cc878ec29c49c3a8b70150e0c804b76d5
    # GitHub 归档包（内层目录 sumo-<commit>/），源码树相同但压缩字节不同，校验和也不同
    OSM2ODR_GITHUB_SHA256SUM=7347d0158dd6cecf2f3e443b69b1479b396df70b971b3cde5770539f7c9e9f53

    # 优先使用 dependencies_u(工蜂)仓库里的本地包,避免依赖外网下载
    if [ -f "dependencies/src/${OSM2ODR_PACKAGE}" ] ; then
      log "Using osm2odr from dependencies_u repository."
      cp "dependencies/src/${OSM2ODR_PACKAGE}" .
    fi

    # 本地包缺失或校验不通过时,从 GitHub 下载
    if [ ! -f "${OSM2ODR_PACKAGE}" ] || [[ $(sha256sum "${OSM2ODR_PACKAGE}" | cut -d " " -f 1 ) != "${OSM2ODR_SHA256SUM}" ]] ; then
      start=$(date +%s)
      curl -fL --connect-timeout 30 --speed-limit 1024 --speed-time 60 \
           -o "${OSM2ODR_PACKAGE}" "https://github.com/carla-simulator/sumo/archive/${CURRENT_OSM2ODR_COMMIT}.zip" || true
      end=$(date +%s)
      echo "Elapsed Time downloading from GitHub: $(($end-$start)) seconds"
      # 下到的是 GitHub 的包，改用它的校验和
      OSM2ODR_SHA256SUM=${OSM2ODR_GITHUB_SHA256SUM}
    fi

    # 仍失败则报错
    if [ ! -f "${OSM2ODR_PACKAGE}" ] || [[ $(sha256sum "${OSM2ODR_PACKAGE}" | cut -d " " -f 1 ) != "${OSM2ODR_SHA256SUM}" ]] ; then
      fatal_error "Failed to retrieve osm2odr ${CURRENT_OSM2ODR_COMMIT} from dependencies_u and GitHub."
    fi

    unzip -qq "${OSM2ODR_PACKAGE}"
    rm -f "${OSM2ODR_PACKAGE}"
    # dependencies_u 包内是 osm2odr-source/，GitHub 包内是 sumo-<commit>/
    if [ -d osm2odr-source ] ; then
      mv osm2odr-source ${OSM2ODR_SOURCE_FOLDER}
    else
      mv sumo-${CURRENT_OSM2ODR_COMMIT} ${OSM2ODR_SOURCE_FOLDER}
    fi
  fi

  mkdir -p ${OSM2ODR_BUILD_FOLDER}
  cd ${OSM2ODR_BUILD_FOLDER}


  # export CC="$UE4_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang"
  # export CXX="$UE4_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang++"
  # export PATH="$UE4_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin:$PATH"
  
  export CC=/usr/bin/clang
  export CXX=/usr/bin/clang++
  export PATH=/usr/bin:/bin:$PATH

  cmake ${OSM2ODR_SOURCE_FOLDER} \
      -G "Eclipse CDT4 - Ninja" \
      -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DCMAKE_CXX_FLAGS="-stdlib=libstdc++" \
      -DCMAKE_INSTALL_PREFIX=${LIBCARLA_INSTALL_CLIENT_FOLDER} \
      -DPROJ_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/proj-install/include \
      -DPROJ_LIBRARY=${CARLA_BUILD_FOLDER}/proj-install/lib/libproj.a \
      -DXercesC_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install/include \
      -DXercesC_LIBRARY=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install/lib/libxerces-c.a

  ninja osm2odr
  ninja install

  mkdir -p ${OSM2ODR_SERVER_BUILD_FOLDER}
  cd ${OSM2ODR_SERVER_BUILD_FOLDER}

  # LLVM_BASENAME=llvm-8.0
  # LLVM_INCLUDE="$UE4_ROOT/Engine/Source/ThirdParty/Linux/LibCxx/include/c++/v1"
  # LLVM_LIBPATH="$UE4_ROOT/Engine/Source/ThirdParty/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu"

  echo $LLVM_INCLUDE
  echo $LLVM_LIBPATH
  
  unset UNREAL_HOSTED_CFLAGS
  unset LLVM_INCLUDE
  unset LLVM_LIBPATH
  export CXXFLAGS="-std=c++11 -stdlib=libstdc++"
  export LDFLAGS="-stdlib=libstdc++"

  cmake ${OSM2ODR_SOURCE_FOLDER} \
      -G "Eclipse CDT4 - Ninja" \
      -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DCMAKE_CXX_FLAGS="-fPIC -std=c++14 -stdlib=libc++" \
      -DCMAKE_INSTALL_PREFIX=${LIBCARLA_INSTALL_SERVER_FOLDER} \
      -DPROJ_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/proj-install-server/include \
      -DPROJ_LIBRARY=${CARLA_BUILD_FOLDER}/proj-install-server/lib/libproj.a \
      -DXercesC_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install-server/include \
      -DXercesC_LIBRARY=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install-server/lib/libxerces-c.a

  ninja osm2odr
  ninja install

  mkdir -p ${OSM2ODR_SERVER_BUILD_FOLDER}
  cd ${OSM2ODR_SERVER_BUILD_FOLDER}

  # LLVM_BASENAME=llvm-8.0
  # LLVM_INCLUDE="$UE4_ROOT/Engine/Source/ThirdParty/Linux/LibCxx/include/c++/v1"
  # LLVM_LIBPATH="$UE4_ROOT/Engine/Source/ThirdParty/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu"

  unset UNREAL_HOSTED_CFLAGS
  unset LLVM_INCLUDE
  unset LLVM_LIBPATH
  export CXXFLAGS="-std=c++11 -stdlib=libstdc++"
  export LDFLAGS="-stdlib=libstdc++"

  cmake ${OSM2ODR_SOURCE_FOLDER} \
      -G "Eclipse CDT4 - Ninja" \
      -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DCMAKE_CXX_FLAGS="-fPIC -std=c++14 -stdlib=libc++" \
      -DCMAKE_INSTALL_PREFIX=${LIBCARLA_INSTALL_SERVER_FOLDER} \
      -DPROJ_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/proj-install-server/include \
      -DPROJ_LIBRARY=${CARLA_BUILD_FOLDER}/proj-install-server/lib/libproj.a \
      -DXercesC_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install-server/include \
      -DXercesC_LIBRARY=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install-server/lib/libxerces-c.a

  ninja osm2odr
  ninja install

fi

log " OSM2ODR Success!"
