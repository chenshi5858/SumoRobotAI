#!/usr/bin/env python3
import csv
import re
import sys
from pathlib import Path


MODEL_CSV_RE = re.compile(
    r"(?P<source>direct|tfmicro),"
    r"(?P<x>[-+0-9.eE]+),"
    r"(?P<y>[-+0-9.eE]+),"
    r"(?P<sin>[-+0-9.eE]+),"
    r"(?P<us>[-+0-9.eE]+),"
    r"(?P<qin>-?\d+),"
    r"(?P<qout>-?\d+)"
)

# Compatibility with the stock hello_world example log format.
TFMICRO_RE = re.compile(
    r"x_value:\s*(?P<x>[-+0-9.eE]+),\s*y_value:\s*(?P<y>[-+0-9.eE]+)"
)


def parse_model_csv(path, source):
    rows = []
    for line in Path(path).read_text(encoding="utf-8", errors="ignore").splitlines():
        match = MODEL_CSV_RE.search(line)
        if match and match.group("source") == source:
            rows.append({
                "x": float(match.group("x")),
                "y": float(match.group("y")),
                "sin": float(match.group("sin")),
                "avg_us": float(match.group("us")),
                "q_input": int(match.group("qin")),
                "q_output": int(match.group("qout")),
            })
    return rows


def parse_direct(path):
    return parse_model_csv(path, "direct")


