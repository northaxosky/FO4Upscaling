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

RESULTS_DIR="$PROJECT_ROOT/test-results/$(date +%Y%m%d_%H%M%S)"
TIMEOUT_SECONDS=120
STABILIZE_SECONDS=10

# Kill game process only — do NOT kill MO2, RootBuilder needs it alive to unmount root mods
kill_game() {
    taskkill.exe //F //IM Fallout4.exe 2>/dev/null || true
    sleep 5
}

# --- Phase 1: Build & Deploy ---
echo "=== Phase 1: Build & Deploy ==="
"$SCRIPT_DIR/deploy.sh" build

# --- Phase 2: Kill stale game process ---
echo "=== Phase 2: Cleanup ==="
taskkill.exe //F //IM Fallout4.exe 2>/dev/null || true
sleep 2

# --- Phase 3: Launch game via MO2 ---
echo "=== Phase 3: Launching Fallout 4 ==="
"$MO2_EXE" -p "$MO2_PROFILE" "moshortcut://${MO2_EXECUTABLE:-F4SE}" &

echo "Waiting for Fallout4.exe..."
WAIT_START=$SECONDS
while ! tasklist.exe 2>/dev/null | grep -qi "Fallout4"; do
    if (( SECONDS - WAIT_START > 30 )); then
        echo "FAIL: Fallout4.exe did not start within 30s"
        mkdir -p "$RESULTS_DIR"
        echo "RESULT: FAIL - Game did not start" > "$RESULTS_DIR/result.txt"
        exit 1
    fi
    sleep 2
done
echo "Fallout4.exe detected"

# --- Phase 3b: Wait for save to auto-load ---
echo "Waiting for save to auto-load..."
UPSCALING_LOG="$F4SE_LOG_DIR/Upscaling.log"
FG_LOG="$F4SE_LOG_DIR/AAAFrameGeneration.log"
MENU_WAIT=$SECONDS
while (( SECONDS - MENU_WAIT < 60 )); do
    if [ -f "$UPSCALING_LOG" ] && grep -q "Data loaded" "$UPSCALING_LOG" 2>/dev/null; then
        echo "Data loaded, waiting for save auto-load..."
        sleep 30
        sleep 2
        echo "Save should be loaded"
        break
    fi
    sleep 3
done

# --- Phase 4: Monitor for rendering activity ---
echo "=== Phase 4: Monitoring ==="
UPSCALING_LOG="$F4SE_LOG_DIR/Upscaling.log"
FG_LOG="$F4SE_LOG_DIR/AAAFrameGeneration.log"

GAME_START=$SECONDS
SUCCESS=false
CRASHED=false

while (( SECONDS - GAME_START < TIMEOUT_SECONDS )); do
    if ! tasklist.exe 2>/dev/null | grep -qi "Fallout4"; then
        CRASHED=true
        break
    fi

    if [ -f "$UPSCALING_LOG" ] && grep -q "\[UPSCALE\]\|\[RES\]\|\[RT\].*Recreating\|UpdateUpscaling first" "$UPSCALING_LOG" 2>/dev/null; then
        echo "Rendering detected, stabilizing ${STABILIZE_SECONDS}s..."
        sleep $STABILIZE_SECONDS

        if ! tasklist.exe 2>/dev/null | grep -qi "Fallout4"; then
            CRASHED=true
        else
            SUCCESS=true
        fi
        break
    fi
    sleep 3
done

# --- Phase 5: Collect results ---
echo "=== Phase 5: Results ==="
mkdir -p "$RESULTS_DIR"

cp "$UPSCALING_LOG" "$RESULTS_DIR/" 2>/dev/null || true
cp "$FG_LOG" "$RESULTS_DIR/" 2>/dev/null || true

# Take screenshot if game is still running (DXGI desktop duplication via dxcam)
if $SUCCESS && tasklist.exe 2>/dev/null | grep -qi "Fallout4"; then
    SCREENSHOT_FILE="$RESULTS_DIR/screenshot.png"
    python "$SCRIPT_DIR/screenshot.py" "$SCREENSHOT_FILE" 2>/dev/null && echo "Screenshot captured" || echo "Screenshot failed"
fi

# Summary
{
    echo "Test Run: $(date)"
    echo ""
    if $SUCCESS; then
        echo "RESULT: PASS"
    elif $CRASHED; then
        echo "RESULT: CRASH"
        echo "Game exited after $(( SECONDS - GAME_START ))s"
    else
        echo "RESULT: TIMEOUT"
    fi
    echo ""
    echo "=== Upscaling.log ==="
    cat "$RESULTS_DIR/Upscaling.log" 2>/dev/null || echo "(none)"
    echo ""
    echo "=== AAAFrameGeneration.log ==="
    cat "$RESULTS_DIR/AAAFrameGeneration.log" 2>/dev/null || echo "(none)"
} > "$RESULTS_DIR/result.txt"

cat "$RESULTS_DIR/result.txt"

# --- Phase 6: Kill game ---
echo "=== Phase 6: Cleanup ==="
kill_game

echo "Results: $RESULTS_DIR"
