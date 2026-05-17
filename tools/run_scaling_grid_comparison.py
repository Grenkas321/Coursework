#!/usr/bin/env python3
import argparse
import json
import math
import re
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


HEADER = "prog_id prog_time weight_1: child_1 ... child_n, weight_2: child_1 ... child_n, ..."


@dataclass(frozen=True)
class Variant:
    key: str
    label: str
    algo_arg: str
    stochastic: bool


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def dump_json(path: Path, data: object) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def quote_bash_path(path: str) -> str:
    return "'" + path.replace("'", "'\"'\"'") + "'"


def windows_to_wsl(path: Path) -> str:
    drive = path.drive.rstrip(":").lower()
    suffix = path.as_posix().split(":", 1)[1]
    return f"/mnt/{drive}{suffix}"


def parse_groups(text: str) -> list[tuple[int, list[int]]]:
    groups: list[tuple[int, list[int]]] = []
    for chunk in text.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        match = re.match(r"(-?\d+)\s*:\s*(.*)$", chunk)
        if not match:
            continue
        weight = int(match.group(1))
        children = [int(x) for x in match.group(2).split()] if match.group(2).strip() else []
        groups.append((weight, children))
    return groups


def graph_stats(path: Path) -> dict:
    vertices = 0
    total_buffer_weight = 0
    max_output_weight = 0
    edges = 0
    producers = 0
    groups_count = 0

    for line in path.read_text(encoding="utf-8-sig").splitlines()[1:]:
        if not line.strip():
            continue
        vertices += 1
        parts = line.split(maxsplit=2)
        buffer_text = parts[2] if len(parts) >= 3 else "0:"
        groups = [(w, kids) for w, kids in parse_groups(buffer_text) if w > 0 and kids]
        if groups:
            producers += 1
            groups_count += len(groups)
            out_weight = sum(w for w, _kids in groups)
            total_buffer_weight += out_weight
            max_output_weight = max(max_output_weight, out_weight)
            edges += sum(len(kids) for _w, kids in groups)

    return {
        "vertices": vertices,
        "total_buffer_weight": total_buffer_weight,
        "max_output_weight": max_output_weight,
        "edges": edges,
        "producers": producers,
        "groups_count": groups_count,
    }


def build_memory_grid(total_buffer_weight: int, vertices: int, divisors: list[str]) -> list[dict]:
    values: dict[int, str] = {}
    x = max(1, total_buffer_weight)
    n = max(1, vertices)
    for token in divisors:
        token = token.strip().lower()
        if not token:
            continue
        if token == "n":
            denom = n
            label = "X/n"
        else:
            denom = max(1, int(token))
            label = "X" if denom == 1 else f"X/{denom}"
        memory = max(1, int(math.ceil(x / denom)))
        values[memory] = label
    return [{"memory": memory, "label": values[memory]} for memory in sorted(values)]


def prepare_variants(data_dir: Path, config_dir: Path, config_prefix: str, seed: int) -> list[Variant]:
    config_dir.mkdir(parents=True, exist_ok=True)

    aco_default = load_json(data_dir / "aco.json")
    aco_default["aco"]["seed"] = seed
    aco_default["aco"]["label"] = "aco_default"
    dump_json(config_dir / "aco_default.json", aco_default)

    aco_tuned = load_json(data_dir / "aco_best.json")
    aco_tuned["aco"]["seed"] = seed
    aco_tuned["aco"]["label"] = "aco_tuned"
    dump_json(config_dir / "aco_tuned.json", aco_tuned)

    sao_base = load_json(data_dir / "sao.json")
    sao_off = json.loads(json.dumps(sao_base))
    sao_off["sao"]["seed"] = seed
    sao_off["sao"]["fedorenko_idle_prob"] = 0.0
    sao_off["sao"]["label"] = "sao_off"
    dump_json(config_dir / "sao_off.json", sao_off)

    sao_fed = json.loads(json.dumps(sao_base))
    sao_fed["sao"]["seed"] = seed
    sao_fed["sao"]["fedorenko_idle_prob"] = 0.3
    sao_fed["sao"]["label"] = "sao_fed"
    dump_json(config_dir / "sao_fed.json", sao_fed)

    csao_base = load_json(data_dir / "csao.json")
    csao_off = json.loads(json.dumps(csao_base))
    csao_off["csao"]["seed"] = seed
    csao_off["csao"]["fedorenko_idle_prob"] = 0.0
    csao_off["csao"]["label"] = "csao_off"
    dump_json(config_dir / "csao_off.json", csao_off)

    csao_fed = json.loads(json.dumps(csao_base))
    csao_fed["csao"]["seed"] = seed
    csao_fed["csao"]["fedorenko_idle_prob"] = 0.3
    csao_fed["csao"]["label"] = "csao_fed"
    dump_json(config_dir / "csao_fed.json", csao_fed)

    return [
        Variant("greedy", "greedy", "greedy", False),
        Variant("aco_default", "aco_default", f"{config_prefix}/configs/aco_default.json", True),
        Variant("aco_tuned", "aco_tuned", f"{config_prefix}/configs/aco_tuned.json", True),
        Variant("sao_off", "sao_off", f"{config_prefix}/configs/sao_off.json", True),
        Variant("sao_fed", "sao_fed", f"{config_prefix}/configs/sao_fed.json", True),
        Variant("csao_off", "csao_off", f"{config_prefix}/configs/csao_off.json", True),
        Variant("csao_fed", "csao_fed", f"{config_prefix}/configs/csao_fed.json", True),
    ]


