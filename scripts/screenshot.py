"""Capture primary monitor screenshot using Windows.Graphics.Capture API."""
import sys
import asyncio
import ctypes
import ctypes.wintypes

# pip install winrt-Windows.Graphics.Capture winrt-Windows.Graphics.DirectX
# But these may not be available — fall back to d3dshot or mss

def capture_monitor_mss(output_path):
    """Capture using mss (multi-screenshot) library — works with HDR/borderless."""
    try:
        import mss
        with mss.mss() as sct:
            monitor = sct.monitors[1]  # Primary monitor
            img = sct.grab(monitor)
            # Save as PNG
            from PIL import Image
            pil_img = Image.frombytes("RGB", img.size, img.bgra, "raw", "BGRX")
            pil_img.save(output_path, "PNG")
            print(f"Screenshot saved: {output_path} ({img.size[0]}x{img.size[1]})")
            return True
    except ImportError:
        return False

def capture_monitor_dxcam(output_path):
    """Capture using dxcam — uses DXGI duplication, handles HDR."""
    try:
        import dxcam
        camera = dxcam.create()
        frame = camera.grab()
        if frame is not None:
            from PIL import Image
            img = Image.fromarray(frame)
            img.save(output_path, "PNG")
            print(f"Screenshot saved: {output_path} ({img.size[0]}x{img.size[1]})")
            return True
        else:
            print("dxcam: No frame captured")
            return False
    except ImportError:
        return False

def capture_monitor_pillow(output_path):
    """Capture using Pillow's ImageGrab — simplest fallback."""
    try:
        from PIL import ImageGrab
        img = ImageGrab.grab(all_screens=False)
        img.save(output_path, "PNG")
        print(f"Screenshot saved: {output_path} ({img.size[0]}x{img.size[1]})")
        return True
    except ImportError:
        return False

if __name__ == "__main__":
    output = sys.argv[1] if len(sys.argv) > 1 else "screenshot.png"

    # Try methods in order of quality
    if capture_monitor_dxcam(output):
        pass
    elif capture_monitor_mss(output):
        pass
    elif capture_monitor_pillow(output):
        pass
    else:
        print("ERROR: No screenshot library available. Install one:")
        print("  pip install dxcam Pillow")
        print("  pip install mss Pillow")
        sys.exit(1)
