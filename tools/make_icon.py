#!/usr/bin/env python3
# Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
"""Draws Aether's icon and logo (the owner's design, redrawn as vector shapes) and renders the
files the build and the installer use:

  resources/icon/aether-icon.svg        the app icon: an "A" peak with a sound wave inside
  resources/icon/aether-icon-small.svg  the same, simplified (thicker, 3 bars) for 16-32 pixels
  resources/icon/aether-icon-1024.png   ICON_BIG for the build
  resources/icon/aether-icon-32.png     ICON_SMALL for the build (title bar, taskbar)
  resources/icon/aether.ico             the installer's icon (16-256 pixels)
  resources/icon/aether-mark.svg        the mark alone (no background), for light backgrounds
  resources/icon/aether-mark-white.svg  the same for dark backgrounds (the start screen)
  docs/images/aether-logo-light.png     the README's logo: the mark and "Aether" (dark text)
  docs/images/aether-logo-dark.png      the same with white text (GitHub's dark mode)

Needs rsvg-convert and ImageMagick (`convert`). Run from the repository root:
    python3 tools/make_icon.py
"""

import pathlib
import subprocess

OUT = pathlib.Path("resources/icon")
BACKGROUND = "#0b0c0e"
PEAK = "#ffffff"
WAVE = "#26a3a4"     # the logo's teal


def peak_path(size, left, right, top, bottom, thickness):
    """The "A" without its bar: an outer triangle with a parallel inner one cut out."""
    middle = size / 2
    slope = (middle - left) / (bottom - top)             # x per y along the outer left edge
    inner_left, inner_right = left + thickness, right - thickness
    inner_top = bottom - (middle - inner_left) / slope   # keeps the inner edges parallel
    return (f"M {left:.1f},{bottom:.1f} L {middle:.1f},{top:.1f} L {right:.1f},{bottom:.1f} "
            f"L {inner_right:.1f},{bottom:.1f} L {middle:.1f},{inner_top:.1f} L {inner_left:.1f},{bottom:.1f} Z")


def bars(size, centre_y, width, spacing, heights, colour):
    out = []
    count = len(heights)
    for i, h in enumerate(heights):
        x = size / 2 + (i - (count - 1) / 2) * spacing - width / 2
        out.append(f'<rect x="{x:.1f}" y="{centre_y - h / 2:.1f}" width="{width:.1f}" height="{h:.1f}" '
                   f'rx="{width / 2:.1f}" fill="{colour}"/>')
    return "\n  ".join(out)


def icon_svg(small=False, with_background=True, peak_colour=PEAK):
    size = 1024.0
    margin = 64.0 if with_background else 0.0
    s = size - 2 * margin                                # the rounded square
    f = lambda v: margin + v * s

    if small:
        # Fewer, thicker shapes, so it still reads at 16 and 32 pixels.
        peak = peak_path(size, f(0.10), f(0.90), f(0.16), f(0.86), 0.13 * s)
        wave = bars(size, f(0.66), 0.075 * s, 0.135 * s, [0.12 * s, 0.24 * s, 0.12 * s], WAVE)
    else:
        peak = peak_path(size, f(0.14), f(0.86), f(0.195), f(0.83), 0.075 * s)
        wave = bars(size, f(0.62), 0.032 * s, 0.05 * s, [0.075 * s, 0.13 * s, 0.20 * s, 0.13 * s, 0.075 * s], WAVE)

    background = (f'<rect x="{margin}" y="{margin}" width="{s}" height="{s}" rx="{0.24 * s:.1f}" fill="{BACKGROUND}"/>'
                  if with_background else "")
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  {background}
  <path d="{peak}" fill="{peak_colour}"/>
  {wave}
</svg>
'''


def make_wordmarks():
    """The mark beside "Aether" in Inter SemiBold (the app's font), like the owner's wordmark."""
    from PIL import Image, ImageDraw, ImageFont

    docs = pathlib.Path("docs/images")
    docs.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype("resources/fonts/Inter-SemiBold.ttf", 150)

    for name, mark, text_colour in (("light", "aether-mark.svg", (17, 17, 17, 255)),
                                    ("dark", "aether-mark-white.svg", (255, 255, 255, 255))):
        mark_png = OUT / "_mark.png"
        run("rsvg-convert", "-w", "220", "-h", "220", str(OUT / mark), "-o", str(mark_png))
        image = Image.new("RGBA", (760, 240), (0, 0, 0, 0))
        image.alpha_composite (Image.open (mark_png).convert ("RGBA"), (10, 0))
        ImageDraw.Draw (image).text ((250, 120), "Aether", font=font, fill=text_colour, anchor="lm")
        image.save (docs / f"aether-logo-{name}.png")
        mark_png.unlink()


def run(*args):
    subprocess.run(args, check=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "aether-icon.svg").write_text(icon_svg())
    (OUT / "aether-icon-small.svg").write_text(icon_svg(small=True))
    (OUT / "aether-mark.svg").write_text(icon_svg(with_background=False, peak_colour="#111111"))
    (OUT / "aether-mark-white.svg").write_text(icon_svg(with_background=False, peak_colour="#ffffff"))
    make_wordmarks()

    run("rsvg-convert", "-w", "1024", "-h", "1024", str(OUT / "aether-icon.svg"), "-o", str(OUT / "aether-icon-1024.png"))
    run("rsvg-convert", "-w", "32", "-h", "32", str(OUT / "aether-icon-small.svg"), "-o", str(OUT / "aether-icon-32.png"))

    # The installer's .ico: the simple version for the small sizes, the full one above.
    parts = []
    for px in (16, 24, 32, 48, 64, 128, 256):
        source = "aether-icon-small.svg" if px <= 32 else "aether-icon.svg"
        target = OUT / f"_ico_{px}.png"
        run("rsvg-convert", "-w", str(px), "-h", str(px), str(OUT / source), "-o", str(target))
        parts.append(str(target))
    run("convert", *parts, str(OUT / "aether.ico"))
    for p in parts:
        pathlib.Path(p).unlink()

    old = OUT / "aether-icon-256.png"
    if old.exists():
        old.unlink()


if __name__ == "__main__":
    main()
