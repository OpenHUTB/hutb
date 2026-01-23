set http_proxy=http://127.0.0.1:7890
set https_proxy=http://127.0.0.1:7890

# python.exe
set PATH=/usr/bin/python 

# sys command
set PATH=/usr/bin:$PATH

# make.exe
set PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Carla UE4 converters directory
CARLA_CONVERTER_DIR="$PROJECT_ROOT/Unreal/CarlaUE4/Plugins/Converters"

if [ -d "$CARLA_CONVERTER_DIR" ]; then
  echo "[setEnv64] Fixing execute permissions under Converters..."
  chmod -R +x "$CARLA_CONVERTER_DIR"
fi

UE4_ROOT="/home/ubuntu/UnrealEngine_4.26"
if [ -d "$UE4_ROOT/Engine/Build/BatchFiles" ]; then
  for f in "$UE4_ROOT/Engine/Build/BatchFiles"/*.sh; do
    [ -f "$f" ] && [ ! -x "$f" ] && chmod u+x "$f"
  done
fi

