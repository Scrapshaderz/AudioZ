#!/usr/bin/env python3
# AudioZ - Erzeugt assets/icon.png und assets/icon.ico (Wellenform auf Farbverlauf).
# Benoetigt: pip install pillow
# SPDX-License-Identifier: GPL-3.0-or-later
import math
import os
from PIL import Image, ImageDraw

SIZE = 1024
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")


def gradient(size, top, bottom):
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = (0.65 * y + 0.35 * x) / size  # leicht diagonaler Verlauf
            px[x, y] = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    base = gradient(SIZE, (38, 64, 255), (126, 34, 206))

    # abgerundetes Quadrat als Maske
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, SIZE - 1, SIZE - 1), radius=int(SIZE * 0.225), fill=255)

    icon = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    icon.paste(base, (0, 0), mask)

    # Wellenform aus Balken; die Huellkurve ist ein Z-foermiger Verlauf
    draw = ImageDraw.Draw(icon)
    bars = 15
    margin = SIZE * 0.17
    gap = SIZE * 0.012
    width = (SIZE - 2 * margin - gap * (bars - 1)) / bars
    centre = SIZE / 2
    for i in range(bars):
        x = i / (bars - 1)
        envelope = 0.18 + 0.82 * abs(math.sin(x * math.pi * 1.0 + 0.35)) ** 0.9
        wobble = 0.78 + 0.22 * math.cos(i * 1.7)
        half = SIZE * 0.31 * envelope * wobble
        x0 = margin + i * (width + gap)
        draw.rounded_rectangle((x0, centre - half, x0 + width, centre + half), radius=width / 2, fill=(255, 255, 255, 245))

    png = icon.resize((512, 512), Image.LANCZOS)
    png.save(os.path.join(OUT, "icon.png"))
    icon.save(os.path.join(OUT, "icon.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("assets/icon.png und assets/icon.ico geschrieben")


if __name__ == "__main__":
    main()
