#!/usr/bin/env python3
import argparse
import json
import math
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


NUMERIC_TRIAL_KEYS = {
    "objective",
    "quality_objective",
    "runtime_penalty",
    "mean_makespan",
    "min_makespan",
    "max_makespan",
    "std_makespan",
    "mean_runtime_sec",
    "min_runtime_sec",
    "max_runtime_sec",
    "std_runtime_sec",
    "mean_rel_gap",
    "min_rel_gap",
    "max_rel_gap",
    "std_rel_gap",
    "min_repeat_objective",
    "max_repeat_objective",
}


def svg_escape(value):
    return (
        str(value)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def detect_svg_size(svg_text):
    width_match = re.search(r'width="([0-9]+(?:\.[0-9]+)?)"', svg_text)
    height_match = re.search(r'height="([0-9]+(?:\.[0-9]+)?)"', svg_text)
    width = int(float(width_match.group(1))) if width_match else 1360
    height = int(float(height_match.group(1))) if height_match else 900
    return max(width, 320), max(height, 240)


def find_browser_executable():
    candidates = [
        shutil.which("msedge"),
        shutil.which("chrome"),
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return str(candidate)
    return None


def render_png_with_browser(svg_text, out_path: Path):
    browser = find_browser_executable()
    if not browser:
        raise RuntimeError(
            "PNG export fallback requires Microsoft Edge or Google Chrome, "
            "but no supported browser executable was found."
        )

    width, height = detect_svg_size(svg_text)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        html_path = tmpdir_path / "plot.html"
        html_path.write_text(
            "<!doctype html><html><head><meta charset='utf-8'>"
            "<style>html,body{margin:0;padding:0;background:white;overflow:hidden;}"
            "svg{display:block;margin:0;}</style></head><body>"
            f"{svg_text}</body></html>",
            encoding="utf-8",
        )
        cmd = [
            browser,
            "--headless",
            "--disable-gpu",
            "--hide-scrollbars",
            f"--window-size={width},{height}",
            f"--screenshot={str(out_path.resolve())}",
            html_path.resolve().as_uri(),
        ]
        completed = subprocess.run(cmd, capture_output=True, text=True)
        if completed.returncode != 0:
            raise RuntimeError(
                "Browser-based PNG export failed.\n"
                f"Command: {' '.join(cmd)}\n"
                f"STDOUT:\n{completed.stdout}\n"
                f"STDERR:\n{completed.stderr}"
            )


def render_png(svg_text, out_path: Path):
    try:
        import cairosvg
        out_path.parent.mkdir(parents=True, exist_ok=True)
        cairosvg.svg2png(bytestring=svg_text.encode("utf-8"), write_to=str(out_path))
        return
    except Exception:
        render_png_with_browser(svg_text, out_path)


def load_trials(path: Path):
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, list):
        raise ValueError("Summary JSON must contain a list of trial objects.")
    if not payload:
        raise ValueError("Summary JSON is empty.")
    return payload


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def detect_hyperparameters(trials):
    names = set()
    for trial in trials:
        params = trial.get("params", {})
        for name, value in params.items():
            if is_number(value):
                names.add(name)
    return sorted(names)


def detect_greedy_baseline(trials, metric_name):
    if not trials:
        return None
    baseline = trials[0].get("greedy_baseline")
    if not isinstance(baseline, dict):
        return None

    metric_key_map = {
        "objective": "quality_objective",
        "quality_objective": "quality_objective",
        "mean_makespan": "mean_makespan",
        "mean_runtime_sec": "mean_runtime_sec",
        "mean_rel_gap": "mean_rel_gap",
    }
    key = metric_key_map.get(metric_name, metric_name)
    value = baseline.get(key)
    if is_number(value):
        return float(value)
    return None


def build_param_groups(hyperparams, requested_groups):
    if not requested_groups:
        return [[name] for name in hyperparams]

    known = set(hyperparams)
    groups = []
    used = set()
    for raw_group in requested_groups:
        group = []
        for name in raw_group:
            if name not in known:
                raise ValueError(f"Unknown hyperparameter in --group: {name}")
            if name in used:
                raise ValueError(f"Hyperparameter repeated across groups: {name}")
            group.append(name)
            used.add(name)
        if group:
            groups.append(group)

    for name in hyperparams:
        if name not in used:
            groups.append([name])
    return groups


def filter_valid_points(trials, param_name, metric_name):
    points = []
    for trial in trials:
        params = trial.get("params", {})
        if param_name not in params or metric_name not in trial:
            continue
        x = params[param_name]
        y = trial[metric_name]
        if not is_number(x) or not is_number(y):
            continue
        if isinstance(y, float) and (math.isnan(y) or math.isinf(y)):
            continue
        points.append((float(x), float(y), trial.get("trial", -1)))
    return points


def metric_label(metric_name):
    labels = {
        "objective": "Objective",
        "quality_objective": "Quality Objective",
        "mean_makespan": "Mean Makespan",
        "mean_runtime_sec": "Mean Runtime, sec",
        "mean_rel_gap": "Mean Relative Gap",
    }
    return labels.get(metric_name, metric_name)


def nice_ticks(vmin, vmax, count=5):
    if vmax <= vmin:
        return [vmin]
    step = (vmax - vmin) / max(1, count - 1)
    return [vmin + i * step for i in range(count)]


def nice_ceil(value):
    if value <= 0:
        return 1.0
    magnitude = 10 ** math.floor(math.log10(value))
    scaled = value / magnitude
    for candidate in (1, 1.2, 1.25, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10):
        if scaled <= candidate + 1e-12:
            return candidate * magnitude
    return 10 * magnitude


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
    step = nice_integer_step(vmax - vmin, target_ticks=target_ticks)
    start = int(math.ceil(vmin / step) * step)
    ticks = []
    tick = start
    while tick <= vmax + 1e-9:
        ticks.append(int(tick))
        tick += step
    if not ticks:
        ticks = [int(vmin), int(vmax)]
    return ticks


def fmt_number(value):
    if abs(value) >= 1000:
        return f"{value:.0f}"
    if abs(value) >= 10:
        return f"{value:.2f}".rstrip("0").rstrip(".")
    return f"{value:.4f}".rstrip("0").rstrip(".")


def axis_bounds(values):
    vmin = min(values)
    vmax = max(values)
    if math.isclose(vmin, vmax):
        delta = 1.0 if math.isclose(vmin, 0.0) else abs(vmin) * 0.05
        vmin -= delta
        vmax += delta
    else:
        pad = (vmax - vmin) * 0.06
        vmin -= pad
        vmax += pad
    return vmin, vmax


def integer_axis_bounds(values):
    vmin = math.floor(min(values)) - 1
    vmax = math.ceil(max(values)) + 1
    if vmin == vmax:
        vmax = vmin + 2
    return vmin, vmax


def zero_based_axis_bounds(values):
    vmax = max(values)
    if math.isclose(vmax, 0.0):
        return 0.0, 1.0
    return 0.0, nice_ceil(vmax)


def trial_axis_bounds(trial_ids):
    if not trial_ids:
        return -0.5, 0.5
    tmin = min(trial_ids)
    tmax = max(trial_ids)
    if tmin == tmax:
        return tmin - 0.5, tmax + 0.5
    return tmin - 0.5, tmax + 0.5


def choose_trial_tick_step(trial_ids, target_labels=24):
    unique_ids = sorted(set(trial_ids))
    if not unique_ids:
        return 1
    if len(unique_ids) <= target_labels:
        return 1
    return max(1, math.ceil(len(unique_ids) / target_labels))


def metric_bounds_for_trial(trial, metric_name):
    explicit_pairs = {
        "objective": ("min_repeat_objective", "max_repeat_objective"),
        "mean_makespan": ("min_makespan", "max_makespan"),
        "mean_runtime_sec": ("min_runtime_sec", "max_runtime_sec"),
        "mean_rel_gap": ("min_rel_gap", "max_rel_gap"),
    }
    if metric_name in explicit_pairs:
        min_key, max_key = explicit_pairs[metric_name]
        min_value = trial.get(min_key)
        max_value = trial.get(max_key)
        if is_number(min_value) and is_number(max_value):
            return float(min_value), float(max_value)

    repeat_values = []
    for repeat in trial.get("repeat_summaries", []):
        value = repeat.get(metric_name)
        if is_number(value):
            repeat_values.append(float(value))
    if repeat_values:
        return min(repeat_values), max(repeat_values)
    return None


def collect_dashboard_rows(trials, hyperparams, metric_name):
    rows = []
    for trial in sorted(trials, key=lambda item: item.get("trial", -1)):
        params = trial.get("params", {})
        bounds = metric_bounds_for_trial(trial, metric_name)
        if bounds is None:
            continue
        metric_value = trial.get(metric_name)
        if not is_number(metric_value):
            continue
        min_metric, max_metric = bounds
        trial_id = int(trial.get("trial", -1))
        param_values = {}
        missing = False
        for name in hyperparams:
            value = params.get(name)
            if not is_number(value):
                missing = True
                break
            param_values[name] = float(value)
        if missing:
            continue
        rows.append(
            {
                "trial": trial_id,
                "metric": float(metric_value),
                "min_metric": float(min_metric),
                "max_metric": float(max_metric),
                "best_makespan": float(trial["min_makespan"]) if is_number(trial.get("min_makespan")) else None,
                "params": param_values,
                "stage": trial.get("stage"),
                "stage_label": trial.get("stage_label"),
            }
        )
    return rows


def stage_key(row):
    if row.get("stage_label") not in (None, ""):
        return ("label", str(row["stage_label"]))
    if row.get("stage") is not None:
        return ("stage", str(row["stage"]))
    return None


def split_stage_segments(rows):
    if not rows:
        return []
    segments = []
    current_rows = [rows[0]]
    current_key = stage_key(rows[0])
    for row in rows[1:]:
        key = stage_key(row)
        if key != current_key:
            segments.append(current_rows)
            current_rows = [row]
            current_key = key
        else:
            current_rows.append(row)
    segments.append(current_rows)
    return segments


def stage_fill_colors():
    return [
        "#eef4ff",
        "#fff3e0",
        "#f3ecff",
        "#e9fbef",
    ]


def stage_display_name(segment_rows, segment_idx, total_segments):
    raw_label = segment_rows[0].get("stage_label")
    if raw_label:
        lowered = str(raw_label).lower()
        if lowered == "coarse_optuna":
            return "Initial coarse stage"
        if lowered in {"refine_optuna", "final_refine_optuna", "boundary_refine_tpe"}:
            return "Final refinement stage" if segment_idx == total_segments - 1 else "Refinement stage"
        if lowered == "initial_tpe":
            return "Initial stage"
        return str(raw_label).replace("_", " ").title()
    if total_segments == 1:
        return "Optimization stage"
    if segment_idx == 0:
        return "Initial coarse stage"
    if segment_idx == total_segments - 1:
        return "Final refinement stage"
    return f"Stage {segment_idx + 1}"


def palette():
    return [
        "#2f6b7c",
        "#b54434",
        "#2e9f42",
        "#8b5fbf",
        "#d48a1f",
        "#c04a8b",
        "#3e78d8",
        "#6b6b2e",
    ]


def make_svg_dashboard(
    rows,
    param_groups,
    metric_name,
    title_prefix,
    greedy_baseline_value=None,
    px_per_trial=30,
    min_width=1360,
):
    top_panel_h = 180
    param_panel_h = 120
    panel_gap = 34
    left = 95
    legend_col_w = 240
    right = legend_col_w + 35
    top = 70
    bottom = 80
    params_start = top + top_panel_h + panel_gap
    if param_groups:
        plot_bottom = params_start + (len(param_groups) - 1) * (param_panel_h + panel_gap) + param_panel_h
    else:
        plot_bottom = top + top_panel_h

    trial_ids = [row["trial"] for row in rows]
    unique_trial_ids = sorted(set(trial_ids))
    trial_count = max(1, len(unique_trial_ids))
    dynamic_plot_w = max(1020, px_per_trial * max(1, trial_count - 1))
    width = max(min_width, left + right + dynamic_plot_w)
    plot_w = width - left - right
    trial_min, trial_max = trial_axis_bounds(trial_ids)
    metric_values = [row["min_metric"] for row in rows] + [row["max_metric"] for row in rows]
    if greedy_baseline_value is not None:
        metric_values.append(greedy_baseline_value)
    metric_min, metric_max = integer_axis_bounds(metric_values)
    metric_ticks = integer_ticks(metric_min, metric_max, target_ticks=5)
    best_row = min(
        rows,
        key=lambda row: (
            row["best_makespan"] if row["best_makespan"] is not None else row["min_metric"],
            row["trial"],
        ),
    )

    def sx(trial_id):
        return left + (trial_id - trial_min) / (trial_max - trial_min) * plot_w

    def sy_metric(value):
        return top + top_panel_h - (value - metric_min) / (metric_max - metric_min) * top_panel_h

    title = (
        f"{title_prefix}: {metric_label(metric_name)} and hyperparameters by trial"
        if title_prefix
        else f"{metric_label(metric_name)} and hyperparameters by trial"
    )

    compact_params = list(best_row["params"].items())
    param_pairs = []
    for idx in range(0, len(compact_params), 2):
        left_name, left_val = compact_params[idx]
        left_txt = f"{left_name}={fmt_number(left_val)}"
        if idx + 1 < len(compact_params):
            right_name, right_val = compact_params[idx + 1]
            right_txt = f"{right_name}={fmt_number(right_val)}"
            param_pairs.append((left_txt, right_txt))
        else:
            param_pairs.append((left_txt, ""))
    best_box_h = 34 + 14 * (2 + len(param_pairs))

    stage_segments = split_stage_segments(rows)
    has_stage_transition = len(stage_segments) > 1 and any(stage_key(row) is not None for row in rows)
    stage_colors = stage_fill_colors()

    summary_legend_h = 102 if greedy_baseline_value is not None else 82
    if has_stage_transition:
        summary_legend_h += 20 * len(stage_segments) + 18
    summary_legend_bottom = 46 + summary_legend_h

    param_legend_bottom = summary_legend_bottom
    if param_groups:
        param_legend_bottom = max(
            panel_top + 10 + (18 * len(param_group) + 10)
            for panel_top, param_group in (
                (params_start + idx * (param_panel_h + panel_gap), group)
                for idx, group in enumerate(param_groups)
            )
        )
    sidebar_bottom = max(summary_legend_bottom, param_legend_bottom)
    best_box_y = sidebar_bottom + 18
    height = max(plot_bottom + bottom, best_box_y + best_box_h + 28)

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    svg.append('<rect x="0" y="0" width="100%" height="100%" fill="#fffdf8"/>')

    if has_stage_transition:
        full_panel_height = plot_bottom - top
        for seg_idx, segment_rows in enumerate(stage_segments):
            seg_start_trial = segment_rows[0]["trial"]
            seg_end_trial = segment_rows[-1]["trial"]
            if seg_idx == 0:
                band_left = left
            else:
                prev_end = stage_segments[seg_idx - 1][-1]["trial"]
                band_left = sx((prev_end + seg_start_trial) / 2.0)
            if seg_idx == len(stage_segments) - 1:
                band_right = left + plot_w
            else:
                next_start = stage_segments[seg_idx + 1][0]["trial"]
                band_right = sx((seg_end_trial + next_start) / 2.0)
            band_width = max(0.0, band_right - band_left)
            fill = stage_colors[seg_idx % len(stage_colors)]
            svg.append(
                f'<rect x="{band_left:.2f}" y="{top}" width="{band_width:.2f}" height="{full_panel_height:.2f}" '
                f'fill="{fill}" fill-opacity="0.38" stroke="none"/>'
            )
            caption = stage_display_name(segment_rows, seg_idx, len(stage_segments))
            caption_x = band_left + band_width / 2.0
            if band_width >= 130:
                svg.append(
                    f'<text x="{caption_x:.2f}" y="46" text-anchor="middle" font-size="12" font-weight="bold" '
                    f'fill="#5d574c">{svg_escape(caption)}</text>'
                )
        for seg_idx in range(len(stage_segments) - 1):
            last_trial = stage_segments[seg_idx][-1]["trial"]
            next_trial = stage_segments[seg_idx + 1][0]["trial"]
            transition_x = sx((last_trial + next_trial) / 2.0)
            svg.append(
                f'<line x1="{transition_x:.2f}" y1="{top}" x2="{transition_x:.2f}" y2="{plot_bottom}" '
                f'stroke="#8b7a52" stroke-width="1.8" stroke-dasharray="8 6"/>'
            )
            if seg_idx == len(stage_segments) - 2:
                svg.append(
                    f'<text x="{transition_x + 10:.2f}" y="{top + 16}" font-size="11" font-weight="bold" '
                    f'fill="#8b7a52">Final refinement starts</text>'
                )

    for y_tick in metric_ticks:
        py = sy_metric(y_tick)
        svg.append(f'<line x1="{left}" y1="{py:.2f}" x2="{left + plot_w}" y2="{py:.2f}" stroke="#d9d4c7" stroke-dasharray="4 4" stroke-width="1"/>')
        svg.append(f'<text x="{left - 10}" y="{py + 4:.2f}" text-anchor="end" font-size="12" fill="#4d4b47">{svg_escape(fmt_number(y_tick))}</text>')

    tick_step = choose_trial_tick_step(trial_ids)
    tick_ids = unique_trial_ids[::tick_step]
    if unique_trial_ids and tick_ids[-1] != unique_trial_ids[-1]:
        tick_ids.append(unique_trial_ids[-1])
    for trial_tick in tick_ids:
        px = sx(trial_tick)
        svg.append(f'<line x1="{px:.2f}" y1="{top}" x2="{px:.2f}" y2="{plot_bottom}" stroke="#ece7dc" stroke-dasharray="3 4" stroke-width="1"/>')
        svg.append(f'<text x="{px:.2f}" y="{plot_bottom + 28}" text-anchor="middle" font-size="12" fill="#4d4b47">{trial_tick}</text>')

    if greedy_baseline_value is not None:
        baseline_y = sy_metric(greedy_baseline_value)
        svg.append(
            f'<line x1="{left}" y1="{baseline_y:.2f}" x2="{left + plot_w}" y2="{baseline_y:.2f}" '
            f'stroke="#7b3fb6" stroke-width="2.2" stroke-dasharray="10 6"/>'
        )
        svg.append(
            f'<text x="{left + plot_w - 8}" y="{baseline_y - 6:.2f}" text-anchor="end" '
            f'font-size="11" fill="#7b3fb6">Greedy = {svg_escape(fmt_number(greedy_baseline_value))}</text>'
        )

    best_x = sx(best_row["trial"])
    svg.append(
        f'<line x1="{best_x:.2f}" y1="{top}" x2="{best_x:.2f}" y2="{plot_bottom}" '
        f'stroke="#c8a227" stroke-dasharray="8 6" stroke-width="2.0"/>'
    )

    svg.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + top_panel_h}" stroke="#2f2b28" stroke-width="1.5"/>')
    svg.append(f'<line x1="{left}" y1="{top + top_panel_h}" x2="{left + plot_w}" y2="{top + top_panel_h}" stroke="#2f2b28" stroke-width="1.5"/>')

    min_path = " ".join(f"L {sx(row['trial']):.2f} {sy_metric(row['min_metric']):.2f}" for row in rows[1:])
    max_path = " ".join(f"L {sx(row['trial']):.2f} {sy_metric(row['max_metric']):.2f}" for row in rows[1:])
    first = rows[0]
    svg.append(
        f'<path d="M {sx(first["trial"]):.2f} {sy_metric(first["min_metric"]):.2f} {min_path}" '
        f'fill="none" stroke="#2e9f42" stroke-width="2.4"/>'
    )
    svg.append(
        f'<path d="M {sx(first["trial"]):.2f} {sy_metric(first["max_metric"]):.2f} {max_path}" '
        f'fill="none" stroke="#c23321" stroke-width="2.4"/>'
    )
    for row in rows:
        svg.append(f'<circle cx="{sx(row["trial"]):.2f}" cy="{sy_metric(row["min_metric"]):.2f}" r="3.9" fill="#2e9f42"/>')
        svg.append(f'<circle cx="{sx(row["trial"]):.2f}" cy="{sy_metric(row["max_metric"]):.2f}" r="3.9" fill="#c23321"/>')
    svg.append(
        f'<circle cx="{best_x:.2f}" cy="{sy_metric(best_row["min_metric"]):.2f}" r="5.8" '
        f'fill="#d4af37" stroke="#6f5a16" stroke-width="1.0"/>'
    )
    svg.append(
        f'<text x="{best_x:.2f}" y="{sy_metric(best_row["min_metric"]) + 4:.2f}" text-anchor="middle" '
        f'font-size="8.5" font-weight="bold" fill="#1f1d1a">B</text>'
    )

    colors = palette()
    for idx, param_group in enumerate(param_groups):
        panel_top = params_start + idx * (param_panel_h + panel_gap)
        panel_bottom = panel_top + param_panel_h
        values = []
        for row in rows:
            for param_name in param_group:
                values.append(row["params"][param_name])
        pmin, pmax = zero_based_axis_bounds(values)

        def sy_param(value):
            return panel_top + param_panel_h - (value - pmin) / (pmax - pmin) * param_panel_h

        for y_tick in nice_ticks(pmin, pmax, 5):
            py = sy_param(y_tick)
            svg.append(f'<line x1="{left}" y1="{py:.2f}" x2="{left + plot_w}" y2="{py:.2f}" stroke="#d9d4c7" stroke-dasharray="4 4" stroke-width="1"/>')
            svg.append(f'<text x="{left - 10}" y="{py + 4:.2f}" text-anchor="end" font-size="11" fill="#4d4b47">{svg_escape(fmt_number(y_tick))}</text>')

        svg.append(f'<line x1="{left}" y1="{panel_top}" x2="{left}" y2="{panel_bottom}" stroke="#2f2b28" stroke-width="1.5"/>')
        svg.append(f'<line x1="{left}" y1="{panel_bottom}" x2="{left + plot_w}" y2="{panel_bottom}" stroke="#2f2b28" stroke-width="1.5"/>')

        group_title = ", ".join(param_group)
        svg.append(
            f'<text x="26" y="{panel_top + param_panel_h / 2:.2f}" transform="rotate(-90 26 {panel_top + param_panel_h / 2:.2f})" '
            f'text-anchor="middle" font-size="14" fill="#3c3a36">Value</text>'
        )
        svg.append(f'<text x="{left + 8}" y="{panel_top - 10}" font-size="13" font-weight="bold" fill="#3c3a36">{svg_escape(group_title)}</text>')

        legend_x = left + plot_w + 20
        legend_y = panel_top + 10
        legend_h = 18 * len(param_group) + 10
        svg.append(f'<rect x="{legend_x}" y="{legend_y}" width="170" height="{legend_h}" rx="8" fill="#fffaf0" stroke="#ddd4c4"/>')

        for p_idx, param_name in enumerate(param_group):
            param_color = colors[p_idx % len(colors)]
            param_path = " ".join(
                f"L {sx(row['trial']):.2f} {sy_param(row['params'][param_name]):.2f}"
                for row in rows[1:]
            )
            svg.append(
                f'<path d="M {sx(rows[0]["trial"]):.2f} {sy_param(rows[0]["params"][param_name]):.2f} {param_path}" '
                f'fill="none" stroke="{param_color}" stroke-width="2.0"/>'
            )
            for row in rows:
                cx = sx(row["trial"])
                cy = sy_param(row["params"][param_name])
                svg.append(
                    f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="4.1" '
                    f'fill="{param_color}" fill-opacity="0.82" stroke="#ffffff" stroke-width="0.8">'
                    f'<title>trial={row["trial"]}, {param_name}={row["params"][param_name]}</title></circle>'
                )

            entry_y = legend_y + 16 + p_idx * 18
            svg.append(f'<line x1="{legend_x + 10}" y1="{entry_y}" x2="{legend_x + 28}" y2="{entry_y}" stroke="{param_color}" stroke-width="2.0"/>')
            svg.append(f'<text x="{legend_x + 36}" y="{entry_y + 4}" font-size="11" fill="#2f2b28">{svg_escape(param_name)}</text>')

    svg.append(f'<text x="{width / 2:.2f}" y="28" text-anchor="middle" font-size="22" font-weight="bold" fill="#2f2b28">{svg_escape(title)}</text>')
    svg.append(f'<text x="{width / 2:.2f}" y="{height - 12}" text-anchor="middle" font-size="15" fill="#3c3a36">Trial</text>')
    svg.append(
        f'<text x="26" y="{top + top_panel_h / 2:.2f}" transform="rotate(-90 26 {top + top_panel_h / 2:.2f})" '
        f'text-anchor="middle" font-size="15" fill="#3c3a36">{svg_escape(metric_label(metric_name))}</text>'
    )
    svg.append(f'<text x="{left + 8}" y="{top - 12}" font-size="14" font-weight="bold" fill="#3c3a36">Objective spread across repeats</text>')

    legend_x = left + plot_w + 20
    legend_y = 46
    legend_h = summary_legend_h
    svg.append(f'<rect x="{legend_x}" y="{legend_y}" width="205" height="{legend_h}" rx="10" fill="#fffaf0" stroke="#d8cfbf"/>')
    svg.append(f'<circle cx="{legend_x + 16}" cy="{legend_y + 20}" r="4.4" fill="{colors[0]}" fill-opacity="0.82" stroke="#ffffff" stroke-width="0.8"/>')
    svg.append(f'<text x="{legend_x + 30}" y="{legend_y + 25}" font-size="12" fill="#2f2b28">Hyperparameter value</text>')
    svg.append(f'<line x1="{legend_x + 8}" y1="{legend_y + 46}" x2="{legend_x + 24}" y2="{legend_y + 46}" stroke="#2e9f42" stroke-width="2.3"/>')
    svg.append(f'<text x="{legend_x + 30}" y="{legend_y + 50}" font-size="12" fill="#2f2b28">Min objective</text>')
    svg.append(f'<line x1="{legend_x + 8}" y1="{legend_y + 66}" x2="{legend_x + 24}" y2="{legend_y + 66}" stroke="#c23321" stroke-width="2.3"/>')
    svg.append(f'<text x="{legend_x + 30}" y="{legend_y + 70}" font-size="12" fill="#2f2b28">Max objective</text>')
    legend_cursor_y = legend_y + 90
    if greedy_baseline_value is not None:
        svg.append(f'<line x1="{legend_x + 8}" y1="{legend_cursor_y - 4}" x2="{legend_x + 24}" y2="{legend_cursor_y - 4}" stroke="#7b3fb6" stroke-width="2.3" stroke-dasharray="10 6"/>')
        svg.append(f'<text x="{legend_x + 30}" y="{legend_cursor_y}" font-size="12" fill="#2f2b28">Greedy baseline</text>')
        legend_cursor_y += 18
    if has_stage_transition:
        for seg_idx, segment_rows in enumerate(stage_segments):
            fill = stage_colors[seg_idx % len(stage_colors)]
            svg.append(f'<rect x="{legend_x + 8}" y="{legend_cursor_y - 11}" width="16" height="12" fill="{fill}" fill-opacity="0.85" stroke="#cfc6b6"/>')
            svg.append(
                f'<text x="{legend_x + 30}" y="{legend_cursor_y}" font-size="11" fill="#2f2b28">'
                f'{svg_escape(stage_display_name(segment_rows, seg_idx, len(stage_segments)))}</text>'
            )
            legend_cursor_y += 18
        svg.append(f'<line x1="{legend_x + 8}" y1="{legend_cursor_y - 5}" x2="{legend_x + 24}" y2="{legend_cursor_y - 5}" stroke="#8b7a52" stroke-width="1.8" stroke-dasharray="8 6"/>')
        svg.append(f'<text x="{legend_x + 30}" y="{legend_cursor_y}" font-size="11" fill="#2f2b28">Stage transition</text>')

    best_box_x = left + plot_w + 20
    svg.append(f'<rect x="{best_box_x}" y="{best_box_y}" width="205" height="{best_box_h}" rx="10" fill="#fffaf0" stroke="#c8a227"/>')
    svg.append(f'<text x="{best_box_x + 12}" y="{best_box_y + 18}" font-size="11" font-weight="bold" fill="#8c6b00">Best solution: trial #{best_row["trial"]}</text>')
    if best_row["best_makespan"] is not None:
        svg.append(f'<text x="{best_box_x + 12}" y="{best_box_y + 34}" font-size="10" fill="#2f2b28">best makespan={svg_escape(fmt_number(best_row["best_makespan"]))}</text>')
    else:
        svg.append(f'<text x="{best_box_x + 12}" y="{best_box_y + 34}" font-size="10" fill="#2f2b28">best={svg_escape(fmt_number(best_row["min_metric"]))}</text>')
    svg.append(f'<text x="{best_box_x + 12}" y="{best_box_y + 48}" font-size="10" fill="#2f2b28">range=[{svg_escape(fmt_number(best_row["min_metric"]))}, {svg_escape(fmt_number(best_row["max_metric"]))}]</text>')
    for idx, (left_txt, right_txt) in enumerate(param_pairs):
        y = best_box_y + 64 + idx * 14
        svg.append(f'<text x="{best_box_x + 12}" y="{y}" font-size="9.2" fill="#2f2b28">{svg_escape(left_txt)}</text>')
        if right_txt:
            svg.append(f'<text x="{best_box_x + 108}" y="{y}" font-size="9.2" fill="#2f2b28">{svg_escape(right_txt)}</text>')

    svg.append("</svg>")
    return "\n".join(svg)


