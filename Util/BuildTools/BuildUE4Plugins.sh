#! /bin/bash
DOC_STRING="Download StreetMapUE4 Plugin."

USAGE_STRING=$(cat <<- END
Usage: $0 [-h|--help]

commands

    [--clean]    Clean intermediate files.
    [--rebuild]  Clean and rebuild both configurations.
END
)

REMOVE_INTERMEDIATE=false
BUILD_STREETMAP=false
GIT_PULL=true
CURRENT_STREETMAP_COMMIT=260273d6b7c3f28988cda31fd33441de7e272958
STREETMAP_BRANCH=master
STREETMAP_REPO=https://github.com/carla-simulator/StreetMap.git

# build air plugin
BUILD_AIR=true
GIT_PULL=true
AIR_BRANCH=main
#AIR_REPO=https://github.com/OpenHUTB/air.git
AIR_REPO=https://github.com/jiandaoshou-aidehua/air.git

ROOT_PATH="/home/ubuntu/hutb"
CARLA_PLUGINS_PATH="$ROOT_PATH/Unreal/CarlaUE4/Plugins"
CARLA_STREETMAP_PLUGINS_PATH="$CARLA_PLUGINS_PATH/StreetMap"

AIR_PLUGIN_PATH="$CARLA_PLUGINS_PATH/AirSim"
AIR_BUILD_PATH="$ROOT_PATH/Build/AirSim"

OPTS=`getopt -o h --long build,rebuild,clean,chrono,chrono-path: -n 'parse-options' -- "$@"`

eval set -- "$OPTS"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild )
      REMOVE_INTERMEDIATE=true;
      BUILD_STREETMAP=true;
      shift ;;
    --build )
      BUILD_STREETMAP=true;
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

if ! { ${REMOVE_INTERMEDIATE} || ${BUILD_STREETMAP}; }; then
 BUILD_SREETMAP=true 
 # fatal_error "Nothing selected to be done."
fi

# ==============================================================================
# -- Clean intermediate files --------------------------------------------------
# ==============================================================================

if ${REMOVE_INTERMEDIATE} ; then

  log "Cleaning intermediate files and folders."

  UE4_INTERMEDIATE_FOLDERS="Binaries Build Intermediate DerivedDataCache"

  pushd "${CARLAUE4_STREETMAP_FOLDER}" >/dev/null

  rm -Rf ${UE4_INTERMEDIATE_FOLDERS}

  popd >/dev/null

fi

# ==============================================================================
# -- Build library -------------------------------------------------------------
# ==============================================================================

if ${BUILD_STREETMAP} ; then
  log "Downloading STREETMAP plugin."
  if ${GIT_PULL} ; then
    if [ ! -d ${CARLAUE4_STREETMAP_FOLDER} ] ; then
      git clone -b ${STREETMAP_BRANCH} ${STREETMAP_REPO} ${CARLAUE4_STREETMAP_FOLDER}
    fi
    cd ${CARLAUE4_STREETMAP_FOLDER}
    git fetch
    git checkout ${CURRENT_STREETMAP_COMMIT}
  fi
fi

# ==============================================================================
# -- Build airsim -------------------------------------------------------------
# ==============================================================================
echo "Build_Air:$BUILD_AIR"
echo "CACHE_DIR:$CACHE_DIR"
echo "Build_Air:$BUILD_AIR"



if [ "$BUILD_AIR" = "true" ]; then
    if [ -d "$AIR_BUILD_PATH/Unreal/Plugins/AirSim" ]; then
        cd "$AIR_BUILD_PATH"
        git fetch --all
        git reset --hard origin/$AIR_BRANCH
        git pull
	echo "0000"
    else 
        echo "Air cache directory: $CACHE_DIR/AirSim"
        if [ -d "$CACHE_DIR/AirSim" ]; then
           cp -a "$CACHE_DIR/AirSim" "$AIR_BUILD_PATH"
	   echo "1111"
        else 
	   echo "2222"
           git clone -b "$AIR_BRANCH" "$AIR_REPO" "$AIR_BUILD_PATH"
	fi
    fi
    cd "$AIR_BUILD_PATH"
    echo "Current dir: $(pwd)"
    # Build AirSim
    # read -p "press enter to continue ..."
    ./setup.sh 
    ./build.sh 
    cp -a \
       "$AIR_BUILD_PATH/Unreal/Plugins/AirSim" \
       "$AIR_PLUGIN_PATH"
fi




log "StreetMap Success!"
