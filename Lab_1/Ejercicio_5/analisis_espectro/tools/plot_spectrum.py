#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path

CONFIG_TAG = "SPECTRUM_CFG,"
FRAME_TAG = "SPECTRUM_FRAME,"
LATEST_SPECTROGRAM_FRAMES = 32


def extract_payload(line: str, prefix: str) -> str | None:
    index = line.find(prefix)
    if index < 0:
        return None
    return line[index + len(prefix):].strip()


def parse_log(log_path: Path) -> tuple[dict, list[dict]]:
    config: dict | None = None
    frames: list[dict] = []

    with log_path.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw_line in handle:
            config_payload = extract_payload(raw_line, CONFIG_TAG)
            if config_payload is not None:
                fields = [field.strip() for field in config_payload.split(",")]
                if len(fields) != 5:
                    continue

                config = {
                    "sample_rate_hz": int(fields[0]),
                    "fft_size": int(fields[1]),
                    "bins": int(fields[2]),
                    "bin_resolution_hz": float(fields[3]),
                    "window_name": fields[4],
                }
                continue

            frame_payload = extract_payload(raw_line, FRAME_TAG)
            if frame_payload is None:
                continue

            fields = [field.strip() for field in frame_payload.split(",")]
            if len(fields) < 6:
                continue

            bins = int(fields[4])
            power_values = [float(value) for value in fields[5:]]
            if len(power_values) != bins:
                continue

            frames.append(
                {
                    "timestamp_s": float(fields[0]),
                    "peak_frequency_hz": float(fields[1]),
                    "peak_power_db": float(fields[2]),
                    "window_name": fields[3],
                    "bins": bins,
                    "power_db": power_values,
                }
            )

    if config is None:
        raise SystemExit(f"No encontré una línea {CONFIG_TAG[:-1]} en {log_path}")

    if not frames:
        raise SystemExit(f"No encontré líneas {FRAME_TAG[:-1]} en {log_path}")

    return config, frames


def write_summary_csv(summary_path: Path, frames: list[dict]) -> None:
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    with summary_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "frame_index",
                "timestamp_s",
                "peak_frequency_hz",
                "peak_power_db",
                "window_name",
                "bins",
            ]
        )
        for frame_index, frame in enumerate(frames):
            writer.writerow(
                [
                    frame_index,
                    f"{frame['timestamp_s']:.6f}",
                    f"{frame['peak_frequency_hz']:.6f}",
                    f"{frame['peak_power_db']:.6f}",
                    frame["window_name"],
                    frame["bins"],
                ]
            )


def write_bins_csv(bins_path: Path, config: dict, frames: list[dict]) -> None:
    bins_path.parent.mkdir(parents=True, exist_ok=True)
    with bins_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "frame_index",
                "timestamp_s",
                "bin_index",
                "frequency_hz",
                "power_db",
                "peak_frequency_hz",
                "peak_power_db",
                "window_name",
            ]
        )

        for frame_index, frame in enumerate(frames):
            for bin_index, power_db in enumerate(frame["power_db"]):
                frequency_hz = bin_index * config["bin_resolution_hz"]
                writer.writerow(
                    [
                        frame_index,
                        f"{frame['timestamp_s']:.6f}",
                        bin_index,
                        f"{frequency_hz:.6f}",
                        f"{power_db:.6f}",
                        f"{frame['peak_frequency_hz']:.6f}",
                        f"{frame['peak_power_db']:.6f}",
                        frame["window_name"],
                    ]
                )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Extrae espectros del monitor serial, genera CSV y exporta espectrogramas PNG."
    )
    parser.add_argument("log_path", type=Path, help="Archivo generado con tee a partir de idf.py monitor.")
    parser.add_argument(
        "--summary-csv",
        type=Path,
        default=Path("exports/spectrum_summary.csv"),
        help="CSV resumido con un registro por captura.",
    )
    parser.add_argument(
        "--bins-csv",
        type=Path,
        default=Path("exports/spectrum_bins.csv"),
        help="CSV detallado con un registro por bin de frecuencia.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("exports/plots"),
        help="Carpeta donde se guardan los espectrogramas PNG.",
    )
    parser.add_argument(
        "--latest-only",
        action="store_true",
        help="Genera solo el espectrograma de la ventana mas reciente.",
    )
    parser.add_argument(
        "--csv-only",
        action="store_true",
        help="Genera solo los CSV y omite los PNG.",
    )
    parser.add_argument(
        "--clean-existing-plots",
        action="store_true",
        help="Elimina los PNG existentes antes de generar nuevos espectrogramas.",
    )
    return parser


def clear_previous_plots(output_dir: Path) -> int:
    output_dir.mkdir(parents=True, exist_ok=True)

    removed = 0
    for pattern in ("spectrum_*.png", "spectrogram_*.png"):
        for plot_path in output_dir.glob(pattern):
            if plot_path.is_file():
                plot_path.unlink()
                removed += 1

    return removed


def build_power_matrix(frames: list[dict]) -> list[list[float]]:
    return [
        [frame["power_db"][bin_index] for frame in frames]
        for bin_index in range(frames[0]["bins"])
    ]


