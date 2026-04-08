#!/bin/bash
# Compare frame generation backends: No FG vs FSR3 vs DLSS-G
# Runs the game 3 times with different configs, captures screenshots
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -f "$SCRIPT_DIR/.env" ]; then
    source "$SCRIPT_DIR/.env"
else
    echo "ERROR: scripts/.env not found. Copy scripts/.env.example to scripts/.env and configure paths."
    exit 1
fi

: "${MOD_DIR:?ERROR: MOD_DIR not set in .env}"
: "${MO2_EXE:?ERROR: MO2_EXE not set in .env}"
: "${MO2_PROFILE:?ERROR: MO2_PROFILE not set in .env}"
: "${F4SE_LOG_DIR:?ERROR: F4SE_LOG_DIR not set in .env}"

INI_FILE="$MOD_DIR/MCM/Settings/FrameGeneration.ini"
RESULTS_DIR="$PROJECT_ROOT/test-results/compare_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

STABILIZE_SECONDS=15

# Reusable: update INI, launch game, wait for save, screenshot, kill
run_config() {
    local NAME="$1"
    local FG_MODE="$2"
    local FG_TYPE="$3"

    echo ""
    echo "========================================="
    echo "  Config: $NAME (bFrameGenerationMode=$FG_MODE, iFrameGenType=$FG_TYPE)"
    echo "========================================="

    # Update INI
    cat > "$INI_FILE" <<INIEOF
[Settings]
bFrameGenerationMode=$FG_MODE
bFrameLimitMode=false
iFrameGenType=$FG_TYPE
INIEOF

    # Kill stale game
    taskkill.exe //F //IM Fallout4.exe 2>/dev/null || true
    sleep 3

    # Launch via MO2
    "$MO2_EXE" -p "$MO2_PROFILE" "moshortcut://${MO2_EXECUTABLE:-F4SE}" &

    # Wait for Fallout4.exe
    echo "  Waiting for Fallout4.exe..."
    local WAIT_START=$SECONDS
    while ! tasklist.exe 2>/dev/null | grep -qi "Fallout4"; do
        if (( SECONDS - WAIT_START > 30 )); then
            echo "  FAIL: Game did not start"
            echo "FAIL" > "$RESULTS_DIR/${NAME}.txt"
            return
        fi
        sleep 2
    done

    # Wait for save auto-load
    echo "  Waiting for save to load..."
    local UPSCALING_LOG="$F4SE_LOG_DIR/Upscaling.log"
    local MENU_WAIT=$SECONDS
    while (( SECONDS - MENU_WAIT < 90 )); do
        if [ -f "$UPSCALING_LOG" ] && grep -q "Data loaded" "$UPSCALING_LOG" 2>/dev/null; then
            echo "  Data loaded, waiting for save..."
            sleep 30
            break
        fi
        sleep 3
    done

    # Stabilize — game window is always-on-top via INI
    echo "  Stabilizing ${STABILIZE_SECONDS}s..."
    sleep $STABILIZE_SECONDS

    # Check game still alive
    if ! tasklist.exe 2>/dev/null | grep -qi "Fallout4"; then
        echo "  CRASH: Game exited before screenshot"
        echo "CRASH" > "$RESULTS_DIR/${NAME}.txt"
        return
    fi

    # Screenshot
    python "$SCRIPT_DIR/screenshot.py" "$RESULTS_DIR/${NAME}.png" 2>/dev/null && echo "  Screenshot: ${NAME}.png" || echo "  Screenshot failed"

    # Copy logs
    cp "$F4SE_LOG_DIR/AAAFrameGeneration.log" "$RESULTS_DIR/${NAME}_fg.log" 2>/dev/null || true
    cp "$F4SE_LOG_DIR/Upscaling.log" "$RESULTS_DIR/${NAME}_upscaling.log" 2>/dev/null || true

    # Kill game
    taskkill.exe //F //IM Fallout4.exe 2>/dev/null || true
    sleep 5
}

# Build & deploy first
echo "=== Building ==="
"$SCRIPT_DIR/deploy.sh" build

# Run all 3 configs
run_config "1_no_fg"    "false" "0"
run_config "2_fsr3_fg"  "true"  "0"
run_config "3_dlssg_fg" "true"  "1"

# Restore default INI
cat > "$INI_FILE" <<INIEOF
[Settings]
bFrameGenerationMode=true
bFrameLimitMode=false
iFrameGenType=1
INIEOF

echo ""
echo "========================================="
echo "  All screenshots in: $RESULTS_DIR"
echo "========================================="
ls -la "$RESULTS_DIR"/*.png 2>/dev/null || echo "  (no screenshots)"
