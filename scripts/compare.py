"""Visual regression testing via SSIM comparison against a reference image.

Usage:
    python compare.py <test_image>                      Compare against default reference
    python compare.py <test_image> -r <reference>       Compare against specific reference
    python compare.py --update-reference <image>        Set a new reference image
    python compare.py --update-reference <image> -r <name>  Set a named reference

Output: JSON to stdout with pass/fail verdict and metrics.
Side effect: saves a diff heatmap image alongside the test image.

Dependencies: pip install scikit-image Pillow numpy
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from skimage.metrics import structural_similarity as ssim


SCRIPT_DIR = Path(__file__).resolve().parent
REFERENCES_DIR = SCRIPT_DIR / "references"
DEFAULT_REFERENCE = REFERENCES_DIR / "enb_baseline.png"

SSIM_THRESHOLD = 0.60
HISTOGRAM_THRESHOLD = 0.50


def load_image(path: Path) -> np.ndarray:
    """Load an image as a numpy array, converting to RGB."""
    img = Image.open(path).convert("RGB")
    return np.array(img)


def compute_ssim(reference: np.ndarray, test: np.ndarray) -> tuple[float, np.ndarray]:
    """Compute SSIM between two RGB images. Returns (score, diff_map).

    Compares across all color channels to catch color-shift regressions
    that would be invisible in grayscale.
    """
    score, diff_map = ssim(
        reference.astype(np.float64),
        test.astype(np.float64),
        full=True,
        data_range=255.0,
        channel_axis=2,
    )
    return float(score), diff_map


def compute_histogram_correlation(reference: np.ndarray, test: np.ndarray) -> float:
    """Compute normalized histogram correlation across RGB channels.

    Returns 0.0–1.0 where 1.0 = identical distribution.
    Catches total failures like black screen where all pixels are near zero.
    Pearson correlation can go negative; clamped to 0 since we only care about similarity.
    """
    correlations = []
    for channel in range(3):
        ref_hist, _ = np.histogram(reference[:, :, channel], bins=256, range=(0, 256))
        test_hist, _ = np.histogram(test[:, :, channel], bins=256, range=(0, 256))

        ref_norm = ref_hist.astype(np.float64)
        test_norm = test_hist.astype(np.float64)

        ref_mean = np.mean(ref_norm)
        test_mean = np.mean(test_norm)

        numerator = np.sum((ref_norm - ref_mean) * (test_norm - test_mean))
        denominator = np.sqrt(
            np.sum((ref_norm - ref_mean) ** 2) * np.sum((test_norm - test_mean) ** 2)
        )

        if denominator < 1e-10:
            correlations.append(0.0)
        else:
            correlations.append(float(numerator / denominator))

    return float(max(0.0, np.mean(correlations)))


def save_diff_image(diff_map: np.ndarray, output_path: Path) -> None:
    """Save SSIM diff map as a heatmap image.

    Red = high difference, green = similar. Makes regressions visually obvious.
    """
    # Average channels if 3D (from multichannel SSIM)
    if diff_map.ndim == 3:
        diff_map = np.mean(diff_map, axis=2)

    # Invert so high error maps to red, not green
    error_map = 1.0 - diff_map
    error_map = np.clip(error_map, 0.0, 1.0)

    # Map to RGB: low error = green, high error = red
    rgb = np.zeros((*error_map.shape, 3), dtype=np.uint8)
    rgb[:, :, 0] = (error_map * 255).astype(np.uint8)       # red channel
    rgb[:, :, 1] = ((1 - error_map) * 255).astype(np.uint8)  # green channel

    Image.fromarray(rgb).save(output_path)


def compare(test_path: Path, reference_path: Path, ssim_threshold: float = SSIM_THRESHOLD) -> dict:
    """Run full comparison and return results dict."""
    if not reference_path.exists():
        return {
            "pass": False,
            "error": f"Reference image not found: {reference_path}",
            "hint": f"Run: python {Path(__file__).name} --update-reference <image>",
        }

    if not test_path.exists():
        return {
            "pass": False,
            "error": f"Test image not found: {test_path}",
        }

    try:
        reference = load_image(reference_path)
        test = load_image(test_path)
    except Exception as exc:
        return {"pass": False, "error": f"Failed to load image: {exc}"}

    # Resolution mismatch is itself a failure — viewport compounding changes content, not dimensions
    if reference.shape != test.shape:
        return {
            "pass": False,
            "error": "Resolution mismatch",
            "test_resolution": f"{test.shape[1]}x{test.shape[0]}",
            "reference_resolution": f"{reference.shape[1]}x{reference.shape[0]}",
            "test_image": str(test_path),
            "reference_image": str(reference_path),
        }

    ssim_score, diff_map = compute_ssim(reference, test)
    histogram_corr = compute_histogram_correlation(reference, test)

    # Save diff heatmap alongside test image
    diff_path = test_path.parent / f"{test_path.stem}_diff.png"
    save_diff_image(diff_map, diff_path)

    passed = ssim_score >= ssim_threshold and histogram_corr >= HISTOGRAM_THRESHOLD

    return {
        "pass": passed,
        "ssim": round(ssim_score, 4),
        "ssim_threshold": ssim_threshold,
        "histogram_correlation": round(histogram_corr, 4),
        "histogram_threshold": HISTOGRAM_THRESHOLD,
        "resolution": f"{reference.shape[1]}x{reference.shape[0]}",
        "test_image": str(test_path),
        "reference_image": str(reference_path),
        "diff_image": str(diff_path),
    }


def update_reference(source_path: Path, reference_path: Path) -> dict:
    """Copy source image as the new reference."""
    if not source_path.exists():
        return {"error": f"Source image not found: {source_path}"}

    REFERENCES_DIR.mkdir(parents=True, exist_ok=True)

    with Image.open(source_path) as img:
        img.save(reference_path)
        return {
            "updated": True,
            "reference": str(reference_path),
            "resolution": f"{img.width}x{img.height}",
        }


def main():
    parser = argparse.ArgumentParser(description="Visual regression testing via SSIM")
    parser.add_argument("test_image", nargs="?", help="Path to test screenshot")
    parser.add_argument(
        "-r", "--reference",
        default=str(DEFAULT_REFERENCE),
        help=f"Reference image path (default: {DEFAULT_REFERENCE.name})",
    )
    parser.add_argument(
        "--update-reference",
        metavar="IMAGE",
        help="Set a new reference image from the given source",
    )
    parser.add_argument(
        "-t", "--threshold",
        type=float,
        help=f"SSIM pass threshold (default: {SSIM_THRESHOLD})",
    )

    args = parser.parse_args()

    reference_path = Path(args.reference)
    if not reference_path.is_absolute():
        reference_path = REFERENCES_DIR / reference_path

    if args.update_reference:
        result = update_reference(Path(args.update_reference), reference_path)
        print(json.dumps(result, indent=2))
        sys.exit(0 if "updated" in result else 1)

    if not args.test_image:
        parser.error("test_image is required unless using --update-reference")

    if args.threshold is not None:
        threshold = args.threshold
    else:
        threshold = SSIM_THRESHOLD

    result = compare(Path(args.test_image), reference_path, threshold)
    print(json.dumps(result, indent=2))
    sys.exit(0 if result.get("pass") else 1)


if __name__ == "__main__":
    main()
