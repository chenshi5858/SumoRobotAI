#!/usr/bin/env python3
import csv
import re
import sys
from pathlib import Path


DIRECT_RE = re.compile(
    r"direct,"
    r"(?P<x>[-+0-9.eE]+),"
    r"(?P<y>[-+0-9.eE]+),"
    r"(?P<sin>[-+0-9.eE]+),"
    r"(?P<us>[-+0-9.eE]+),"
    r"(?P<qin>-?\d+),"
    r"(?P<qout>-?\d+)"
)

TFMICRO_RE = re.compile(
    r"x_value:\s*(?P<x>[-+0-9.eE]+),\s*y_value:\s*(?P<y>[-+0-9.eE]+)"
)


def parse_direct(path):
    rows = []
    for line in Path(path).read_text(encoding="utf-8", errors="ignore").splitlines():
        match = DIRECT_RE.search(line)
        if match:
            rows.append({
                "x": float(match.group("x")),
                "y": float(match.group("y")),
                "sin": float(match.group("sin")),
                "avg_us": float(match.group("us")),
                "q_input": int(match.group("qin")),
                "q_output": int(match.group("qout")),
            })
    return rows


def parse_tfmicro(path):
    rows = []
    for line in Path(path).read_text(encoding="utf-8", errors="ignore").splitlines():
        match = TFMICRO_RE.search(line)
        if match:
            rows.append({
                "x": float(match.group("x")),
                "y": float(match.group("y")),
            })
    return rows


def write_csv(rows, output_path):
    fieldnames = [
        "index",
        "x_direct",
        "y_direct",
        "sin_reference",
        "direct_avg_us",
        "x_tfmicro",
        "y_tfmicro",
        "delta_direct_minus_tfmicro",
    ]
    with Path(output_path).open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def maybe_plot(rows, output_path):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        return False

    x_direct = [row["x_direct"] for row in rows]
    y_direct = [row["y_direct"] for row in rows]
    y_sin = [row["sin_reference"] for row in rows]
    x_tfmicro = [row["x_tfmicro"] for row in rows if row["x_tfmicro"] != ""]
    y_tfmicro = [row["y_tfmicro"] for row in rows if row["y_tfmicro"] != ""]

    plt.figure(figsize=(8, 4.5))
    plt.plot(x_direct, y_sin, label="sin(x)", linewidth=2)
    plt.plot(x_direct, y_direct, "o-", label="direct C")
    if x_tfmicro:
        plt.plot(x_tfmicro, y_tfmicro, "x--", label="TF Micro")
    plt.xlabel("x")
    plt.ylabel("y")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=160)
    return True


def main():
    if len(sys.argv) not in (2, 3):
        print("uso: python3 tools/compare_outputs.py direct.log [tfmicro.log]")
        return 1

    direct_rows = parse_direct(sys.argv[1])
    tfmicro_rows = parse_tfmicro(sys.argv[2]) if len(sys.argv) == 3 else []

    if not direct_rows:
        print("no se encontraron filas direct en el log")
        return 1

    count = max(len(direct_rows), len(tfmicro_rows))
    rows = []
    for index in range(count):
        direct = direct_rows[index % len(direct_rows)]
        tfmicro = tfmicro_rows[index] if index < len(tfmicro_rows) else None
        y_tfmicro = tfmicro["y"] if tfmicro else ""
        rows.append({
            "index": index,
            "x_direct": direct["x"],
            "y_direct": direct["y"],
            "sin_reference": direct["sin"],
            "direct_avg_us": direct["avg_us"],
            "x_tfmicro": tfmicro["x"] if tfmicro else "",
            "y_tfmicro": y_tfmicro,
            "delta_direct_minus_tfmicro": direct["y"] - y_tfmicro if tfmicro else "",
        })

    write_csv(rows, "comparison.csv")
    plotted = maybe_plot(rows, "comparison.png")

    print(f"filas directas: {len(direct_rows)}")
    print(f"filas TF Micro: {len(tfmicro_rows)}")
    print("generado: comparison.csv")
    if plotted:
        print("generado: comparison.png")
    else:
        print("matplotlib no esta instalado; se omitio comparison.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
