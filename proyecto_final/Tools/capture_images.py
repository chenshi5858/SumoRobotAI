"""Capture absent-identifier images from ESP32-CAM over its serial port.

Every successful capture is saved as a 96x96 grayscale PNG and immediately
added to etiquetas.txt with the presence label 0 (identifier absent).
"""

import argparse
import os
import re
import sys
import time
from pathlib import Path

import serial
from PIL import Image


IMG_W = 96
IMG_H = 96
IMAGE_BYTES = IMG_W * IMG_H
IMAGE_NAME_RE = re.compile(r"^esp_(\d+)\.png$", re.IGNORECASE)
WINDOWS_TRAINING_ROOT = Path(
    r"C:\Users\chens\OneDrive\Escritorio\lab2embebidos\lab2embebidos"
)
WSL_TRAINING_ROOT = Path(
    "/mnt/c/Users/chens/OneDrive/Escritorio/lab2embebidos/lab2embebidos"
)


def default_output_dir() -> Path:
    training_root = WINDOWS_TRAINING_ROOT if os.name == "nt" else WSL_TRAINING_ROOT
    return training_root / "dataset_grupo_99" / "esp"


def find_serial_port(hint: str | None = None) -> str:
    import serial.tools.list_ports

    ports = list(serial.tools.list_ports.comports())
    if not ports:
        raise SystemExit("No serial ports found.")
    if hint:
        for port in ports:
            if hint in port.device:
                return port.device
    print("Available ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")
    return ports[0].device


def wait_for_prompt(ser: serial.Serial, timeout: float = 30.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            line = ser.readline()
        except serial.SerialException:
            time.sleep(0.5)
            continue
        if not line:
            continue
        text = line.rstrip().decode(errors="replace")
        if text:
            print(f"  {text}")
        if "Send 'c' via serial" in text:
            return
    raise SystemExit("Timed out waiting for ESP32-CAM capture prompt.")


def read_exactly(ser: serial.Serial, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = ser.read(size - len(data))
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def existing_label_names(labels_path: Path) -> set[str]:
    if not labels_path.exists():
        return set()
    names: set[str] = set()
    for line in labels_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        parts = [part.strip() for part in line.split(",")]
        if parts and parts[0]:
            names.add(parts[0])
    return names


def repair_missing_absent_labels(output_dir: Path, labels_path: Path) -> int:
    """Add class-0 rows for PNG files left unlabeled by an interrupted run."""
    known_names = existing_label_names(labels_path)
    missing = sorted(
        path.name
        for path in output_dir.glob("esp_*.png")
        if IMAGE_NAME_RE.match(path.name) and path.name not in known_names
    )
    if not missing:
        return 0
    with labels_path.open("a", encoding="utf-8", newline="\n") as labels_file:
        for name in missing:
            labels_file.write(f"{name}, 0, 0\n")
    return len(missing)


def next_image_number(output_dir: Path) -> int:
    numbers = []
    for path in output_dir.glob("esp_*.png"):
        match = IMAGE_NAME_RE.match(path.name)
        if match:
            numbers.append(int(match.group(1)))
    return max(numbers, default=0) + 1


def save_absent_capture(raw: bytes, output_dir: Path, number: int) -> Path:
    filename = f"esp_{number:04d}.png"
    image_path = output_dir / filename
    labels_path = output_dir / "etiquetas.txt"

    Image.frombytes("L", (IMG_W, IMG_H), raw).save(image_path, "PNG")
    with labels_path.open("a", encoding="utf-8", newline="\n") as labels_file:
        labels_file.write(f"{filename}, 0, 0\n")
    return image_path


def capture_images(
    port: str,
    baud: int,
    output_dir: Path,
    count: int,
    interval: float,
    manual: bool,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    labels_path = output_dir / "etiquetas.txt"
    labels_path.touch(exist_ok=True)
    repaired = repair_missing_absent_labels(output_dir, labels_path)
    if repaired:
        print(f"Recovered {repaired} missing class-0 label rows.")

    number = next_image_number(output_dir)
    saved = 0
    ser = serial.Serial(port, baud, timeout=5)
    try:
        ser.dtr = False
        ser.rts = False
        time.sleep(0.2)
        ser.reset_input_buffer()
        print(f"Connected to {port} at {baud} baud")
        print("Waiting for ESP32-CAM to boot...")
        wait_for_prompt(ser)
        print(f"Dataset directory: {output_dir}")
        print("Automatic label for every capture: identifier absent (0, 0)")

        for capture_index in range(count):
            if manual:
                input(f"\n[{capture_index + 1}/{count}] Press Enter to capture...")
            ser.reset_input_buffer()
            ser.write(b"c")
            ser.flush()
            print(f"\n[{capture_index + 1}/{count}] Capture triggered...")

            image_size = None
            while image_size is None:
                line = ser.readline()
                if not line:
                    continue
                text = line.rstrip().decode(errors="replace")
                if text:
                    print(f"  {text}")
                if line.startswith(b"IMG_START "):
                    parts = line.strip().split()
                    if len(parts) >= 2:
                        try:
                            image_size = int(parts[1])
                        except ValueError:
                            image_size = None

            if image_size != IMAGE_BYTES:
                print(f"  ERROR: expected {IMAGE_BYTES} grayscale bytes, ESP sent {image_size}")
                read_exactly(ser, image_size)
                continue

            raw = read_exactly(ser, image_size)
            if len(raw) != image_size:
                print(f"  ERROR: expected {image_size} bytes, received {len(raw)}")
                continue

            image_path = save_absent_capture(raw, output_dir, number)
            print(f"  Saved and labeled: {image_path.name}, 0, 0")
            number += 1
            saved += 1

            if not manual and interval > 0 and capture_index + 1 < count:
                time.sleep(interval)
    except KeyboardInterrupt:
        print("\nCapture stopped by user.")
    finally:
        ser.close()

    print(f"\nDone: {saved} images saved in {output_dir}")
    print(f"Labels: {labels_path}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Capture ESP32-CAM images labeled automatically as identifier absent."
    )
    parser.add_argument("--port", "-p", default=None, help="Serial port, e.g. /dev/ttyUSB0 or COM5")
    parser.add_argument("--baud", "-b", type=int, default=115200)
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=default_output_dir(),
        help="ESP dataset folder containing images and etiquetas.txt",
    )
    parser.add_argument("--count", "-n", type=int, default=1)
    parser.add_argument(
        "--interval",
        type=float,
        default=0.5,
        help="Seconds between automatic captures (default: 0.5)",
    )
    parser.add_argument(
        "--manual",
        action="store_true",
        help="Wait for Enter before each capture so the scene can be changed.",
    )
    args = parser.parse_args()

    if args.count < 1:
        parser.error("--count must be at least 1")
    if args.interval < 0:
        parser.error("--interval cannot be negative")

    port = args.port or find_serial_port()
    capture_images(port, args.baud, args.output.expanduser(), args.count, args.interval, args.manual)


if __name__ == "__main__":
    main()