def discover_graphs(graphs_dir: Path, name_pattern: str) -> list[Path]:
    graphs = sorted(path for path in graphs_dir.glob("*.txt") if path.name != HEADER)
    if name_pattern:
        graphs = [path for path in graphs if re.search(name_pattern, path.stem)]
    return graphs


def build_graph_specs(graphs: list[Path], processors: list[int], divisors: list[str]) -> dict[str, dict]:
    specs: dict[str, dict] = {}
    for path in graphs:
        stats = graph_stats(path)
        memory_grid = build_memory_grid(stats["total_buffer_weight"], stats["vertices"], divisors)
        specs[path.stem] = {
            "graph_file": str(path),
            "vertices": stats["vertices"],
            "edges": stats["edges"],
            "processors": processors,
            "total_buffer_weight": stats["total_buffer_weight"],
            "max_output_weight": stats["max_output_weight"],
            "memory_grid": memory_grid,
        }
    return specs


def make_input_dirs(graph_specs: dict[str, dict], input_root: Path) -> dict[str, Path]:
    input_root.mkdir(parents=True, exist_ok=True)
    graph_inputs: dict[str, Path] = {}
    for graph, spec in graph_specs.items():
        dst_dir = input_root / graph
        dst_dir.mkdir(parents=True, exist_ok=True)
        src = Path(spec["graph_file"])
        shutil.copy2(src, dst_dir / src.name)
        graph_inputs[graph] = dst_dir
    return graph_inputs


def parse_result_json(json_path: Path, graph: str) -> tuple[Optional[int], Optional[float]]:
    if not json_path.exists():
        return None, None
    data = load_json(json_path)
    node = data.get(graph)
    if not node:
        return None, None
    return node.get("makespan"), node.get("runtime_sec")


