#!/usr/bin/env python3
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


GRAPH_ORDER = [
    "dag2_new_time",
    "dagB0_new_time",
    "dag0_new_time",
    "triadag20_7_new_time",
]

VARIANT_ORDER = [
    "greedy",
    "aco_default",
    "aco_tuned",
    "sao_off",
    "sao_fed",
    "csao_off",
    "csao_fed",
]

VARIANT_LABELS = {
    "greedy": "Greedy",
    "aco_default": "ACO default",
    "aco_tuned": "ACO tuned",
    "sao_off": "SAO 0.0",
    "sao_fed": "SAO + Fedorenko",
    "csao_off": "CSAO 0.0",
    "csao_fed": "CSAO + Fedorenko",
}

STYLES = {
    "greedy": {"color": "#3f3f46", "dash": "", "marker": "#3f3f46"},
    "aco_default": {"color": "#d97706", "dash": "7 5", "marker": "#d97706"},
    "aco_tuned": {"color": "#d97706", "dash": "", "marker": "#d97706"},
    "sao_off": {"color": "#2563eb", "dash": "7 5", "marker": "#2563eb"},
    "sao_fed": {"color": "#2563eb", "dash": "", "marker": "#2563eb"},
    "csao_off": {"color": "#16a34a", "dash": "7 5", "marker": "#16a34a"},
    "csao_fed": {"color": "#16a34a", "dash": "", "marker": "#16a34a"},
}

DELTA_SERIES = [
    ("aco_default", "aco_tuned", "ACO tuned gain", "#d97706"),
    ("sao_off", "sao_fed", "SAO Fedorenko gain", "#2563eb"),
    ("csao_off", "csao_fed", "CSAO Fedorenko gain", "#16a34a"),
]


