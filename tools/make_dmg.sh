#!/bin/bash
# Gera um .dmg do Hanami para macOS (compila, monta o .app e empacota).
#
# Uso:   tools/make_dmg.sh [versao]
# Opcoes (variaveis de ambiente):
#   BUILD_DIR=build   usa uma pasta de build ja existente (evita recompilar tudo)
#   ARCH=arm64        arquitetura (padrao: a do seu Mac)
#
# Resultado: dist/Hanami-<versao>-<arch>.dmg
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
ARCH="${ARCH:-$(uname -m)}"
VERSION="${1:-$(git describe --tags --always 2>/dev/null || git rev-parse --short HEAD)}"
BUILD_DIR="${BUILD_DIR:-build/release}"
# Ferramentas da Apple primeiro: o strip do Homebrew quebra o macdeployqt.
APPLE_PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

PATH="$APPLE_PATH" cmake "$ROOT" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DENABLE_FFMPEG=OFF \
    -DENABLE_ROOM_STANDALONE=OFF \
    -DENABLE_QT_UPDATE_CHECKER=OFF
PATH="$APPLE_PATH" ninja
PATH="$APPLE_PATH" ninja bundle

APP="Hanami.app"
rm -rf "$APP"
cp -R bundle/hanami.app "$APP"

# Assinatura "ad hoc": sem conta de desenvolvedor, mas evita o app aparecer como "danificado"
# em Macs com Apple Silicon.
codesign --force --deep --sign - "$APP"

STAGING="$(mktemp -d)"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

mkdir -p "$ROOT/dist"
DMG="$ROOT/dist/Hanami-$VERSION-$ARCH.dmg"
rm -f "$DMG"
hdiutil create -volname "Hanami" -srcfolder "$STAGING" -ov -format UDZO "$DMG"
rm -rf "$STAGING"

echo
echo "Pronto: $DMG"
shasum -a 256 "$DMG"
