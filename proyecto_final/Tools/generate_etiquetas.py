"""Rebuild etiquetas.txt for a folder containing only absent examples."""

import argparse
from pathlib import Path


IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}


def generate(input_dir: Path, output_file: Path) -> None:
    image_files = sorted(
        path for path in input_dir.iterdir() if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
    )
    if not image_files:
        raise SystemExit(f"No supported image files found in {input_dir}")

    rows = [f"{path.name}, 0, 0" for path in image_files]
    output_file.write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(f"Generated {output_file} with {len(rows)} absent-label entries")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate class-0 etiquetas.txt for absent-identifier images."
    )
    parser.add_argument("--input", "-i", type=Path, required=True)
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help="Default: <input>/etiquetas.txt",
    )
    args = parser.parse_args()
    input_dir = args.input.expanduser()
    if not input_dir.is_dir():
        parser.error(f"input directory does not exist: {input_dir}")
    generate(input_dir, args.output.expanduser() if args.output else input_dir / "etiquetas.txt")


if __name__ == "__main__":
    main()