def compute_time_extent(frames: list[dict]) -> tuple[float, float]:
    if len(frames) == 1:
        start = frames[0]["timestamp_s"]
        return start, start + 1.0

    deltas = [
        frames[index]["timestamp_s"] - frames[index - 1]["timestamp_s"]
        for index in range(1, len(frames))
    ]
    median_delta = statistics.median(deltas)
    return frames[0]["timestamp_s"], frames[-1]["timestamp_s"] + median_delta


def sanitize_float_label(value: float) -> str:
    return f"{value:.2f}".replace(".", "p")


def build_output_path(output_dir: Path, stem: str) -> Path:
    candidate = output_dir / f"{stem}.png"
    if not candidate.exists():
        return candidate

    index = 1
    while True:
        candidate = output_dir / f"{stem}_{index:02d}.png"
        if not candidate.exists():
            return candidate
        index += 1


def render_spectrogram(output_path: Path, config: dict, frames: list[dict], title: str) -> None:
    import matplotlib.pyplot as plt

    power_matrix = build_power_matrix(frames)
    time_start, time_end = compute_time_extent(frames)
    freq_max_hz = (config["bins"] - 1) * config["bin_resolution_hz"]
    peak_times = [frame["timestamp_s"] for frame in frames]
    peak_frequencies = [frame["peak_frequency_hz"] for frame in frames]
    min_power_db = min(min(frame["power_db"]) for frame in frames)
    max_power_db = max(max(frame["power_db"]) for frame in frames)

    figure, axis = plt.subplots(figsize=(11, 6), dpi=150)
    image = axis.imshow(
        power_matrix,
        aspect="auto",
        origin="lower",
        interpolation="nearest",
        extent=[time_start, time_end, 0.0, freq_max_hz],
        cmap="magma",
        vmin=min_power_db,
        vmax=max_power_db,
    )
    axis.plot(peak_times, peak_frequencies, color="#3dd5f3", linewidth=1.2, label="Pico dominante")
    axis.set_title(title)
    axis.set_xlabel("Tiempo [s]")
    axis.set_ylabel("Frecuencia [Hz]")
    axis.legend(loc="upper right")
    colorbar = figure.colorbar(image, ax=axis)
    colorbar.set_label("Potencia [dB]")
    figure.tight_layout()
    figure.savefig(output_path)
    plt.close(figure)


def save_spectrograms(
    output_dir: Path,
    config: dict,
    frames: list[dict],
    latest_only: bool,
    clean_existing_plots: bool,
) -> int:
    try:
        import matplotlib

        matplotlib.use("Agg")
    except ImportError as exc:
        raise SystemExit(
            "matplotlib no esta instalado. Instala matplotlib para generar el espectrograma, "
            "o usa --csv-only para extraer solo los CSV."
        ) from exc

    removed = 0
    if clean_existing_plots:
        removed = clear_previous_plots(output_dir)
    else:
        output_dir.mkdir(parents=True, exist_ok=True)

    recent_frames = frames[-min(LATEST_SPECTROGRAM_FRAMES, len(frames)) :]
    sample_rate_label = f"fs={config['sample_rate_hz']} Hz"
    full_range_label = (
        f"t{sanitize_float_label(frames[0]['timestamp_s'])}_"
        f"{sanitize_float_label(frames[-1]['timestamp_s'])}"
    )
    recent_range_label = (
        f"t{sanitize_float_label(recent_frames[0]['timestamp_s'])}_"
        f"{sanitize_float_label(recent_frames[-1]['timestamp_s'])}"
    )

    if not latest_only:
        full_output_path = build_output_path(
            output_dir,
            f"spectrogram_full_fs{config['sample_rate_hz']}_{full_range_label}",
        )
        render_spectrogram(
            full_output_path,
            config,
            frames,
            (
                f"Espectrograma completo | {len(frames)} capturas"
                f" | ventana={config['window_name']} | {sample_rate_label}"
            ),
        )

    latest_output_path = build_output_path(
        output_dir,
        f"spectrogram_latest_fs{config['sample_rate_hz']}_{recent_range_label}",
    )
    render_spectrogram(
        latest_output_path,
        config,
        recent_frames,
        (
            f"Espectrograma reciente | ultimas {len(recent_frames)} capturas"
            f" | ventana={config['window_name']} | {sample_rate_label}"
        ),
    )

    return removed


def main() -> None:
    args = build_parser().parse_args()
    config, frames = parse_log(args.log_path)

    write_summary_csv(args.summary_csv, frames)
    write_bins_csv(args.bins_csv, config, frames)

    removed_plots = 0
    if not args.csv_only:
        removed_plots = save_spectrograms(
            args.output_dir,
            config,
            frames,
            args.latest_only,
            args.clean_existing_plots,
        )

    print(f"Capturas procesadas: {len(frames)}")
    print(f"Resumen CSV: {args.summary_csv}")
    print(f"Bins CSV: {args.bins_csv}")
    if not args.csv_only:
        if args.clean_existing_plots:
            print(f"PNG anteriores eliminados: {removed_plots}")
        print(f"Espectrogramas PNG: {args.output_dir}")


if __name__ == "__main__":
    main()