def svg_escape(value):
    return (
        str(value)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def fmt_number(value):
    if value is None:
        return "-"
    if isinstance(value, float):
        if abs(value) >= 100:
            return f"{value:.1f}".rstrip("0").rstrip(".")
        if abs(value) >= 10:
            return f"{value:.2f}".rstrip("0").rstrip(".")
        return f"{value:.3f}".rstrip("0").rstrip(".")
    return str(value)


def nice_integer_step(span, target_ticks=6):
    if span <= 0:
        return 1
    raw = span / max(1, target_ticks - 1)
    magnitude = 10 ** math.floor(math.log10(raw))
    scaled = raw / magnitude
    for candidate in (1, 2, 5, 10):
        if scaled <= candidate + 1e-12:
            return max(1, int(candidate * magnitude))
    return max(1, int(10 * magnitude))


def integer_ticks(vmin, vmax, target_ticks=6):
    step = nice_integer_step(vmax - vmin, target_ticks)
    start = int(math.ceil(vmin / step) * step)
    ticks = []
    tick = start
    while tick <= vmax + 1e-9:
        ticks.append(int(tick))
        tick += step
    if not ticks:
        ticks = [int(vmin), int(vmax)]
    return ticks


def axis_bounds(values, include_zero=False):
    vals = [float(v) for v in values if v is not None]
    if not vals:
        return 0.0, 1.0
    if include_zero:
        vals.append(0.0)
    vmin = min(vals)
    vmax = max(vals)
    if math.isclose(vmin, vmax):
        delta = max(1.0, abs(vmin) * 0.05)
        return vmin - delta, vmax + delta
    pad = (vmax - vmin) * 0.08
    return vmin - pad, vmax + pad


def integer_axis_bounds(values, include_zero=False):
    vals = [float(v) for v in values if v is not None]
    if not vals:
        return 0, 1
    if include_zero:
        vals.append(0.0)
    vmin = math.floor(min(vals))
    vmax = math.ceil(max(vals))
    if vmin == vmax:
        vmax = vmin + 1
    return vmin, vmax


def scale_linear(value, domain_min, domain_max, range_min, range_max):
    if math.isclose(domain_min, domain_max):
        return (range_min + range_max) / 2
    ratio = (value - domain_min) / (domain_max - domain_min)
    return range_min + ratio * (range_max - range_min)


def category_positions(values, x0, x1):
    if not values:
        return {}
    if len(values) == 1:
        return {values[0]: (x0 + x1) / 2}
    step = (x1 - x0) / (len(values) - 1)
    return {value: x0 + idx * step for idx, value in enumerate(values)}


def add_text(parts, x, y, text, size=12, weight="normal", fill="#111827", anchor="middle", rotate=None):
    extra = ""
    if rotate is not None:
        extra = f' transform="rotate({rotate} {x:.2f} {y:.2f})"'
    parts.append(
        f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" font-weight="{weight}" '
        f'fill="{fill}" text-anchor="{anchor}" font-family="Segoe UI, Arial, sans-serif"{extra}>{svg_escape(text)}</text>'
    )


def add_line(parts, x1, y1, x2, y2, stroke="#9ca3af", width=1, dash=""):
    dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
    parts.append(
        f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
        f'stroke="{stroke}" stroke-width="{width}"{dash_attr}/>'
    )


def add_rect(parts, x, y, w, h, fill="none", stroke="#d1d5db", width=1):
    parts.append(
        f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" '
        f'fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'
    )


def add_circle(parts, cx, cy, r, fill, stroke="white", width=1):
    parts.append(
        f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'
    )


def add_polyline(parts, points, stroke, width=2.4, dash=""):
    if len(points) < 2:
        return
    dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
    pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
    parts.append(
        f'<polyline fill="none" stroke="{stroke}" stroke-width="{width}" points="{pts}" '
        f'stroke-linejoin="round" stroke-linecap="round"{dash_attr}/>'
    )


def load_rows(csv_path: Path):
    rows = []
    with csv_path.open("r", encoding="utf-8", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            rows.append(
                {
                    "variant": row["variant"],
                    "graph": row["graph"],
                    "processors": int(row["processors"]),
                    "memory": int(row["memory"]),
                    "makespan": int(row["makespan"]) if row["makespan"] else None,
                    "runtime_sec": float(row["runtime_sec"]) if row["runtime_sec"] else None,
                    "status": row["status"],
                }
            )
    return rows


def build_lookup(rows):
    return {(r["variant"], r["graph"], r["processors"], r["memory"]): r for r in rows}


def graph_memories(rows, graph):
    return sorted({r["memory"] for r in rows if r["graph"] == graph})


def graph_processors(rows, graph):
    return sorted({r["processors"] for r in rows if r["graph"] == graph})


def draw_legend(parts, items, x, y):
    cursor_y = y
    for label, color, dash in items:
        add_line(parts, x, cursor_y, x + 28, cursor_y, stroke=color, width=3, dash=dash)
        add_circle(parts, x + 14, cursor_y, 4, fill=color)
        add_text(parts, x + 38, cursor_y + 4, label, size=12, anchor="start")
        cursor_y += 22


def render_makespan_svg(rows, graph, out_path: Path):
    processors = graph_processors(rows, graph)
    memories = graph_memories(rows, graph)
    lookup = build_lookup(rows)

    width = 380 * len(processors) + 260
    height = 560
    left = 70
    top = 70
    panel_w = 290
    panel_h = 320
    gap = 26
    right_legend_x = left + len(processors) * (panel_w + gap) - gap + 30
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
    ]
    add_rect(parts, 0, 0, width, height, fill="white", stroke="white", width=0)
    add_text(parts, width / 2, 34, f"Makespan vs memory: {graph}", size=22, weight="bold")
    add_text(parts, width / 2, 58, "Separate panels show processor counts P", size=12, fill="#4b5563")

    values = []
    for proc_count in processors:
        for variant in VARIANT_ORDER:
            for memory in memories:
                row = lookup.get((variant, graph, proc_count, memory))
                if row and row["makespan"] is not None:
                    values.append(row["makespan"])
    y_min, y_max = integer_axis_bounds(values)
    y_ticks = integer_ticks(y_min, y_max, target_ticks=6)

    for p_idx, proc_count in enumerate(processors):
        px = left + p_idx * (panel_w + gap)
        py = top + 20
        add_rect(parts, px, py, panel_w, panel_h, fill="none", stroke="#d1d5db")
        add_text(parts, px + panel_w / 2, py - 10, f"P = {proc_count}", size=14, weight="bold")

        plot_x0 = px + 48
        plot_x1 = px + panel_w - 18
        plot_y0 = py + panel_h - 48
        plot_y1 = py + 16
        x_map = category_positions(memories, plot_x0, plot_x1)

        for tick in y_ticks:
            ty = scale_linear(tick, y_min, y_max, plot_y0, plot_y1)
            add_line(parts, plot_x0, ty, plot_x1, ty, stroke="#e5e7eb", width=1)
            add_text(parts, plot_x0 - 8, ty + 4, tick, size=11, anchor="end", fill="#6b7280")
        add_line(parts, plot_x0, plot_y0, plot_x1, plot_y0, stroke="#111827", width=1.2)
        add_line(parts, plot_x0, plot_y0, plot_x0, plot_y1, stroke="#111827", width=1.2)

        for memory in memories:
            tx = x_map[memory]
            add_line(parts, tx, plot_y0, tx, plot_y0 + 5, stroke="#111827", width=1)
            add_text(parts, tx, plot_y0 + 16, memory, size=10, fill="#4b5563", rotate=45)

        for variant in VARIANT_ORDER:
            style = STYLES[variant]
            points = []
            for memory in memories:
                row = lookup.get((variant, graph, proc_count, memory))
                if not row or row["makespan"] is None:
                    if points:
                        add_polyline(parts, points, style["color"], dash=style["dash"])
                        points = []
                    continue
                x = x_map[memory]
                y = scale_linear(row["makespan"], y_min, y_max, plot_y0, plot_y1)
                points.append((x, y))
                add_circle(parts, x, y, 3.6, fill=style["marker"])
            if points:
                add_polyline(parts, points, style["color"], dash=style["dash"])

        add_text(parts, px + panel_w / 2, py + panel_h + 30, "Memory limit M", size=12)
        add_text(parts, px - 30, py + panel_h / 2, "Makespan", size=12, rotate=-90)

    legend_items = [(VARIANT_LABELS[v], STYLES[v]["color"], STYLES[v]["dash"]) for v in VARIANT_ORDER]
    add_text(parts, right_legend_x + 80, top + 34, "Legend", size=14, weight="bold")
    draw_legend(parts, legend_items, right_legend_x, top + 60)
    parts.append("</svg>")
    out_path.write_text("\n".join(parts), encoding="utf-8")


def render_feasibility_svg(rows, graph, out_path: Path):
    processors = graph_processors(rows, graph)
    lookup = build_lookup(rows)
    by_variant = {}
    y_values = []
    for variant in VARIANT_ORDER:
        vals = []
        for proc_count in processors:
            feasible = []
            for row in rows:
                if row["graph"] == graph and row["variant"] == variant and row["processors"] == proc_count and row["makespan"] is not None:
                    feasible.append(row["memory"])
            min_mem = min(feasible) if feasible else None
            vals.append(min_mem)
            if min_mem is not None:
                y_values.append(min_mem)
        by_variant[variant] = vals

    width = 980
    height = 460
    left = 80
    top = 80
    plot_w = 620
    plot_h = 260
    plot_x0 = left
    plot_x1 = left + plot_w
    plot_y0 = top + plot_h
    plot_y1 = top
    x_map = category_positions(processors, plot_x0, plot_x1)
    y_min, y_max = integer_axis_bounds(y_values)
    y_ticks = integer_ticks(y_min, y_max, target_ticks=6)

    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">']
    add_rect(parts, 0, 0, width, height, fill="white", stroke="white", width=0)
    add_text(parts, width / 2, 34, f"Feasibility boundary: {graph}", size=22, weight="bold")
    add_text(parts, width / 2, 58, "Minimal memory where a valid schedule exists", size=12, fill="#4b5563")

    add_rect(parts, plot_x0, plot_y1, plot_w, plot_h, fill="none", stroke="#d1d5db")
    for tick in y_ticks:
        ty = scale_linear(tick, y_min, y_max, plot_y0, plot_y1)
        add_line(parts, plot_x0, ty, plot_x1, ty, stroke="#e5e7eb", width=1)
        add_text(parts, plot_x0 - 8, ty + 4, tick, size=11, anchor="end", fill="#6b7280")
    add_line(parts, plot_x0, plot_y0, plot_x1, plot_y0, stroke="#111827", width=1.2)
    add_line(parts, plot_x0, plot_y0, plot_x0, plot_y1, stroke="#111827", width=1.2)

    for proc_count in processors:
        tx = x_map[proc_count]
        add_line(parts, tx, plot_y0, tx, plot_y0 + 5, stroke="#111827", width=1)
        add_text(parts, tx, plot_y0 + 20, proc_count, size=12, fill="#4b5563")

    for variant in VARIANT_ORDER:
        style = STYLES[variant]
        points = []
        for idx, proc_count in enumerate(processors):
            value = by_variant[variant][idx]
            if value is None:
                if points:
                    add_polyline(parts, points, style["color"], dash=style["dash"])
                    points = []
                continue
            x = x_map[proc_count]
            y = scale_linear(value, y_min, y_max, plot_y0, plot_y1)
            points.append((x, y))
            add_circle(parts, x, y, 4, fill=style["marker"])
        if points:
            add_polyline(parts, points, style["color"], dash=style["dash"])

    add_text(parts, (plot_x0 + plot_x1) / 2, plot_y0 + 44, "Processors P", size=12)
    add_text(parts, plot_x0 - 46, (plot_y0 + plot_y1) / 2, "Min feasible M", size=12, rotate=-90)
    add_text(parts, 796, 92, "Legend", size=14, weight="bold")
    draw_legend(parts, [(VARIANT_LABELS[v], STYLES[v]["color"], STYLES[v]["dash"]) for v in VARIANT_ORDER], 740, 120)
    parts.append("</svg>")
    out_path.write_text("\n".join(parts), encoding="utf-8")


def render_delta_svg(rows, graph, out_path: Path):
    processors = graph_processors(rows, graph)
    memories = graph_memories(rows, graph)
    lookup = build_lookup(rows)

    width = 380 * len(processors) + 240
    height = 540
    left = 70
    top = 78
    panel_w = 290
    panel_h = 300
    gap = 26
    legend_x = left + len(processors) * (panel_w + gap) - gap + 28

    delta_values = []
    for proc_count in processors:
        for base_key, improved_key, _, _ in DELTA_SERIES:
            for memory in memories:
                base = lookup.get((base_key, graph, proc_count, memory))
                improved = lookup.get((improved_key, graph, proc_count, memory))
                if base and improved and base["makespan"] is not None and improved["makespan"] is not None:
                    delta_values.append(base["makespan"] - improved["makespan"])
    y_min, y_max = axis_bounds(delta_values, include_zero=True)
    y_ticks = integer_ticks(math.floor(y_min), math.ceil(y_max), target_ticks=6)

    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">']
    add_rect(parts, 0, 0, width, height, fill="white", stroke="white", width=0)
    add_text(parts, width / 2, 34, f"Improvement over baseline variants: {graph}", size=22, weight="bold")
    add_text(parts, width / 2, 58, "Positive values mean the improved variant found a shorter schedule", size=12, fill="#4b5563")

    for p_idx, proc_count in enumerate(processors):
        px = left + p_idx * (panel_w + gap)
        py = top + 20
        add_rect(parts, px, py, panel_w, panel_h, fill="none", stroke="#d1d5db")
        add_text(parts, px + panel_w / 2, py - 10, f"P = {proc_count}", size=14, weight="bold")
        plot_x0 = px + 48
        plot_x1 = px + panel_w - 18
        plot_y0 = py + panel_h - 48
        plot_y1 = py + 16
        x_map = category_positions(memories, plot_x0, plot_x1)

        for tick in y_ticks:
            ty = scale_linear(tick, y_min, y_max, plot_y0, plot_y1)
            stroke = "#cbd5e1" if tick == 0 else "#e5e7eb"
            width_tick = 1.5 if tick == 0 else 1
            add_line(parts, plot_x0, ty, plot_x1, ty, stroke=stroke, width=width_tick)
            add_text(parts, plot_x0 - 8, ty + 4, tick, size=11, anchor="end", fill="#6b7280")
        add_line(parts, plot_x0, plot_y0, plot_x1, plot_y0, stroke="#111827", width=1.2)
        add_line(parts, plot_x0, plot_y0, plot_x0, plot_y1, stroke="#111827", width=1.2)

        for memory in memories:
            tx = x_map[memory]
            add_line(parts, tx, plot_y0, tx, plot_y0 + 5, stroke="#111827", width=1)
            add_text(parts, tx, plot_y0 + 16, memory, size=10, fill="#4b5563", rotate=45)

        for base_key, improved_key, label, color in DELTA_SERIES:
            points = []
            for memory in memories:
                base = lookup.get((base_key, graph, proc_count, memory))
                improved = lookup.get((improved_key, graph, proc_count, memory))
                if not base or not improved or base["makespan"] is None or improved["makespan"] is None:
                    if points:
                        add_polyline(parts, points, color)
                        points = []
                    continue
                delta = base["makespan"] - improved["makespan"]
                x = x_map[memory]
                y = scale_linear(delta, y_min, y_max, plot_y0, plot_y1)
                points.append((x, y))
                add_circle(parts, x, y, 3.8, fill=color)
            if points:
                add_polyline(parts, points, color)

        add_text(parts, px + panel_w / 2, py + panel_h + 30, "Memory limit M", size=12)
        add_text(parts, px - 34, py + panel_h / 2, "Gain", size=12, rotate=-90)

    add_text(parts, legend_x + 70, top + 34, "Legend", size=14, weight="bold")
    draw_legend(parts, [(label, color, "") for _, _, label, color in DELTA_SERIES], legend_x, top + 60)
    parts.append("</svg>")
    out_path.write_text("\n".join(parts), encoding="utf-8")


def compute_summary(rows):
    total_cells = len({(r["graph"], r["processors"], r["memory"]) for r in rows})
    best_by_cell = {}
    for row in rows:
        if row["makespan"] is None:
            continue
        key = (row["graph"], row["processors"], row["memory"])
        best_by_cell[key] = min(best_by_cell.get(key, row["makespan"]), row["makespan"])

    summary = {}
    for variant in VARIANT_ORDER:
        subset = [r for r in rows if r["variant"] == variant]
        feasible = [r for r in subset if r["makespan"] is not None]
        gaps = []
        runtimes = []
        wins = 0
        for row in feasible:
            key = (row["graph"], row["processors"], row["memory"])
            best = best_by_cell.get(key)
            if best is None:
                continue
            gaps.append((row["makespan"] - best) / best if best else 0.0)
            if row["runtime_sec"] is not None:
                runtimes.append(row["runtime_sec"])
            if row["makespan"] == best:
                wins += 1
        summary[variant] = {
            "success_rate": len(feasible) / total_cells if total_cells else 0.0,
            "mean_rel_gap": sum(gaps) / len(gaps) if gaps else None,
            "mean_runtime_sec": sum(runtimes) / len(runtimes) if runtimes else None,
            "win_count": wins,
            "feasible_count": len(feasible),
            "total_cells": total_cells,
        }
    return summary


def draw_bar_panel(parts, x, y, w, h, title, values_map, labels, colors, formatter, max_override=None):
    add_rect(parts, x, y, w, h, fill="none", stroke="#d1d5db")
    add_text(parts, x + w / 2, y - 10, title, size=14, weight="bold")
    values = [values_map[label] for label in labels if values_map[label] is not None]
    if not values:
        return
    vmin = 0.0
    vmax = max_override if max_override is not None else max(values)
    if vmax <= 0:
        vmax = 1.0
    ticks = integer_ticks(0, math.ceil(vmax), target_ticks=5)
    plot_x0 = x + 34
    plot_x1 = x + w - 14
    plot_y0 = y + h - 46
    plot_y1 = y + 18
    add_line(parts, plot_x0, plot_y0, plot_x1, plot_y0, stroke="#111827", width=1.2)
    add_line(parts, plot_x0, plot_y0, plot_x0, plot_y1, stroke="#111827", width=1.2)
    for tick in ticks:
        ty = scale_linear(tick, 0, vmax, plot_y0, plot_y1)
        add_line(parts, plot_x0, ty, plot_x1, ty, stroke="#e5e7eb", width=1)
        add_text(parts, plot_x0 - 8, ty + 4, formatter(tick), size=10, anchor="end", fill="#6b7280")
    bar_w = (plot_x1 - plot_x0) / max(1, len(labels)) * 0.58
    step = (plot_x1 - plot_x0) / max(1, len(labels))
    for idx, label in enumerate(labels):
        value = values_map[label]
        cx = plot_x0 + idx * step + step / 2
        if value is not None:
            ty = scale_linear(value, 0, vmax, plot_y0, plot_y1)
            add_rect(parts, cx - bar_w / 2, ty, bar_w, plot_y0 - ty, fill=colors[label], stroke=colors[label], width=1)
            add_text(parts, cx, ty - 6, formatter(value), size=10)
        add_text(parts, cx, plot_y0 + 16, VARIANT_LABELS[label], size=10, fill="#4b5563", rotate=35)


def render_summary_svg(rows, out_path: Path):
    summary = compute_summary(rows)
    width = 1580
    height = 520
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">']
    add_rect(parts, 0, 0, width, height, fill="white", stroke="white", width=0)
    add_text(parts, width / 2, 34, "Aggregate comparison summary", size=22, weight="bold")
    add_text(parts, width / 2, 58, "Success rate, average relative gap to best known result, and average runtime", size=12, fill="#4b5563")
    colors = {variant: STYLES[variant]["color"] for variant in VARIANT_ORDER}
    success_map = {variant: summary[variant]["success_rate"] * 100.0 for variant in VARIANT_ORDER}
    gap_map = {variant: (summary[variant]["mean_rel_gap"] * 100.0 if summary[variant]["mean_rel_gap"] is not None else None) for variant in VARIANT_ORDER}
    runtime_map = {variant: summary[variant]["mean_runtime_sec"] for variant in VARIANT_ORDER}
    draw_bar_panel(parts, 40, 100, 470, 340, "Success rate, %", success_map, VARIANT_ORDER, colors, lambda x: fmt_number(x), max_override=100.0)
    draw_bar_panel(parts, 555, 100, 470, 340, "Mean relative gap to best, %", gap_map, VARIANT_ORDER, colors, lambda x: fmt_number(x))
    draw_bar_panel(parts, 1070, 100, 470, 340, "Mean runtime, sec", runtime_map, VARIANT_ORDER, colors, lambda x: fmt_number(x))
    parts.append("</svg>")
    out_path.write_text("\n".join(parts), encoding="utf-8")


def write_index(out_dir: Path, graphs):
    items = []
    for graph in graphs:
        items.append(f"<h2>{svg_escape(graph)}</h2>")
        for kind, label in [
            ("feasibility", "Feasibility boundary"),
            ("makespan", "Makespan vs memory"),
            ("delta", "Improvement vs baseline"),
        ]:
            filename = f"{kind}_{graph}.svg"
            items.append(f'<p><a href="{filename}">{label}</a></p>')
            items.append(f'<img src="{filename}" style="max-width:100%;border:1px solid #ddd;margin-bottom:24px">')
    items.append("<h2>Aggregate summary</h2>")
    items.append('<p><a href="summary.svg">Summary</a></p>')
    items.append('<img src="summary.svg" style="max-width:100%;border:1px solid #ddd;margin-bottom:24px">')
    html = (
        "<!doctype html><html><head><meta charset='utf-8'><title>Coursework comparison plots</title>"
        "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.4;}img{display:block}</style>"
        "</head><body><h1>Coursework comparison plots</h1>"
        + "\n".join(items)
        + "</body></html>"
    )
    (out_dir / "index.html").write_text(html, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", required=True, help="Path to results_long.csv")
    parser.add_argument("--out-dir", required=True, help="Directory for generated SVG plots")
    parser.add_argument("--graph", action="append", dest="graphs", help="Optional graph subset")
    args = parser.parse_args()

    rows = load_rows(Path(args.results))
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    graphs = args.graphs if args.graphs else [g for g in GRAPH_ORDER if any(r["graph"] == g for r in rows)]

    for graph in graphs:
        render_feasibility_svg(rows, graph, out_dir / f"feasibility_{graph}.svg")
        render_makespan_svg(rows, graph, out_dir / f"makespan_{graph}.svg")
        render_delta_svg(rows, graph, out_dir / f"delta_{graph}.svg")
    render_summary_svg(rows, out_dir / "summary.svg")
    write_index(out_dir, graphs)


if __name__ == "__main__":
    main()