def run_task(
    build_dir: Path,
    variant: Variant,
    graph: str,
    proc_count: int,
    memory: int,
    memory_label: str,
    input_dir: Path,
    output_root: Path,
    app_threads: int,
) -> dict:
    cell_dir = output_root / variant.key / graph / f"P{proc_count}_M{memory}"
    cell_dir.mkdir(parents=True, exist_ok=True)
    log_path = cell_dir / "run.log"
    expected_json = cell_dir / variant.label / "schedules" / "best" / f"{graph}.json"

    if expected_json.exists():
        makespan, runtime = parse_result_json(expected_json, graph)
        return {
            "variant": variant.key,
            "graph": graph,
            "processors": proc_count,
            "memory": memory,
            "memory_label": memory_label,
            "makespan": makespan,
            "runtime_sec": runtime,
            "status": "cached",
        }

    algo_arg = variant.algo_arg
    if algo_arg != "greedy":
        algo_arg = windows_to_wsl((build_dir / algo_arg).resolve())

    cmd = (
        f"cd {quote_bash_path(windows_to_wsl(build_dir))} && "
        f"./application --command run --algo {quote_bash_path(algo_arg)} "
        f"--input {quote_bash_path(windows_to_wsl(input_dir))} "
        f"--output {quote_bash_path(windows_to_wsl(cell_dir))} "
        f"--processors {proc_count} --memory {memory} --threads {app_threads} --dups 1 --sample 1 --batch 1"
    )
    proc = subprocess.run(
        ["wsl", "bash", "-lc", cmd],
        cwd=build_dir,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    log_path.write_text(stdout + "\n--- STDERR ---\n" + stderr, encoding="utf-8")
    makespan, runtime = parse_result_json(expected_json, graph)
    combined = (stdout + "\n" + stderr).lower()
    memory_too_small = (
        "memory limit" in combined and "too small" in combined
    ) or (
        "ограничению по памяти" in combined
    )
    status = "ok" if makespan is not None else "infeasible"
    if proc.returncode != 0 and makespan is None and not memory_too_small:
        status = "failed"
    return {
        "variant": variant.key,
        "graph": graph,
        "processors": proc_count,
        "memory": memory,
        "memory_label": memory_label,
        "makespan": makespan,
        "runtime_sec": runtime,
        "status": status,
        "returncode": proc.returncode,
    }


def load_existing_results(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8"))


def write_outputs(out_root: Path, graph_specs: dict[str, dict], variants: list[Variant], results: list[dict]) -> None:
    results_sorted = sorted(
        results,
        key=lambda x: (x["graph"], x["processors"], x["memory"], x["variant"]),
    )
    dump_json(out_root / "results_long.json", results_sorted)

    csv_lines = ["graph,variant,processors,memory,memory_label,makespan,runtime_sec,status,returncode"]
    for row in results_sorted:
        csv_lines.append(
            f"{row['graph']},{row['variant']},{row['processors']},{row['memory']},{row['memory_label']},"
            f"{'' if row['makespan'] is None else row['makespan']},"
            f"{'' if row.get('runtime_sec') is None else row['runtime_sec']},"
            f"{row['status']},{row.get('returncode', '')}"
        )
    (out_root / "results_long.csv").write_text("\n".join(csv_lines) + "\n", encoding="utf-8")

    table = {}
    by_key = {(r["variant"], r["graph"], r["processors"], r["memory"]): r for r in results_sorted}
    for graph, spec in graph_specs.items():
        graph_table = {}
        for variant in variants:
            rows = []
            for proc_count in spec["processors"]:
                row = {"P": proc_count}
                for mem_spec in spec["memory_grid"]:
                    item = by_key.get((variant.key, graph, proc_count, mem_spec["memory"]))
                    key = f"{mem_spec['label']}={mem_spec['memory']}"
                    row[key] = "-" if item is None or item["makespan"] is None else item["makespan"]
                rows.append(row)
            graph_table[variant.key] = rows
        table[graph] = {
            "meta": spec,
            "variants": graph_table,
        }
    dump_json(out_root / "tables.json", table)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--graphs-dir", required=True)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--app-threads", type=int, default=1)
    parser.add_argument("--processors", default="2,3,5,10,20")
    parser.add_argument("--memory-divisors", default="n,20,10,5,4,3,2,1")
    parser.add_argument("--graph-pattern", default="")
    parser.add_argument("--limit-graphs", type=int, default=0)
    parser.add_argument("--limit-tasks", type=int, default=0)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--out-name", default="tmp_scaling_grid_comparison")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    build_dir = Path(args.build_dir).resolve()
    graphs_dir = Path(args.graphs_dir).resolve()
    out_root = build_dir / args.out_name
    input_root = out_root / "inputs"
    config_root = out_root / "configs"
    out_root.mkdir(parents=True, exist_ok=True)

    processors = [int(x.strip()) for x in args.processors.split(",") if x.strip()]
    divisors = [x.strip() for x in args.memory_divisors.split(",") if x.strip()]

    graphs = discover_graphs(graphs_dir, args.graph_pattern)
    if args.limit_graphs > 0:
        graphs = graphs[: args.limit_graphs]
    graph_specs = build_graph_specs(graphs, processors, divisors)
    dump_json(out_root / "graph_specs.json", graph_specs)

    variants = prepare_variants(build_dir.parent / "data", config_root, args.out_name, args.seed)
    graph_inputs = make_input_dirs(graph_specs, input_root)

    existing_results = load_existing_results(out_root / "results_long.json") if args.resume else []
    done_keys = {
        (row["variant"], row["graph"], row["processors"], row["memory"])
        for row in existing_results
    }
    results = list(existing_results)

    tasks = []
    for graph, spec in graph_specs.items():
        for proc_count in spec["processors"]:
            for mem_spec in spec["memory_grid"]:
                for variant in variants:
                    key = (variant.key, graph, proc_count, mem_spec["memory"])
                    if key in done_keys:
                        continue
                    tasks.append((variant, graph, proc_count, mem_spec["memory"], mem_spec["label"], graph_inputs[graph]))

    if args.limit_tasks > 0:
        tasks = tasks[: args.limit_tasks]

    total = len(tasks)
    completed = 0
    if total == 0:
        write_outputs(out_root, graph_specs, variants, results)
        print("No pending tasks.")
        return

    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        future_map = {
            executor.submit(
                run_task,
                build_dir,
                variant,
                graph,
                proc_count,
                memory,
                memory_label,
                input_dir,
                out_root,
                args.app_threads,
            ): (variant.key, graph, proc_count, memory)
            for variant, graph, proc_count, memory, memory_label, input_dir in tasks
        }
        for future in as_completed(future_map):
            result = future.result()
            results.append(result)
            completed += 1
            print(
                f"[{completed}/{total}] {result['graph']} {result['variant']} "
                f"P={result['processors']} M={result['memory']} ({result['memory_label']}) "
                f"-> {result['makespan']} [{result['status']}]"
            )
            if completed % 10 == 0:
                write_outputs(out_root, graph_specs, variants, results)

    write_outputs(out_root, graph_specs, variants, results)


if __name__ == "__main__":
    main()
