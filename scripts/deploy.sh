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

: "${MOD_DIR:?ERROR: MOD_DIR not set in .env}"
: "${VCPKG_ROOT:?ERROR: VCPKG_ROOT not set in .env}"
[[ -d "$VCPKG_ROOT" ]] || { echo "ERROR: VCPKG_ROOT='$VCPKG_ROOT' does not exist"; exit 1; }

BUILD_DIR="$PROJECT_ROOT/build"
PACKAGE_DIR="$PROJECT_ROOT/package"

FG_BUILD="$BUILD_DIR/plugins/FrameGeneration/Release"
US_BUILD="$BUILD_DIR/plugins/Upscaling/Release"
MV_BUILD="$BUILD_DIR/plugins/MotionVectorFixes/Release"

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
    # Ensure SDK DLLs are present before deploying
    if [ ! -f "$PACKAGE_DIR/F4SE/Plugins/Streamline/sl.interposer.dll" ] || \
       [ ! -f "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/FidelityFX/amd_fidelityfx_dx12.dll" ] || \
       [ ! -f "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/XeSS/libxess_fg.dll" ]; then
        echo "SDK DLLs missing, fetching..."
        bash "$SCRIPT_DIR/fetch-sdks.sh"
    fi

    echo "=== Deploying to mod folder ==="
    local DEST="$MOD_DIR/F4SE/Plugins"

    # Ensure all subdirectories exist
    mkdir -p "$DEST/Upscaling"
    mkdir -p "$DEST/FrameGeneration/FidelityFX"
    mkdir -p "$DEST/FrameGeneration/Streamline"
    mkdir -p "$DEST/Streamline"
    mkdir -p "$MOD_DIR/MCM/Config/Upscaling"
    mkdir -p "$MOD_DIR/MCM/Config/FrameGeneration"

    # Plugin DLLs (build output)
    cp "$FG_BUILD/AAAFrameGeneration.dll" "$DEST/"
    [ -f "$FG_BUILD/AAAFrameGeneration.pdb" ] && cp "$FG_BUILD/AAAFrameGeneration.pdb" "$DEST/"
    echo "  AAAFrameGeneration.dll deployed"

    cp "$US_BUILD/Upscaling.dll" "$DEST/"
    [ -f "$US_BUILD/Upscaling.pdb" ] && cp "$US_BUILD/Upscaling.pdb" "$DEST/"
    echo "  Upscaling.dll deployed"

    cp "$MV_BUILD/MotionVectorFixes.dll" "$DEST/"
    [ -f "$MV_BUILD/MotionVectorFixes.pdb" ] && cp "$MV_BUILD/MotionVectorFixes.pdb" "$DEST/"
    echo "  MotionVectorFixes.dll deployed"

    # HLSL shaders (package)
    cp "$PACKAGE_DIR/F4SE/Plugins/Upscaling/"*.hlsl "$DEST/Upscaling/"
    echo "  Upscaling shaders deployed"

    cp "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/"*.hlsl "$DEST/FrameGeneration/"
    echo "  FrameGen shaders deployed"

    # FidelityFX runtime DLL (package)
    cp "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/FidelityFX/amd_fidelityfx_dx12.dll" "$DEST/FrameGeneration/FidelityFX/"
    echo "  FidelityFX DLL deployed"

    # Streamline DLLs — shared (spatial upscaling: DLSS SR)
    cp "$PACKAGE_DIR/F4SE/Plugins/Streamline/"*.dll "$DEST/Streamline/"
    echo "  Shared Streamline DLLs deployed"

    # XeSS-FG DLLs (Intel frame generation)
    mkdir -p "$DEST/FrameGeneration/XeSS"
    for dll in libxess_fg.dll libxell.dll; do
        [ -f "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/XeSS/$dll" ] && \
            cp "$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/XeSS/$dll" "$DEST/FrameGeneration/XeSS/"
    done
    echo "  XeSS-FG DLLs deployed"

    # Streamline DLLs — frame generation (DLSS-G interposer + frame gen)
    for dll in sl.interposer.dll sl.common.dll sl.dlss_g.dll nvngx_dlssg.dll sl.pcl.dll sl.reflex.dll; do
        [ -f "$PACKAGE_DIR/F4SE/Plugins/Streamline/$dll" ] && cp "$PACKAGE_DIR/F4SE/Plugins/Streamline/$dll" "$DEST/FrameGeneration/Streamline/"
    done
    echo "  FrameGen Streamline DLLs deployed"

    # MCM config (no-clobber: don't overwrite user changes)
    cp -u "$PACKAGE_DIR/MCM/Config/Upscaling/"* "$MOD_DIR/MCM/Config/Upscaling/" 2>/dev/null || true
    cp -u "$PACKAGE_DIR/MCM/Config/FrameGeneration/"* "$MOD_DIR/MCM/Config/FrameGeneration/" 2>/dev/null || true
    echo "  MCM config deployed"

    # Game asset overrides (meshes/textures)
    if [ -d "$PACKAGE_DIR/Meshes" ]; then
        cp -r "$PACKAGE_DIR/Meshes" "$MOD_DIR/"
        echo "  Mesh overrides deployed"
    fi
    if [ -d "$PACKAGE_DIR/Textures" ]; then
        cp -r "$PACKAGE_DIR/Textures" "$MOD_DIR/"
        echo "  Texture overrides deployed"
    fi

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
