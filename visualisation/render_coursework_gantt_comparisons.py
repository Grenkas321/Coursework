#!/usr/bin/env python3
"""Render large coursework Gantt+memory comparisons for selected algorithms."""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch


ALGORITHM_LABELS = {
    "greedy": "ЖА",
    "aco": "МА",
    "sao": "ИО",
    "sao_fed": "ИО-ОУП",
    "csao": "КИО",
    "csao_fed": "КИО-ОУП",
}

ALGORITHM_DIR_LABELS = {
    "greedy": "greedy",
    "aco": "aco_s{seed}",
    "sao": "sao_s{seed}",
    "sao_fed": "sao_fed_s{seed}",
    "csao": "csao",
    "csao_fed": "csao_fed",
}


@dataclass(frozen=True)
class BufferGroup:
    parent: str
    index: int
    size: int
    children: tuple[str, ...]


def load_graph(path: Path) -> tuple[list[str], dict[str, list[str]], dict[str, list[BufferGroup]]]:
    nodes: list[str] = []
    children: dict[str, list[str]] = {}
    buffers: dict[str, list[BufferGroup]] = {}

    with path.open(encoding="utf-8") as f:
        lines = f.readlines()[1:]

    for line in lines:
        parts = line.split()
        if not parts:
            continue
        node = parts[0]
        nodes.append(node)
        children[node] = []
        buffers[node] = []
        buffer_text = " ".join(parts[2:]).strip()
        if not buffer_text or buffer_text == "0:":
            continue

        for index, raw_group in enumerate(buffer_text.split(", "), start=1):
            size_text, child_text = raw_group.split(":", 1)
            child_list = tuple(child_text.split())
            size = int(size_text)
            children[node].extend(child_list)
            buffers[node].append(BufferGroup(node, index, size, child_list))

    return nodes, children, buffers


def load_schedule(path: Path, graph_name: str) -> dict:
    with path.open(encoding="utf-8") as f:
        return json.load(f)[graph_name]


def resolve_schedule_path(root: Path, dataset: str, graph_name: str, algorithm: str, seed: int) -> Path:
    if algorithm in {"sao", "sao_fed", "aco"}:
        label = ALGORITHM_DIR_LABELS[algorithm].format(seed=seed)
        seeded = (
            root
            / "build"
            / "final_exp_seed_stability"
            / "runs"
            / f"seed_{seed}"
            / algorithm
            / dataset
            / graph_name
            / "output"
            / label
            / "schedules"
            / "best"
            / f"{graph_name}.json"
        )
        if seeded.exists():
            return seeded

    label = ALGORITHM_DIR_LABELS.get(algorithm, algorithm)
    if "{seed}" in label:
        label = algorithm
    runtime = (
        root
        / "build"
        / "final_exp_runtime"
        / "runs"
        / algorithm
        / dataset
        / graph_name
        / "output"
        / label
        / "schedules"
        / "best"
        / f"{graph_name}.json"
    )
    if runtime.exists():
        return runtime

    raise FileNotFoundError(f"Cannot find schedule for {algorithm}: {runtime}")


def luminance(rgb: tuple[float, float, float]) -> float:
    r, g, b = rgb[:3]
    return 0.299 * r + 0.587 * g + 0.114 * b


def task_color(vertex: str):
    palette = list(plt.cm.tab20.colors) + list(plt.cm.Set3.colors) + list(plt.cm.Dark2.colors)
    return palette[int(vertex) % len(palette)]


def memory_profile(schedule: dict, buffers: dict[str, list[BufferGroup]]) -> tuple[list[int], list[int]]:
    events: dict[int, list[tuple[str, str]]] = {}
    max_finish = 0
    for vertex, params in schedule.items():
        start = int(params["start"])
        finish = int(params["finish"])
        max_finish = max(max_finish, finish)
        events.setdefault(start, []).append((vertex, "start"))
        events.setdefault(finish, []).append((vertex, "finish"))

    alive: dict[tuple[str, int], list[str]] = {}
    alive_size: dict[tuple[str, int], int] = {}
    xs: list[int] = []
    ys: list[int] = []

    for t in range(max_finish + 1):
        for vertex, status in events.get(t, []):
            if status == "start":
                for group in buffers.get(vertex, []):
                    key = (group.parent, group.index)
                    alive[key] = list(group.children)
                    alive_size[key] = group.size
            else:
                for key in list(alive):
                    if vertex in alive[key]:
                        alive[key].remove(vertex)
                        if not alive[key]:
                            del alive[key]
                            del alive_size[key]
        xs.append(t)
        ys.append(sum(alive_size.values()))

    return xs, ys