def write_html_index(out_dir: Path, metric_name: str, generated_files):
    cards = []
    for path in generated_files:
        cards.append(
            f"""
            <div class="card">
              <h3>{svg_escape(path.stem)}</h3>
              <a href="{svg_escape(path.name)}"><img src="{svg_escape(path.name)}" alt="{svg_escape(path.name)}"></a>
            </div>
            """
        )
    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>ACO Hyperparameter Plots</title>
  <style>
    body {{ font-family: Segoe UI, Arial, sans-serif; margin: 24px; background: #f7f3ea; color: #2f2b28; }}
    h1 {{ margin-bottom: 8px; }}
    .meta {{ margin-bottom: 24px; color: #4d4b47; }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); gap: 20px; }}
    .card {{ background: white; border: 1px solid #ddd4c4; border-radius: 12px; padding: 14px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); }}
    .card h3 {{ margin-top: 0; font-size: 16px; }}
    img {{ width: 100%; height: auto; border-radius: 8px; border: 1px solid #ece4d5; background: #fffdf8; }}
    a {{ color: inherit; text-decoration: none; }}
  </style>
</head>
<body>
  <h1>ACO Hyperparameter Plots</h1>
  <div class="meta">Metric: <strong>{svg_escape(metric_name)}</strong>. Plots: {len(generated_files)}.</div>
  <div class="grid">
    {''.join(cards)}
  </div>
</body>
</html>
"""
    (out_dir / "index.html").write_text(html, encoding="utf-8")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot objective-vs-hyperparameter charts from tune_aco.py summary JSON."
    )
    parser.add_argument("--summary", required=True, help="Path to summary.json produced by tune_aco.py.")
    parser.add_argument("--out-dir", required=True, help="Directory where plots will be written.")
    parser.add_argument(
        "--metric",
        default="objective",
        choices=sorted(NUMERIC_TRIAL_KEYS),
        help="Trial-level metric to visualize.",
    )
    parser.add_argument(
        "--params",
        nargs="*",
        help="Optional explicit list of hyperparameters to plot. By default all numeric params are used.",
    )
    parser.add_argument(
        "--group",
        nargs="+",
        action="append",
        help="Draw listed hyperparameters on one shared lower panel. Can be repeated.",
    )
    parser.add_argument(
        "--title-prefix",
        default="ACO Tuning",
        help="Prefix used in figure titles.",
    )
    parser.add_argument(
        "--format",
        choices=("svg", "png", "both"),
        default="svg",
        help="Output image format. PNG export requires cairosvg.",
    )
    parser.add_argument(
        "--px-per-trial",
        type=int,
        default=30,
        help="Horizontal pixels allocated per trial step. Increase for denser trial sequences.",
    )
    parser.add_argument(
        "--min-width",
        type=int,
        default=1360,
        help="Minimum SVG width in pixels.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    trials = load_trials(Path(args.summary))
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    hyperparams = args.params if args.params else detect_hyperparameters(trials)
    if not hyperparams:
        raise ValueError("No numeric hyperparameters found in summary.")
    param_groups = build_param_groups(hyperparams, args.group)

    rows = collect_dashboard_rows(trials, hyperparams, args.metric)
    if not rows:
        raise ValueError("No plots were generated. Check metric and hyperparameter selection.")

    greedy_baseline_value = detect_greedy_baseline(trials, args.metric)
    svg = make_svg_dashboard(
        rows,
        param_groups,
        args.metric,
        args.title_prefix,
        greedy_baseline_value=greedy_baseline_value,
        px_per_trial=max(1, args.px_per_trial),
        min_width=max(320, args.min_width),
    )
    generated = []
    base_name = f"{args.metric}_dashboard"
    if args.format in ("svg", "both"):
        svg_path = out_dir / f"{base_name}.svg"
        svg_path.write_text(svg, encoding="utf-8")
        generated.append(svg_path)
    if args.format in ("png", "both"):
        png_path = out_dir / f"{base_name}.png"
        render_png(svg, png_path)
        generated.append(png_path)

    write_html_index(out_dir, args.metric, generated)
    print(f"Generated {len(generated)} plot file(s) in {out_dir}")


if __name__ == "__main__":
    main()