def parse_tfmicro(path):
    rows = parse_model_csv(path, "tfmicro")
    if rows:
        return rows

    for line in Path(path).read_text(encoding="utf-8", errors="ignore").splitlines():
        match = TFMICRO_RE.search(line)
        if match:
            rows.append({
                "x": float(match.group("x")),
                "y": float(match.group("y")),
                "sin": "",
                "avg_us": "",
                "q_input": "",
                "q_output": "",
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
        "tfmicro_avg_us",
        "delta_direct_minus_tfmicro",
        "delta_avg_us_direct_minus_tfmicro",
    ]
    with Path(output_path).open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def maybe_plot_png(rows, output_path):
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


def write_svg(rows, output_path):
    width = 900
    height = 520
    margin_left = 70
    margin_right = 30
    margin_top = 30
    margin_bottom = 65
    plot_width = width - margin_left - margin_right
    plot_height = height - margin_top - margin_bottom

    series = [
        ("sin(x)", "#333333", [(row["x_direct"], row["sin_reference"]) for row in rows]),
        ("direct C", "#0072b2", [(row["x_direct"], row["y_direct"]) for row in rows]),
        ("TF Micro", "#d55e00", [
            (row["x_tfmicro"], row["y_tfmicro"])
            for row in rows
            if row["x_tfmicro"] != "" and row["y_tfmicro"] != ""
        ]),
    ]
    points = [point for _, _, values in series for point in values]
    x_values = [point[0] for point in points]
    y_values = [point[1] for point in points]
    x_min = min(x_values)
    x_max = max(x_values)
    y_min = min(y_values)
    y_max = max(y_values)
    y_padding = max((y_max - y_min) * 0.08, 0.1)
    y_min -= y_padding
    y_max += y_padding

    def sx(x):
        return margin_left + ((x - x_min) / (x_max - x_min)) * plot_width

    def sy(y):
        return margin_top + ((y_max - y) / (y_max - y_min)) * plot_height

    def polyline(values):
        return " ".join(f"{sx(x):.1f},{sy(y):.1f}" for x, y in values)

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width / 2:.0f}" y="22" text-anchor="middle" font-family="Arial" font-size="16">Salida hello_world: directo vs TF Micro</text>',
        f'<line x1="{margin_left}" y1="{margin_top + plot_height}" x2="{margin_left + plot_width}" y2="{margin_top + plot_height}" stroke="#222"/>',
        f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_height}" stroke="#222"/>',
    ]

    for tick in range(6):
        x = x_min + (x_max - x_min) * tick / 5
        x_pos = sx(x)
        lines.append(f'<line x1="{x_pos:.1f}" y1="{margin_top + plot_height}" x2="{x_pos:.1f}" y2="{margin_top + plot_height + 5}" stroke="#222"/>')
        lines.append(f'<text x="{x_pos:.1f}" y="{margin_top + plot_height + 24}" text-anchor="middle" font-family="Arial" font-size="12">{x:.2f}</text>')

    for tick in range(5):
        y = y_min + (y_max - y_min) * tick / 4
        y_pos = sy(y)
        lines.append(f'<line x1="{margin_left - 5}" y1="{y_pos:.1f}" x2="{margin_left}" y2="{y_pos:.1f}" stroke="#222"/>')
        lines.append(f'<line x1="{margin_left}" y1="{y_pos:.1f}" x2="{margin_left + plot_width}" y2="{y_pos:.1f}" stroke="#dddddd"/>')
        lines.append(f'<text x="{margin_left - 10}" y="{y_pos + 4:.1f}" text-anchor="end" font-family="Arial" font-size="12">{y:.2f}</text>')

    for name, color, values in series:
        if values:
            lines.append(f'<polyline points="{polyline(values)}" fill="none" stroke="{color}" stroke-width="2.4"/>')

    legend_x = margin_left + plot_width - 170
    legend_y = margin_top + 18
    for index, (name, color, _) in enumerate(series):
        y = legend_y + index * 22
        lines.append(f'<line x1="{legend_x}" y1="{y}" x2="{legend_x + 28}" y2="{y}" stroke="{color}" stroke-width="3"/>')
        lines.append(f'<text x="{legend_x + 36}" y="{y + 4}" font-family="Arial" font-size="13">{name}</text>')

    lines.append(f'<text x="{margin_left + plot_width / 2:.1f}" y="{height - 18}" text-anchor="middle" font-family="Arial" font-size="13">x</text>')
    lines.append(f'<text x="18" y="{margin_top + plot_height / 2:.1f}" text-anchor="middle" font-family="Arial" font-size="13" transform="rotate(-90 18 {margin_top + plot_height / 2:.1f})">y</text>')
    lines.append("</svg>")

    Path(output_path).write_text("\n".join(lines), encoding="utf-8")


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
        direct_avg_us = direct["avg_us"]
        tfmicro_avg_us = tfmicro["avg_us"] if tfmicro else ""
        rows.append({
            "index": index,
            "x_direct": direct["x"],
            "y_direct": direct["y"],
            "sin_reference": direct["sin"],
            "direct_avg_us": direct_avg_us,
            "x_tfmicro": tfmicro["x"] if tfmicro else "",
            "y_tfmicro": y_tfmicro,
            "tfmicro_avg_us": tfmicro_avg_us,
            "delta_direct_minus_tfmicro": direct["y"] - y_tfmicro if tfmicro else "",
            "delta_avg_us_direct_minus_tfmicro": (
                direct_avg_us - tfmicro_avg_us
                if tfmicro and tfmicro_avg_us != ""
                else ""
            ),
        })

    write_csv(rows, "comparison.csv")
    plotted = maybe_plot_png(rows, "comparison.png")
    if not plotted:
        write_svg(rows, "comparison.svg")

    print(f"filas directas: {len(direct_rows)}")
    print(f"filas TF Micro: {len(tfmicro_rows)}")
    timed_rows = [row for row in rows if row["tfmicro_avg_us"] != ""]
    if timed_rows:
        direct_avg = sum(row["direct_avg_us"] for row in timed_rows) / len(timed_rows)
        tfmicro_avg = sum(row["tfmicro_avg_us"] for row in timed_rows) / len(timed_rows)
        print(f"tiempo promedio direct: {direct_avg:.3f} us")
        print(f"tiempo promedio TF Micro: {tfmicro_avg:.3f} us")
        print(f"diferencia promedio direct - TF Micro: {direct_avg - tfmicro_avg:.3f} us")
    print("generado: comparison.csv")
    if plotted:
        print("generado: comparison.png")
    else:
        print("matplotlib no esta instalado; generado: comparison.svg")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