def draw_dependencies(ax, schedule: dict, children: dict[str, list[str]], processors: int) -> None:
    for parent, child_list in children.items():
        if parent not in schedule:
            continue
        parent_params = schedule[parent]
        x1 = int(parent_params["finish"])
        y1 = int(parent_params["processor"]) + 0.5

        for child in child_list:
            if child not in schedule:
                continue
            child_params = schedule[child]
            x2 = int(child_params["start"])
            y2 = int(child_params["processor"]) + 0.5
            if x2 < x1:
                continue

            gap = x2 - x1
            same_processor = abs(y1 - y2) < 1e-9
            if same_processor:
                rad = -0.22 if gap < 18 else -0.12
                start = (x1, y1 - 0.23)
                end = (max(x2, x1 + 4), y2 - 0.23)
            elif gap <= 2:
                rad = 0.0
                start = (x1, y1)
                end = (x2 + 4, y2)
            else:
                direction = -1 if y2 > y1 else 1
                rad = direction * min(0.16, 0.035 + gap / 6000.0)
                start = (x1, y1)
                end = (x2, y2)

            ax.add_patch(
                FancyArrowPatch(
                    start,
                    end,
                    arrowstyle="-|>",
                    mutation_scale=6.5,
                    linewidth=0.52,
                    color="black",
                    alpha=0.42,
                    connectionstyle=f"arc3,rad={rad}",
                    shrinkA=1.0,
                    shrinkB=1.0,
                    zorder=5,
                )
            )


def draw_gantt(
    ax,
    schedule_data: dict,
    graph_name: str,
    algorithm: str,
    processors: int,
    x_limit: int,
    children: dict[str, list[str]],
    show_dependencies: bool,
) -> None:
    schedule = schedule_data["schedule"]
    makespan = int(schedule_data["makespan"])
    runtime = float(schedule_data["runtime_sec"])

    for vertex, params in schedule.items():
        processor = int(params["processor"])
        start = int(params["start"])
        finish = int(params["finish"])
        duration = finish - start
        color = task_color(vertex)
        text_color = "black" if luminance(color) > 0.62 else "white"
        ax.broken_barh(
            [(start, duration)],
            (processor + 0.12, 0.76),
            facecolors=[color],
            edgecolors="white",
            linewidth=0.45,
            zorder=3,
        )
        font_size = 5.2 if duration >= 18 else 4.3
        ax.text(
            start + duration / 2,
            processor + 0.5,
            vertex,
            ha="center",
            va="center",
            fontsize=font_size,
            fontweight="bold",
            color=text_color,
            clip_on=True,
            path_effects=[pe.withStroke(linewidth=0.75, foreground="black" if text_color == "white" else "white")],
            zorder=8,
        )

    if show_dependencies:
        draw_dependencies(ax, schedule, children, processors)

    ax.axvline(makespan, color="0.2", linestyle="--", linewidth=1.1, alpha=0.85, zorder=6)
    ax.text(
        makespan,
        -0.35,
        f"{makespan}",
        ha="center",
        va="top",
        fontsize=8.5,
        fontweight="bold",
        color="0.15",
    )
    ax.set_xlim(0, x_limit)
    ax.set_ylim(0, processors)
    ax.set_yticks([p + 0.5 for p in range(processors)])
    ax.set_yticklabels([rf"$\pi_{{{p + 1}}}$" for p in range(processors)], fontsize=10, fontweight="bold")
    ax.invert_yaxis()
    ax.grid(True, axis="x", linestyle="--", linewidth=0.6, alpha=0.35)
    ax.grid(True, axis="y", linestyle="-", linewidth=0.3, alpha=0.18)
    ax.tick_params(axis="x", labelsize=9)
    ax.set_title(
        f"{ALGORITHM_LABELS.get(algorithm, algorithm)}: длительность={makespan}, время={runtime:.2f} с",
        fontsize=13,
        fontweight="bold",
        pad=5,
    )


