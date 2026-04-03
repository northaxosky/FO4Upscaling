#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Load user config
if [ -f "$SCRIPT_DIR/.env" ]; then
    source "$SCRIPT_DIR/.env"
else
    echo "ERROR: scripts/.env not found. Copy scripts/.env.example to scripts/.env and configure paths."
    exit 1
fi

BUILD_DIR="$PROJECT_ROOT/build"
PACKAGE_DIR="$PROJECT_ROOT/package"

FG_BUILD="$BUILD_DIR/plugins/FrameGeneration/Release"
US_BUILD="$BUILD_DIR/plugins/Upscaling/Release"

check_game_running() {
    if tasklist.exe 2>/dev/null | grep -qi "Fallout4"; then
        echo "ERROR: Fallout4.exe is running. DLLs are locked."
        if [[ "$1" != "--force" ]]; then
            exit 1
        fi
        echo "WARNING: --force specified, attempting copy anyway..."
    fi
}

build() {
    echo "=== Building ==="
    export VCPKG_ROOT
    cd "$PROJECT_ROOT"
    cmake --build build --config Release 2>&1
    echo "=== Build complete ==="
}

deploy() {
    echo "=== Deploying to mod folder ==="
    local DEST="$MOD_DIR/F4SE/Plugins"

    # Frame Generation
    cp "$FG_BUILD/AAAFrameGeneration.dll" "$DEST/AAAFrameGeneration.dll"
    [ -f "$FG_BUILD/AAAFrameGeneration.pdb" ] && cp "$FG_BUILD/AAAFrameGeneration.pdb" "$DEST/AAAFrameGeneration.pdb"
    echo "  AAAFrameGeneration.dll deployed"

    # Upscaling
    cp "$US_BUILD/Upscaling.dll" "$DEST/Upscaling.dll"
    [ -f "$US_BUILD/Upscaling.pdb" ] && cp "$US_BUILD/Upscaling.pdb" "$DEST/Upscaling.pdb"
    echo "  Upscaling.dll deployed"

    # HLSL shaders
    cp "$PACKAGE_DIR/F4SE/Plugins/Upscaling/"*.hlsl "$DEST/Upscaling/" 2>/dev/null && echo "  Upscaling shaders deployed" || true
    cp "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/"*.hlsl "$DEST/FrameGeneration/" 2>/dev/null && echo "  FrameGen shaders deployed" || true

    # Shared Streamline DLLs (used by both frame gen and upscaling)
    if [ -d "$PACKAGE_DIR/F4SE/Plugins/Streamline" ]; then
        mkdir -p "$DEST/Streamline"
        cp "$PACKAGE_DIR/F4SE/Plugins/Streamline/"*.dll "$DEST/Streamline/" 2>/dev/null && echo "  Shared Streamline DLLs deployed" || true
    fi

    # Config files
    cp -u "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration.ini" "$DEST/FrameGeneration.ini" 2>/dev/null || true
    mkdir -p "$MOD_DIR/MCM/Config/Upscaling"
    cp -u "$PACKAGE_DIR/MCM/Config/Upscaling/"* "$MOD_DIR/MCM/Config/Upscaling/" 2>/dev/null || true

    echo "=== Deploy complete ==="
}

ACTION="${1:-deploy}"
FORCE_FLAG="$2"

case "$ACTION" in
    build)
        check_game_running "$FORCE_FLAG"
        build
        deploy
        ;;
    deploy)
        check_game_running "$FORCE_FLAG"
        deploy
        ;;
    *)
        echo "Usage: $0 [build|deploy] [--force]"
        exit 1
        ;;
esac
