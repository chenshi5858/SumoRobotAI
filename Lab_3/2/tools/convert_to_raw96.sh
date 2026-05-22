#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Uso: $0 <imagen_entrada> <salida_raw>"
  echo "Ejemplo: $0 foto.jpg ../person_detection/static_images/sample_images/image0"
  exit 1
fi

input_image="$1"
output_raw="$2"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "Error: ffmpeg no esta instalado o no esta en PATH." >&2
  exit 1
fi

mkdir -p "$(dirname "$output_raw")"

ffmpeg -y \
  -i "$input_image" \
  -vf "scale=96:96:force_original_aspect_ratio=increase,crop=96:96,format=gray" \
  -f rawvideo \
  -pix_fmt gray \
  "$output_raw"

echo "Imagen convertida a raw 96x96 grayscale: $output_raw"