def draw_memory(ax, schedule_data: dict, buffers: dict[str, list[BufferGroup]], memory_limit: int, x_limit: int) -> None:
    xs, ys = memory_profile(schedule_data["schedule"], buffers)
    ax.fill_between(xs, ys, step="post", color="#8ec1e8", alpha=0.6)
    ax.plot(xs, ys, drawstyle="steps-post", color="#2a78bd", linewidth=1.2)
    ax.axhline(memory_limit, color="red", linewidth=1.3)
    ax.text(
        8,
        memory_limit * 1.025,
        f"M={memory_limit}",
        color="red",
        fontsize=9,
        va="bottom",
        ha="left",
    )
    y_max = max(memory_limit * 1.08, max(ys) * 1.08 if ys else memory_limit)
    ax.set_ylim(0, y_max)
    ax.set_xlim(0, x_limit)
    ax.set_ylabel("Память", fontsize=10)
    ax.set_xlabel("Время", fontsize=10)
    ax.grid(True, axis="both", linestyle="--", linewidth=0.5, alpha=0.28)
    ax.tick_params(axis="both", labelsize=9)


def rounded_limit(values: list[int]) -> int:
    value = max(values)
    return int(math.ceil(value / 100.0) * 100)


def render_pair(args, algorithms: list[str], output: Path) -> None:
    root = Path(args.root)
    graph_name = args.graph_name
    graph_path = root / "build" / "final_exp" / args.dataset / f"{graph_name}.txt"
    _, children, buffers = load_graph(graph_path)

    schedules = {
        alg: load_schedule(resolve_schedule_path(root, args.dataset, graph_name, alg, args.seed), graph_name)
        for alg in algorithms
    }
    x_limit = args.x_limit or rounded_limit([int(data["makespan"]) for data in schedules.values()])

    fig, axes = plt.subplots(
        len(algorithms) * 2,
        1,
        figsize=(args.width, args.row_height * len(algorithms)),
        gridspec_kw={"height_ratios": sum(([2.75, 0.95] for _ in algorithms), [])},
        sharex=True,
    )
    fig.suptitle(graph_name, fontsize=15, fontweight="bold", y=0.995)

    for index, alg in enumerate(algorithms):
        ax_gantt = axes[index * 2]
        ax_memory = axes[index * 2 + 1]
        draw_gantt(
            ax_gantt,
            schedules[alg],
            graph_name,
            alg,
            args.processors,
            x_limit,
            children,
            args.dependencies,
        )
        draw_memory(ax_memory, schedules[alg], buffers, args.memory, x_limit)

    fig.tight_layout(rect=(0, 0, 1, 0.982), h_pad=0.7)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=args.dpi, bbox_inches="tight", pad_inches=0.05)
    plt.close(fig)
    print(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--graph-name", default="triadag20_210_N_uniform_12_1300")
    parser.add_argument("--dataset", default="decision_quality/new_triadags_time_P_and_M")
    parser.add_argument("--seed", type=int, default=101)
    parser.add_argument("--processors", type=int, default=12)
    parser.add_argument("--memory", type=int, default=1300)
    parser.add_argument("--x-limit", type=int, default=3100)
    parser.add_argument("--width", type=float, default=21.0)
    parser.add_argument("--row-height", type=float, default=5.7)
    parser.add_argument("--dpi", type=int, default=190)
    parser.add_argument("--dependencies", action="store_true")
    parser.add_argument("--output-dir", default=None)
    args = parser.parse_args()

    output_dir = (
        Path(args.output_dir)
        if args.output_dir
        else Path(args.root) / "build" / "final_exp_runtime" / "plots_final_exp" / "gantt_and_memory"
    )
    base = f"{args.graph_name}_seed{args.seed}"
    render_pair(args, ["sao", "sao_fed"], output_dir / f"{base}_sao_sao_fed_coursework.png")
    render_pair(args, ["greedy", "sao_fed"], output_dir / f"{base}_greedy_sao_fed_coursework.png")


if __name__ == "__main__":
    main()
