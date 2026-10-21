#!/usr/bin/env python3
"""Aplica um icone PNG quadrado (ex.: gerado por IA) em todos os destinos do app.

Uso (da raiz do repositorio):
    python3 tools/apply_icon.py caminho/do/icone.png

Fundo preto puro nos cantos vira transparente (preenchimento a partir das bordas).
Grava dist/azahar.png, azahar.ico, apple/azahar.icns, apple/launch_logo.png e os
256x256 do tema padrao. Dependencia: Pillow.
"""
import sys
from collections import deque
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent.parent


def knock_out_corners(im, threshold=8):
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    seen = bytearray(w * h)
    queue = deque()
    for x in range(w):
        queue.extend([(x, 0), (x, h - 1)])
    for y in range(h):
        queue.extend([(0, y), (w - 1, y)])
    while queue:
        x, y = queue.popleft()
        if not (0 <= x < w and 0 <= y < h) or seen[y * w + x]:
            continue
        r, g, b, _ = px[x, y]
        if max(r, g, b) > threshold:
            continue
        seen[y * w + x] = 1
        queue.extend([(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)])
    mask = Image.new("L", (w, h), 255)
    mp = mask.load()
    for y in range(h):
        for x in range(w):
            if seen[y * w + x]:
                mp[x, y] = 0
    mask = mask.filter(ImageFilter.GaussianBlur(1.2))
    im.putalpha(mask)
    return im


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = knock_out_corners(Image.open(sys.argv[1]))
    d = ROOT / "dist"

    def sized(n):
        return src.resize((n, n), Image.LANCZOS)

    sized(512).save(d / "azahar.png")
    sized(512).save(d / "apple" / "launch_logo.png")
    for sub in ("icons", "icons_light"):
        target = d / "qt_themes" / "default" / sub / "256x256"
        if target.exists():
            sized(256).save(target / "azahar.png")
    sized(256).save(d / "azahar.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    sizes = [16, 32, 64, 128, 256, 512, 1024]
    images = [sized(s) for s in sizes]
    images[-1].save(d / "apple" / "azahar.icns", append_images=images[:-1])
    print("Icone aplicado.")


if __name__ == "__main__":
    main()
