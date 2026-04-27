#!/bin/bash
# Downloads third-party SDK runtime DLLs needed for packaging.
# These are proprietary binaries (NVIDIA, AMD, Intel) that shouldn't live in git.
# Run this once after cloning, or when SDK versions change.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PACKAGE_DIR="$PROJECT_ROOT/package"
TEMP_DIR="$PROJECT_ROOT/.sdk-cache"

# SDK versions — update these when upgrading
STREAMLINE_VERSION="v2.11.1"
FIDELITYFX_VERSION="v1.1.4"

STREAMLINE_URL="https://github.com/NVIDIA-RTX/Streamline/releases/download/${STREAMLINE_VERSION}/streamline-sdk-${STREAMLINE_VERSION}.zip"
FIDELITYFX_URL="https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/download/${FIDELITYFX_VERSION}/FidelityFX-SDK-${FIDELITYFX_VERSION}.zip"

mkdir -p "$TEMP_DIR"

# ─── Streamline (NVIDIA DLSS-G) ──────────────────────────────────────────────
STREAMLINE_DEST="$PACKAGE_DIR/F4SE/Plugins/Streamline"
STREAMLINE_DLLS=(
    sl.interposer.dll
    sl.common.dll
    sl.dlss.dll
    sl.dlss_g.dll
    sl.pcl.dll
    sl.reflex.dll
    nvngx_dlss.dll
    nvngx_dlssg.dll
)

fetch_streamline() {
    # Check if all DLLs already exist
    local missing=false
    for dll in "${STREAMLINE_DLLS[@]}"; do
        if [ ! -f "$STREAMLINE_DEST/$dll" ]; then
            missing=true
            break
        fi
    done

    if [ "$missing" = false ] && [ "$1" != "--force" ]; then
        echo "  Streamline DLLs already present, skipping (use --force to re-download)"
        return
    fi

    local zip="$TEMP_DIR/streamline-sdk-${STREAMLINE_VERSION}.zip"
    if [ ! -f "$zip" ]; then
        echo "  Downloading Streamline SDK ${STREAMLINE_VERSION}..."
        curl -fSL "$STREAMLINE_URL" -o "$zip" || { echo "ERROR: Failed to download $STREAMLINE_URL"; exit 1; }
    fi

    mkdir -p "$STREAMLINE_DEST"
    for dll in "${STREAMLINE_DLLS[@]}"; do
        unzip -jo "$zip" "bin/x64/$dll" -d "$STREAMLINE_DEST" 2>/dev/null || \
            echo "  WARNING: $dll not found in archive"
    done
    [[ -f "$STREAMLINE_DEST/sl.interposer.dll" ]] || { echo "ERROR: sl.interposer.dll not found after extraction"; exit 1; }
    echo "  Streamline ${STREAMLINE_VERSION}: ${#STREAMLINE_DLLS[@]} DLLs extracted"
}

# ─── FidelityFX (AMD FSR3) ───────────────────────────────────────────────────
FIDELITYFX_DEST="$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/FidelityFX"

fetch_fidelityfx() {
    if [ -f "$FIDELITYFX_DEST/amd_fidelityfx_dx12.dll" ] && [ "$1" != "--force" ]; then
        echo "  FidelityFX DLL already present, skipping (use --force to re-download)"
        return
    fi

    local zip="$TEMP_DIR/FidelityFX-SDK-${FIDELITYFX_VERSION}.zip"
    if [ ! -f "$zip" ]; then
        echo "  Downloading FidelityFX SDK ${FIDELITYFX_VERSION}..."
        curl -fSL "$FIDELITYFX_URL" -o "$zip" || { echo "ERROR: Failed to download $FIDELITYFX_URL"; exit 1; }
    fi

    mkdir -p "$FIDELITYFX_DEST"
    unzip -jo "$zip" "PrebuiltSignedDLL/amd_fidelityfx_dx12.dll" -d "$FIDELITYFX_DEST" 2>/dev/null
    [[ -f "$FIDELITYFX_DEST/amd_fidelityfx_dx12.dll" ]] || { echo "ERROR: amd_fidelityfx_dx12.dll not found after extraction"; exit 1; }
    echo "  FidelityFX ${FIDELITYFX_VERSION}: amd_fidelityfx_dx12.dll extracted"
}

# ─── XeSS (Intel XeSS-FG) ───────────────────────────────────────────────────
XESS_DEST="$PACKAGE_DIR/F4SE/Plugins/FrameGeneration/XeSS"
XESS_SRC="$PROJECT_ROOT/extern/XeSS/bin"

fetch_xess() {
    if [ -f "$XESS_DEST/libxess_fg.dll" ] && [ -f "$XESS_DEST/libxell.dll" ] && [ "$1" != "--force" ]; then
        echo "  XeSS DLLs already present, skipping (use --force to re-copy)"
        return
    fi

    if [ ! -d "$XESS_SRC" ]; then
        echo "ERROR: extern/XeSS not found. Run: git submodule update --init extern/XeSS"
        exit 1
    fi

    mkdir -p "$XESS_DEST"
    cp "$XESS_SRC/libxess_fg.dll" "$XESS_DEST/" || { echo "ERROR: Failed to copy libxess_fg.dll"; exit 1; }
    cp "$XESS_SRC/libxell.dll" "$XESS_DEST/" || { echo "ERROR: Failed to copy libxell.dll"; exit 1; }
    echo "  XeSS: libxess_fg.dll + libxell.dll copied from submodule"
}

# ─── Main ────────────────────────────────────────────────────────────────────
echo "=== Fetching SDK runtime DLLs ==="
FORCE_FLAG="$1"
fetch_streamline "$FORCE_FLAG"
fetch_fidelityfx "$FORCE_FLAG"
fetch_xess "$FORCE_FLAG"
echo "=== Done ==="
