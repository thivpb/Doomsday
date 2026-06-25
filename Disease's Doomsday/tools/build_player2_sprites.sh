#!/usr/bin/env bash
# build_player2_sprites.sh
# Gera as folhas de sprite transparentes do personagem "ANTICORPO-V" a partir dos
# JPEGs originais (fundo preto). Reprodutível: rode sempre que os JPEGs mudarem.
#
#   1) sips decodifica JPEG -> PNG temporário (raylib do Homebrew não lê JPG).
#   2) tools/key_player2_sprites chaveia o fundo preto -> transparente.
#
# Os JPEGs originais NÃO são apagados. Saída em Assets/Sprites/Player/.
set -euo pipefail

# Raiz do jogo (este script vive em <raiz>/tools/).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT/../../Sprite Main character"   # pasta irmã com os JPEGs originais
OUT_DIR="$ROOT/Assets/Sprites/Player"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

# Thresholds da chave de transparência (calibráveis).
THR_LOW="${THR_LOW:-16}"
THR_HIGH="${THR_HIGH:-48}"

# Flags raylib (macOS/Homebrew). Espelha o branch Darwin do Makefile.
RAYLIB_CFLAGS="-I/opt/homebrew/include -I/usr/local/include"
RAYLIB_LIBS="-L/opt/homebrew/lib -L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo -lm"

echo "[1/3] Compilando o keyer..."
cc -O2 -Wall $RAYLIB_CFLAGS "$ROOT/tools/key_player2_sprites.c" -o "$TMP_DIR/keyer" $RAYLIB_LIBS

# Mapeamento: <jpeg de origem> -> <png de destino>
declare -a PAIRS=(
  "sprite personagem parado animações.jpeg|anticorpo_v_idle.png"
  "sprite personagem andando.jpeg|anticorpo_v_walk.png"
  "sprite personagem tomando dano.jpeg|anticorpo_v_hurt.png"
)

echo "[2/3] Decodificando JPEG -> PNG (sips) e chaveando preto -> alpha..."
mkdir -p "$OUT_DIR"
for pair in "${PAIRS[@]}"; do
  IFS='|' read -r src dst <<< "$pair"
  in_jpeg="$SRC_DIR/$src"
  tmp_png="$TMP_DIR/${dst}"
  out_png="$OUT_DIR/$dst"
  if [[ ! -f "$in_jpeg" ]]; then
    echo "[ERRO] JPEG ausente: $in_jpeg" >&2
    exit 1
  fi
  sips -s format png "$in_jpeg" --out "$tmp_png" >/dev/null
  "$TMP_DIR/keyer" "$tmp_png" "$out_png" "$THR_LOW" "$THR_HIGH"
done

echo "[3/3] Concluido. PNGs transparentes em: $OUT_DIR"
ls -la "$OUT_DIR"/anticorpo_v_*.png
