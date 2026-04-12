#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ANSI_ESCAPE_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")

METRICS_HEADERS = [
    "label",
    "X",
    "time_us",
    "cycles",
    "cycles_per_iter",
    "time_from_cycles_us",
]

TOTAL_HEADERS = [
    "label",
    "runs",
    "total_iterations",
    "total_time_us",
    "total_cycles",
    "avg_cycles_per_run",
    "avg_cycles_per_iter",
    "total_time_from_cycles_us",
]

END_TO_END_HEADERS = [
    "label",
    "total_time_us",
    "total_cycles",
    "total_time_from_cycles_us",
]

CHECK_HEADERS = [
    "X",
    "status",
]

META_HEADERS = [
    "key",
    "value",
]


def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE_RE.sub("", text).strip()


def dedupe_rows(rows: list[dict[str, str]], headers: list[str]) -> list[dict[str, str]]:
    seen = set()
    unique_rows = []

    for row in rows:
        key = tuple(row.get(header, "") for header in headers)
        if key in seen:
            continue
        seen.add(key)
        unique_rows.append(row)

    return unique_rows


def find_latest_log(project_dir: Path) -> Path:
    candidates = []
    for build_name in ("build", "build_codex"):
        log_dir = project_dir / build_name / "log"
        if not log_dir.is_dir():
            continue
        for path in log_dir.glob("idf_py_stdout_output_*"):
            if path.is_file() and path.stat().st_size > 0:
                candidates.append(path)

    if not candidates:
        raise FileNotFoundError(
            "No se encontraron logs idf_py_stdout_output_* en build/log ni build_codex/log."
        )

    candidates.sort(key=lambda path: path.stat().st_mtime, reverse=True)
    for path in candidates:
        content = path.read_text(encoding="utf-8", errors="replace")
        if "CSV," in content:
            return path

    raise FileNotFoundError(
        "Se encontraron logs, pero ninguno contiene lineas CSV exportables."
    )


def parse_log(log_path: Path) -> dict[str, list[dict[str, str]]]:
    data: dict[str, list[dict[str, str]]] = {
        "metrics": [],
        "totals": [],
        "end_to_end": [],
        "checks": [],
        "metadata": [],
    }

    cpu_mhz = None

    for raw_line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = strip_ansi(raw_line)
        if not line:
            continue

        parts = [part.strip() for part in line.split(",")]
        tag = parts[0]

        if tag == "CPU_MHZ" and len(parts) == 2:
            cpu_mhz = parts[1]
            continue

        if tag == "CSV" and len(parts) == 7:
            data["metrics"].append(
                {
                    "label": parts[1],
                    "X": parts[2],
                    "time_us": parts[3],
                    "cycles": parts[4],
                    "cycles_per_iter": parts[5],
                    "time_from_cycles_us": parts[6],
                }
            )
            continue

        if tag == "TOTAL_CSV" and len(parts) == 9:
            data["totals"].append(
                {
                    "label": parts[1],
                    "runs": parts[2],
                    "total_iterations": parts[3],
                    "total_time_us": parts[4],
                    "total_cycles": parts[5],
                    "avg_cycles_per_run": parts[6],
                    "avg_cycles_per_iter": parts[7],
                    "total_time_from_cycles_us": parts[8],
                }
            )
            continue

        if tag == "END_TO_END" and len(parts) == 5:
            data["end_to_end"].append(
                {
                    "label": parts[1],
                    "total_time_us": parts[2],
                    "total_cycles": parts[3],
                    "total_time_from_cycles_us": parts[4],
                }
            )
            continue

        if tag == "CHECK" and len(parts) == 3:
            data["checks"].append(
                {
                    "X": parts[1],
                    "status": parts[2],
                }
            )

    if cpu_mhz is not None:
        data["metadata"].append({"key": "cpu_mhz", "value": cpu_mhz})

    data["metrics"] = dedupe_rows(data["metrics"], METRICS_HEADERS)
    data["totals"] = dedupe_rows(data["totals"], TOTAL_HEADERS)
    data["end_to_end"] = dedupe_rows(data["end_to_end"], END_TO_END_HEADERS)
    data["checks"] = dedupe_rows(data["checks"], CHECK_HEADERS)

    data["metadata"].append({"key": "source_log", "value": str(log_path)})
    data["metadata"].append({"key": "metrics_rows", "value": str(len(data["metrics"]))})
    data["metadata"].append({"key": "totals_rows", "value": str(len(data["totals"]))})
    data["metadata"].append({"key": "end_to_end_rows", "value": str(len(data["end_to_end"]))})
    data["metadata"].append({"key": "checks_rows", "value": str(len(data["checks"]))})

    return data


def write_csv(path: Path, headers: list[str], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=headers)
        writer.writeheader()
        writer.writerows(rows)


def export_csvs(project_dir: Path, log_path: Path, output_dir: Path) -> list[Path]:
    parsed = parse_log(log_path)

    if not parsed["metrics"] and not parsed["totals"] and not parsed["end_to_end"]:
        raise ValueError(
            f"El log '{log_path}' no contiene datos CSV reconocibles de Ejercicio_3."
        )

    files_to_write = [
        ("metrics.csv", METRICS_HEADERS, parsed["metrics"]),
        ("totals.csv", TOTAL_HEADERS, parsed["totals"]),
        ("end_to_end.csv", END_TO_END_HEADERS, parsed["end_to_end"]),
        ("checks.csv", CHECK_HEADERS, parsed["checks"]),
        ("metadata.csv", META_HEADERS, parsed["metadata"]),
    ]

    written_paths = []
    for filename, headers, rows in files_to_write:
        output_path = output_dir / filename
        write_csv(output_path, headers, rows)
        written_paths.append(output_path.relative_to(project_dir))

    return written_paths


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent

    parser = argparse.ArgumentParser(
        description="Exporta la salida serial de Ejercicio_3 a archivos CSV en columnas."
    )
    parser.add_argument(
        "--input",
        type=Path,
        help="Ruta al log idf_py_stdout_output_* a convertir. Si se omite, usa el log mas reciente con datos CSV.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=script_dir / "csv_output",
        help="Directorio donde se escribiran los CSV exportados.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = Path(__file__).resolve().parent

    try:
        log_path = args.input.resolve() if args.input else find_latest_log(project_dir)
        output_dir = args.output_dir.resolve()
        written_paths = export_csvs(project_dir, log_path, output_dir)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"Log usado: {log_path}")
    print("Archivos generados:")
    for path in written_paths:
        print(f"- {path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
