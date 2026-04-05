"""Capture primary monitor screenshot using mss."""
import sys
import os


def capture_screenshot(output_path):
    import mss
    with mss.mss() as sct:
        # Monitor 1 = primary monitor (0 = all monitors combined)
        monitor = sct.monitors[1]
        img = sct.grab(monitor)
        mss.tools.to_png(img.rgb, img.size, output=output_path)
        print(f"Screenshot saved: {output_path} ({monitor['width']}x{monitor['height']}) [mss]")
        return True


if __name__ == "__main__":
    output = sys.argv[1] if len(sys.argv) > 1 else "screenshot.png"
    output = os.path.abspath(output)

    if not capture_screenshot(output):
        print("ERROR: Screenshot capture failed")
        sys.exit(1)
