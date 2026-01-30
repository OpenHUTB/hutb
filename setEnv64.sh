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

# ---------------- Converters ----------------
CARLA_CONVERTER_DIR="$PROJECT_ROOT/Unreal/CarlaUE4/Plugins/Converters"

if [ -d "$CARLA_CONVERTER_DIR" ]; then
  echo "[setEnv64] Fixing execute permissions under Converters..."
  chmod -R u+x "$CARLA_CONVERTER_DIR" \
    || echo "[setEnv64] Warning: chmod Converters failed, continuing"
else
  echo "[setEnv64] Warning: Converters directory not found"
fi

# ---------------- UE4 tools ----------------
UE4_ROOT="/home/ubuntu/UnrealEngine_4.26"
BATCH_DIR="$UE4_ROOT/Engine/Build/BatchFiles"

if [ -d "$BATCH_DIR" ]; then
  echo "[setEnv64] Fixing UE4 BatchFiles permissions..."
  for f in "$BATCH_DIR"/*.sh; do
    [ -f "$f" ] || continue
    chmod u+x "$f" || echo "[setEnv64] Warning: chmod failed: $f"
  done
fi