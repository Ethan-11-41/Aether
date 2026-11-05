#!/usr/bin/env python3
"""Generates Aether's placeholder app icon (PNG + Windows .ico) from a small SVG.

Requires ImageMagick ("magick" or "convert") with SVG support (librsvg / rsvg-convert).
Run from the repository root:
    python3 tools/make_icon.py

The generated files are committed, so this only needs re-running if the design changes.
"""

import math
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "resources" / "icon"


def wave_path(size: float) -> str:
    """A sine wave whose amplitude swells in the middle, across the icon."""
    left, right = size * 0.18, size * 0.82
    mid_y = size * 0.5
    points = []
    steps = 240
    for i in range(steps + 1):
        t = i / steps
        x = left + (right - left) * t
        envelope = math.sin(math.pi * t) ** 1.5
        y = mid_y - math.sin(t * 2.0 * math.pi * 2.5) * size * 0.2 * envelope
        points.append(f"{x:.2f},{y:.2f}")
    return "M " + " L ".join(points)


def make_svg(size: int) -> str:
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {size} {size}">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#26304a"/>
      <stop offset="1" stop-color="#0f1320"/>
    </linearGradient>
    <linearGradient id="wave" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#4fc3c9"/>
      <stop offset="1" stop-color="#a98bff"/>
    </linearGradient>
  </defs>
  <rect x="{size*0.04}" y="{size*0.04}" width="{size*0.92}" height="{size*0.92}" rx="{size*0.2}" fill="url(#bg)"/>
  <path d="{wave_path(size)}" fill="none" stroke="url(#wave)" stroke-width="{size*0.06}"
        stroke-linecap="round" stroke-linejoin="round"/>
</svg>
"""


def main() -> int:
    magick = shutil.which("magick") or shutil.which("convert")
    if magick is None:
        print("ImageMagick not found", file=sys.stderr)
        return 1

    OUT.mkdir(parents=True, exist_ok=True)
    svg_path = OUT / "aether-icon.svg"
    svg_path.write_text(make_svg(1024), encoding="utf-8")

    for px in (1024, 256):
        subprocess.run([magick, "-background", "none", "-density", "96", str(svg_path),
                        "-resize", f"{px}x{px}", str(OUT / f"aether-icon-{px}.png")], check=True)

    subprocess.run([magick, str(OUT / "aether-icon-1024.png"), "-define",
                    "icon:auto-resize=256,128,64,48,32,16", str(OUT / "aether.ico")], check=True)
    print("Wrote", ", ".join(p.name for p in sorted(OUT.iterdir())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
