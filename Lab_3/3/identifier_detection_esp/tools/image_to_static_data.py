#!/usr/bin/env python3
"""Convert images into main/static_image_data.cc for ESP inference."""

from __future__ import annotations

import argparse
from pathlib import Path


IMAGE_SIZE = 96
DEFAULT_OUTPUT = Path("main/static_image_data.cc")
SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".raw"}


def list_images(images_dir: Path) -> list[Path]:
    if not images_dir.exists():
        raise SystemExit(f"No existe la carpeta: {images_dir}")
    images = [
        path
        for path in images_dir.iterdir()
        if path.is_file() and path.suffix.lower() in SUPPORTED_EXTENSIONS
    ]
    if not images:
        raise SystemExit(f"No hay imagenes en: {images_dir}")
    return sorted(images)


def collect_images(image_or_dir: Path | None, default_dir: Path) -> list[Path]:
    if image_or_dir is None:
        return list_images(default_dir)
    if image_or_dir.is_dir():
        return list_images(image_or_dir)
    if image_or_dir.is_file():
        return [image_or_dir]
    raise SystemExit(f"No existe: {image_or_dir}")


def sanitize_name(name: str) -> str:
    return "".join(ch if 32 <= ord(ch) < 127 else "_" for ch in name)


def format_array(values: bytes, indent: str = "        ") -> str:
    lines = []
    for i in range(0, len(values), 16):
        chunk = ", ".join(f"{value:3d}" for value in values[i : i + 16])
        lines.append(f"{indent}{chunk},")
    return "\n".join(lines)


def load_image_bytes(path: Path) -> bytes:
    if path.suffix.lower() == ".raw":
        values = path.read_bytes()
        if len(values) != IMAGE_SIZE * IMAGE_SIZE:
            raise SystemExit(f"Invalid raw size for {path}: {len(values)} bytes")
        return values

    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Falta Pillow. Instala con: pip install Pillow") from exc

    with Image.open(path) as img:
        img = img.convert("L").resize((IMAGE_SIZE, IMAGE_SIZE), Image.Resampling.BILINEAR)
        values = img.tobytes()

    if len(values) != IMAGE_SIZE * IMAGE_SIZE:
        raise SystemExit(f"Unexpected image size for {path}: {len(values)} bytes")
    return values


def write_output(images: list[Path], output: Path) -> None:
    lines = [
        '#include "static_image_data.h"',
        "",
        f"const size_t g_static_image_count = {len(images)};",
        "",
        "const char* g_static_image_names[] = {",
    ]

    for image_path in images:
        lines.append(f"    \"{sanitize_name(image_path.name)}\",")
    lines.append("};")
    lines.append("")
    lines.append("const uint8_t g_static_images[][kImageElementCount] = {")

    for image_path in images:
        safe_name = sanitize_name(image_path.name)
        lines.append(f"    // {safe_name}")
        lines.append("    {")
        lines.append(format_array(load_image_bytes(image_path)))
        lines.append("    },")

    lines.append("};")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {output} with {len(images)} images")


def main() -> None:
    default_images = Path(__file__).resolve().parents[1] / "raw_images"
    parser = argparse.ArgumentParser(description="Convert images into static_image_data.cc")
    parser.add_argument("image_or_dir", type=Path, nargs="?", help="Imagen o carpeta de imagenes")
    parser.add_argument("output", type=Path, nargs="?", default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    images = collect_images(args.image_or_dir, default_images)
    write_output(images, args.output)


if __name__ == "__main__":
    main()
