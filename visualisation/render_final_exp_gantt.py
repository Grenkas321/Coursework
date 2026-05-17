import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch


def load_graph(path):
    children = {}
    buffer_sizes = {}
    nodes = []
    with path.open(encoding="utf-8") as f:
        lines = f.readlines()[1:]

    for line in lines:
        parts = line.split()
        if not parts:
            continue
        node = parts[0]
        nodes.append(node)
        children[node] = []
        buffers = " ".join(parts[2:]).split(", ")
        if buffers == ["0:"]:
            continue
        for buffer_index, buffer in enumerate(buffers, start=1):
            size, vertexes = buffer.split(": ")
            for child in vertexes.split():
                children[node].append(child)
                buffer_sizes[(node, child)] = int(size)
    return nodes, children, buffer_sizes


def load_schedule(path, graph_name):
    with path.open(encoding="utf-8") as f:
        data = json.load(f)
    return data[graph_name]


def resolve_schedule_path(root, dataset, graph_name, algorithm, seed):
    label = f"{algorithm}_s{seed}"
    seeded_path = (
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
    if seeded_path.exists():
        return seeded_path

    label = algorithm
    runtime_path = (
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
    if runtime_path.exists():
        return runtime_path

    raise FileNotFoundError(f"Schedule for {algorithm} was not found: {seeded_path} or {runtime_path}")


def memory_usage(schedule, children, buffer_sizes):
    events = {}
    max_finish = 0
    for vertex, params in schedule.items():
        start = int(params["start"])
        finish = int(params["finish"])
        max_finish = max(max_finish, finish)
        events.setdefault(start, []).append((vertex, "start"))
        events.setdefault(finish, []).append((vertex, "finish"))

    alive = {}
    xs = []
    ys = []
    for t in range(max_finish + 1):
        for vertex, status in events.get(t, []):
            if status == "start":
                grouped = {}
                for child in children.get(vertex, []):
                    grouped.setdefault(buffer_sizes[(vertex, child)], []).append(child)
                for index, (size, child_list) in enumerate(grouped.items(), start=1):
                    alive[(vertex, index, size)] = child_list
            else:
                for key in list(alive):
                    if vertex in alive[key]:
                        alive[key].remove(vertex)
                        if not alive[key]:
                            del alive[key]
        xs.append(t)
        ys.append(sum(size for _, _, size in alive))
    return xs, ys


def draw_dependencies(ax, schedule, children, linewidth=0.45, alpha=0.23):
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
            same_processor = y1 == y2
            if same_processor:
                start = (x1, y1 + 0.08)
                end = (x2, y2 + 0.08)
                connection = "arc3,rad=0.0"
            else:
                start = (x1, y1)
                end = (x2, y2)
                direction = -1 if y2 > y1 else 1
                curve = min(0.18, 0.04 + gap / 4000.0) * direction
                connection = f"arc3,rad={curve}"

            arrow = FancyArrowPatch(
                start,
                end,
                arrowstyle="-|>",
                mutation_scale=max(7, linewidth * 7),
                linewidth=linewidth,
                color="black",
                alpha=alpha,
                connectionstyle=connection,
                zorder=4,
            )
            ax.add_patch(arrow)


def draw_one(
    ax_gantt,
    ax_mem,
    graph_name,
    graph_path,
    schedule_path,
    algorithm,
    seed,
    processors,
    memory,
    time_limit=None,
    dependencies=False,
    dependency_linewidth=0.45,
    dependency_alpha=0.23,
):
    nodes, children, buffer_sizes = load_graph(graph_path)
    data = load_schedule(schedule_path, graph_name)
    schedule = data["schedule"]
    makespan = int(data["makespan"])
    runtime = float(data["runtime_sec"])

    colors = plt.cm.tab20.colors
    tasks_by_processor = {p: [] for p in range(processors)}
    for vertex, params in schedule.items():
        processor = int(params["processor"])
        start = int(params["start"])
        finish = int(params["finish"])
        tasks_by_processor.setdefault(processor, []).append((start, finish - start, vertex))

    for processor in range(processors):
        tasks = sorted(tasks_by_processor.get(processor, []))
        for start, duration, vertex in tasks:
            color = colors[int(vertex) % len(colors)]
            ax_gantt.broken_barh([(start, duration)], (processor + 0.15, 0.7), facecolors=[color], zorder=3)
            if duration >= max(25, makespan * 0.012):
                ax_gantt.text(
                    start + duration / 2,
                    processor + 0.5,
                    vertex,
                    ha="center",
                    va="center",
                    fontsize=5,
                    color="white",
                    zorder=5,
                )

    x_limit = time_limit or makespan
    ax_gantt.set_xlim(0, x_limit)
    ax_gantt.set_ylim(0, processors)
    ax_gantt.set_yticks([p + 0.5 for p in range(processors)])
    ax_gantt.set_yticklabels([f"P{p + 1}" for p in range(processors)])
    ax_gantt.invert_yaxis()
    ax_gantt.grid(True, axis="x", linestyle="--", alpha=0.35)
    ax_gantt.set_title(f"{algorithm}, seed={seed}: makespan={makespan}, time={runtime:.2f}s")
    if dependencies:
        draw_dependencies(ax_gantt, schedule, children, linewidth=dependency_linewidth, alpha=dependency_alpha)

    xs, ys = memory_usage(schedule, children, buffer_sizes)
    ax_mem.fill_between(xs, ys, step="post", alpha=0.35)
    ax_mem.plot(xs, ys, linewidth=1.2, drawstyle="steps-post")
    ax_mem.axhline(memory, color="red", linewidth=1.2)
    ax_mem.text(0, memory * 1.02, f"M={memory}", color="red", fontsize=8)
    ax_mem.set_xlim(0, x_limit)
    ax_mem.set_ylabel("Memory")
    ax_mem.set_xlabel("Time")
    ax_mem.grid(True, axis="y", linestyle="--", alpha=0.3)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--graph-name", default="triadag20_210_N_uniform_12_1300")
    parser.add_argument("--dataset", default="decision_quality/new_triadags_time_P_and_M")
    parser.add_argument("--seed", type=int, default=101)
    parser.add_argument("--algorithms", nargs="+", default=["sao", "sao_fed"])
    parser.add_argument("--processors", type=int, default=12)
    parser.add_argument("--memory", type=int, default=1300)
    parser.add_argument("--dependencies", action="store_true")
    parser.add_argument("--dpi", type=int, default=220)
    parser.add_argument("--width", type=float, default=18)
    parser.add_argument("--row-height", type=float, default=4.8)
    parser.add_argument("--dependency-linewidth", type=float, default=0.45)
    parser.add_argument("--dependency-alpha", type=float, default=0.23)
    parser.add_argument("--output", default=None)
    args = parser.parse_args()

    root = Path(args.root)
    graph_path = root / "build" / "final_exp" / args.dataset / f"{args.graph_name}.txt"
    rows = len(args.algorithms)
    fig, axes = plt.subplots(
        rows,
        2,
        figsize=(args.width, args.row_height * rows),
        gridspec_kw={"width_ratios": [3, 1]},
    )
    if rows == 1:
        axes = [axes]

    schedule_paths = {}
    time_limit = 0
    for algorithm in args.algorithms:
        schedule_path = resolve_schedule_path(root, args.dataset, args.graph_name, algorithm, args.seed)
        schedule_paths[algorithm] = schedule_path
        time_limit = max(time_limit, int(load_schedule(schedule_path, args.graph_name)["makespan"]))

    for row, algorithm in enumerate(args.algorithms):
        draw_one(
            axes[row][0],
            axes[row][1],
            args.graph_name,
            graph_path,
            schedule_paths[algorithm],
            algorithm,
            args.seed,
            args.processors,
            args.memory,
            time_limit=time_limit,
            dependencies=args.dependencies,
            dependency_linewidth=args.dependency_linewidth,
            dependency_alpha=args.dependency_alpha,
        )

    fig.suptitle(args.graph_name, fontsize=14, fontweight="bold")
    fig.tight_layout()
    output = Path(args.output) if args.output else root / "build" / "final_exp_runtime" / "plots_final_exp" / "gantt" / f"{args.graph_name}_seed{args.seed}_{'_'.join(args.algorithms)}.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=args.dpi, bbox_inches="tight")
    print(output)


if __name__ == "__main__":
    main()
