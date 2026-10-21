#!/usr/bin/env python3
"""Gera o icone do app (portatil de duas telas) e grava SVG, PNGs e .icns.

Uso (da raiz do repositorio):
    python3 tools/make_icon.py                    # cor padrao
    python3 tools/make_icon.py --color "#E8604C"  # outra cor primaria

Dependencias: PySide6 (QtSvg) e Pillow.
Todas as outras cores do icone (brilho, sombra, dobradica, botoes) sao
derivadas da cor primaria, entao basta trocar --color.
"""

import argparse
import colorsys
import os
import sys
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

ROOT = Path(__file__).resolve().parent.parent


def shade(hex_color, dl=0.0, ds=0.0, dh=0.0):
    """Ajusta luminosidade, saturacao e matiz de uma cor #RRGGBB."""
    h = hex_color.lstrip("#")
    r, g, b = (int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
    hue, light, sat = colorsys.rgb_to_hls(r, g, b)
    hue = (hue + dh) % 1.0
    light = min(1.0, max(0.0, light + dl))
    sat = min(1.0, max(0.0, sat + ds))
    r, g, b = colorsys.hls_to_rgb(hue, light, sat)
    return "#{:02X}{:02X}{:02X}".format(round(r * 255), round(g * 255), round(b * 255))


def build_svg(primary):
    light = shade(primary, dl=0.07)
    dark = shade(primary, dl=-0.03)
    hinge = shade(primary, dl=-0.17, dh=0.015)
    button_hi = shade(primary, dl=-0.06, ds=0.05)
    button_lo = shade(primary, dl=-0.14, ds=0.05)
    tile_lo = shade(primary, dl=0.36, ds=-0.1)
    shadow = shade(primary, dl=-0.3)

    petals = "".join(
        '<ellipse cx="0" cy="-30" rx="17" ry="31" transform="rotate({})"/>'.format(a)
        for a in (0, 72, 144, 216, 288)
    )

    return """<?xml version="1.0" encoding="UTF-8"?>
<!-- Icone do app: portatil de duas telas. Gerado por tools/make_icon.py -->
<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <defs>
    <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#F6F9FD"/>
      <stop offset="1" stop-color="{tile_lo}"/>
    </linearGradient>
    <linearGradient id="shell" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{light}"/>
      <stop offset="1" stop-color="{dark}"/>
    </linearGradient>
    <linearGradient id="screen" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#6C757D"/>
      <stop offset="1" stop-color="#2F3337"/>
    </linearGradient>
    <linearGradient id="btn" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{button_hi}"/>
      <stop offset="1" stop-color="{button_lo}"/>
    </linearGradient>
  </defs>

  <rect x="100" y="100" width="824" height="824" rx="186" fill="url(#tile)"/>

  <g transform="translate(512 512) scale(0.66) translate(-600 -600)">
    <rect x="165" y="205" width="870" height="870" rx="64" fill="{shadow}" opacity="0.07"/>
    <rect x="175" y="190" width="850" height="870" rx="60" fill="{shadow}" opacity="0.08"/>
    <rect x="190" y="170" width="820" height="880" rx="56" fill="{shadow}" opacity="0.09"/>

    <path d="M200 150H1000a50 50 0 0 1 50 50V550a25 25 0 0 1-25 25H175a25 25 0 0 1-25-25V200a50 50 0 0 1 50-50Z" fill="url(#shell)"/>
    <path d="M175 625H1025a25 25 0 0 1 25 25V1000a50 50 0 0 1-50 50H200a50 50 0 0 1-50-50V650a25 25 0 0 1 25-25Z" fill="url(#shell)"/>
    <rect x="150" y="575" width="900" height="50" rx="25" fill="{hinge}"/>

    <rect x="350" y="250" width="500" height="325" fill="url(#screen)"/>
    <path d="M350 250H640L350 540Z" fill="#FFFFFF" opacity="0.08"/>
    <rect x="400" y="675" width="400" height="300" fill="url(#screen)"/>
    <path d="M400 675H650L400 925Z" fill="#FFFFFF" opacity="0.08"/>

    <g transform="translate(600 825) scale(0.95)">
      <g fill="#FFFFFF">{petals}</g>
      <circle r="14" fill="#F5B301"/>
    </g>

    <circle cx="275" cy="750" r="50" fill="#C9D3DC"/>
    <rect x="213" y="900" width="125" height="50" rx="14" fill="url(#btn)"/>
    <rect x="250" y="862" width="50" height="125" rx="14" fill="url(#btn)"/>
    <circle cx="925" cy="700" r="26" fill="url(#btn)"/>
    <circle cx="875" cy="750" r="26" fill="url(#btn)"/>
    <circle cx="975" cy="750" r="26" fill="url(#btn)"/>
    <circle cx="925" cy="800" r="26" fill="url(#btn)"/>
  </g>
</svg>
""".format(
        tile_lo=tile_lo, light=light, dark=dark, button_hi=button_hi,
        button_lo=button_lo, hinge=hinge, shadow=shadow, petals=petals,
    )


def render_png(svg_text, size, out_path):
    from PySide6.QtCore import QByteArray, Qt
    from PySide6.QtGui import QGuiApplication, QImage, QPainter
    from PySide6.QtSvg import QSvgRenderer

    app = QGuiApplication.instance() or QGuiApplication(sys.argv[:1])
    renderer = QSvgRenderer(QByteArray(svg_text.encode("utf-8")))
    image = QImage(size, size, QImage.Format_ARGB32)
    image.fill(Qt.transparent)
    painter = QPainter(image)
    painter.setRenderHint(QPainter.Antialiasing)
    painter.setRenderHint(QPainter.SmoothPixmapTransform)
    renderer.render(painter)
    painter.end()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(str(out_path))
    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--color", default="#1F95E4", help="cor primaria do icone (#RRGGBB)")
    parser.add_argument("--out", default=str(ROOT), help="raiz do repositorio")
    args = parser.parse_args()
    out = Path(args.out)

    svg = build_svg(args.color)
    (out / "dist" / "azahar.svg").write_text(svg, encoding="utf-8")

    render_png(svg, 512, out / "dist" / "azahar.png")
    for sub in ("icons", "icons_light"):
        render_png(svg, 256, out / "dist" / "qt_themes" / "default" / sub / "256x256" / "azahar.png")

    from PIL import Image

    sizes = [16, 32, 64, 128, 256, 512, 1024]
    tmp = out / "dist" / "apple" / "_icon_tmp"
    images = []
    for s in sizes:
        p = tmp / "{}.png".format(s)
        render_png(svg, s, p)
        images.append(Image.open(p).convert("RGBA"))
    base = images[-1]
    base.save(out / "dist" / "apple" / "azahar.icns", append_images=images[:-1])
    for p in tmp.glob("*.png"):
        p.unlink()
    tmp.rmdir()
    print("Icone gerado com a cor", args.color)


if __name__ == "__main__":
    main()
