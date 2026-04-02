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
TIMEOUT_SECONDS=180
STABILIZE_SECONDS=15

# Kill any existing game/MO2 instances
cleanup_processes() {
    taskkill.exe //F //IM Fallout4.exe 2>/dev/null || true
    taskkill.exe //F //IM "$(basename "$MO2_EXE")" 2>/dev/null || true
    sleep 2
}

# --- Phase 1: Build & Deploy ---
echo "=== Phase 1: Build & Deploy ==="
"$SCRIPT_DIR/deploy.sh" build

# --- Phase 2: Ensure clean state ---
echo "=== Phase 2: Cleanup ==="
cleanup_processes

# --- Phase 3: Launch game via MO2 ---
echo "=== Phase 3: Launching Fallout 4 ==="
"$MO2_EXE" -p "$MO2_PROFILE" "moshortcut://F4SE" &

echo "Waiting for Fallout4.exe..."
WAIT_START=$SECONDS
while ! tasklist.exe 2>/dev/null | grep -qi "Fallout4"; do
    if (( SECONDS - WAIT_START > 60 )); then
        echo "FAIL: Fallout4.exe did not start within 60s"
        mkdir -p "$RESULTS_DIR"
        echo "RESULT: FAIL - Game did not start" > "$RESULTS_DIR/result.txt"
        exit 1
    fi
    sleep 2
done
ELAPSED=$(( SECONDS - WAIT_START ))
echo "Fallout4.exe detected (${ELAPSED}s)"

# --- Phase 4: Monitor ---
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
        echo "Upscaling plugin loaded successfully"
        echo "Stabilizing for ${STABILIZE_SECONDS}s..."
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

# Check Windows Event Log for crashes
powershell.exe -Command "
    Get-WinEvent -FilterHashtable @{LogName='Application'; Level=2; StartTime=(Get-Date).AddMinutes(-5)} -ErrorAction SilentlyContinue |
    Where-Object { \$_.Message -like '*Fallout4*' } |
    Select-Object TimeCreated, Message |
    Out-File -FilePath '$RESULTS_DIR/event_log.txt' -Encoding utf8
" 2>/dev/null || true

# Take screenshot if game is still running
if $SUCCESS && tasklist.exe 2>/dev/null | grep -qi "Fallout4"; then
    SCREENSHOT_FILE="$RESULTS_DIR/screenshot.png"
    SCREENSHOT_WIN=$(cygpath -w "$SCREENSHOT_FILE")
    powershell.exe -Command "
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing
        \$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        \$bmp = New-Object System.Drawing.Bitmap(\$screen.Width, \$screen.Height)
        \$gfx = [System.Drawing.Graphics]::FromImage(\$bmp)
        \$gfx.CopyFromScreen(\$screen.Location, [System.Drawing.Point]::Empty, \$screen.Size)
        \$bmp.Save('$SCREENSHOT_WIN', [System.Drawing.Imaging.ImageFormat]::Png)
        \$gfx.Dispose()
        \$bmp.Dispose()
    " 2>/dev/null && echo "Screenshot captured: $SCREENSHOT_FILE" || echo "Screenshot failed"
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
cleanup_processes

echo "Results: $RESULTS_DIR"
