import argparse
import json
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


GRAPH_SPECS = {
    "dag2_new_time": {
        "processors": [2, 4, 10],
        "memory": [1228800, 1240000, 1280000, 1348950, 1470000, 1500000, 2000000],
    },
    "dagB0_new_time": {
        "processors": [2, 4, 10],
        "memory": [752, 782, 790, 800, 900, 1000, 2000],
    },
    "dag0_new_time": {
        "processors": [2, 4, 10, 20],
        "memory": [2739000, 2800000, 2900000, 3070000, 3201000, 3215400, 3230000,
                   3350000, 3400000, 3450000, 4000000, 5000000],
    },
    "triadag20_7_new_time": {
        "processors": [2, 4, 10, 20],
        "memory": [1181, 1210, 1219, 1250, 1281, 1290, 1310, 1320, 1350, 1500, 2000, 3000],
    },
}


@dataclass(frozen=True)
class Variant:
    key: str
    label: str
    algo_arg: str
    stochastic: bool


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def dump_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def prepare_variants(data_dir: Path, config_dir: Path, seed: int) -> list[Variant]:
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

    sao_on = json.loads(json.dumps(sao_base))
    sao_on["sao"]["seed"] = seed
    sao_on["sao"]["fedorenko_idle_prob"] = 0.3
    sao_on["sao"]["label"] = "sao_fed"
    dump_json(config_dir / "sao_fed.json", sao_on)

    csao_base = load_json(data_dir / "csao.json")
    csao_off = json.loads(json.dumps(csao_base))
    csao_off["csao"]["seed"] = seed
    csao_off["csao"]["fedorenko_idle_prob"] = 0.0
    csao_off["csao"]["label"] = "csao_off"
    dump_json(config_dir / "csao_off.json", csao_off)

    csao_on = json.loads(json.dumps(csao_base))
    csao_on["csao"]["seed"] = seed
    csao_on["csao"]["fedorenko_idle_prob"] = 0.3
    csao_on["csao"]["label"] = "csao_fed"
    dump_json(config_dir / "csao_fed.json", csao_on)

    return [
        Variant("greedy", "greedy", "greedy", False),
        Variant("aco_default", "aco_default", "tmp_coursework_comparison/configs/aco_default.json", True),
        Variant("aco_tuned", "aco_tuned", "tmp_coursework_comparison/configs/aco_tuned.json", True),
        Variant("sao_off", "sao_off", "tmp_coursework_comparison/configs/sao_off.json", True),
        Variant("sao_fed", "sao_fed", "tmp_coursework_comparison/configs/sao_fed.json", True),
        Variant("csao_off", "csao_off", "tmp_coursework_comparison/configs/csao_off.json", True),
        Variant("csao_fed", "csao_fed", "tmp_coursework_comparison/configs/csao_fed.json", True),
    ]


def quote_bash_path(path: str) -> str:
    return "'" + path.replace("'", "'\"'\"'") + "'"


def windows_to_wsl(path: Path) -> str:
    drive = path.drive.rstrip(":").lower()
    suffix = path.as_posix().split(":", 1)[1]
    return f"/mnt/{drive}{suffix}"


def make_input_dirs(build_dir: Path, graphs_dir: Path, graph_names: list[str], input_root: Path) -> dict[str, Path]:
    input_root.mkdir(parents=True, exist_ok=True)
    graph_inputs = {}
    for graph in graph_names:
        dst_dir = input_root / graph
        dst_dir.mkdir(parents=True, exist_ok=True)
        src = graphs_dir / f"{graph}.txt"
        shutil.copy2(src, dst_dir / f"{graph}.txt")
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


def run_task(build_dir: Path,
             variant: Variant,
             graph: str,
             proc_count: int,
             memory: int,
             input_dir: Path,
             output_root: Path) -> dict:
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
        f"--processors {proc_count} --memory {memory} --threads 1 --dups 1 --sample 1 --batch 1"
    )
    full_cmd = ["wsl", "bash", "-lc", cmd]
    proc = subprocess.run(full_cmd, cwd=build_dir, capture_output=True, text=True, encoding="utf-8", errors="replace")
    log_path.write_text((proc.stdout or "") + "\n--- STDERR ---\n" + (proc.stderr or ""), encoding="utf-8")
    makespan, runtime = parse_result_json(expected_json, graph)
    status = "ok" if makespan is not None else "infeasible"
    if proc.returncode != 0 and makespan is None:
        status = "failed"
    return {
        "variant": variant.key,
        "graph": graph,
        "processors": proc_count,
        "memory": memory,
        "makespan": makespan,
        "runtime_sec": runtime,
        "status": status,
        "returncode": proc.returncode,
    }


def write_outputs(out_root: Path, graph_specs: dict, variants: list[Variant], results: list[dict]) -> None:
    results_sorted = sorted(results, key=lambda x: (x["variant"], x["graph"], x["processors"], x["memory"]))
    dump_json(out_root / "results_long.json", results_sorted)

    csv_lines = ["variant,graph,processors,memory,makespan,runtime_sec,status,returncode"]
    for row in results_sorted:
        csv_lines.append(
            f"{row['variant']},{row['graph']},{row['processors']},{row['memory']},"
            f"{'' if row['makespan'] is None else row['makespan']},"
            f"{'' if row.get('runtime_sec') is None else row['runtime_sec']},"
            f"{row['status']},{row.get('returncode', '')}"
        )
    (out_root / "results_long.csv").write_text("\n".join(csv_lines) + "\n", encoding="utf-8")

    table = {}
    by_key = {(r["variant"], r["graph"], r["processors"], r["memory"]): r for r in results_sorted}
    for variant in variants:
        table[variant.key] = {}
        for graph, spec in graph_specs.items():
            rows = []
            for proc_count in spec["processors"]:
                row = {"P": proc_count}
                for memory in spec["memory"]:
                    item = by_key.get((variant.key, graph, proc_count, memory))
                    row[str(memory)] = "-" if item is None or item["makespan"] is None else item["makespan"]
                rows.append(row)
            table[variant.key][graph] = {
                "memory": spec["memory"],
                "rows": rows,
            }
    dump_json(out_root / "tables.json", table)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--graphs-dir", required=True)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--graph", action="append", dest="graphs")
    args = parser.parse_args()

    build_dir = Path(args.build_dir).resolve()
    graphs_dir = Path(args.graphs_dir).resolve()

    if args.graphs:
        graph_specs = {name: GRAPH_SPECS[name] for name in args.graphs}
    else:
        graph_specs = dict(GRAPH_SPECS)

    out_root = build_dir / "tmp_coursework_comparison"
    input_root = out_root / "inputs"
    config_root = out_root / "configs"
    out_root.mkdir(parents=True, exist_ok=True)

    variants = prepare_variants(build_dir.parent / "data", config_root, args.seed)
    graph_inputs = make_input_dirs(build_dir, graphs_dir, list(graph_specs.keys()), input_root)

    tasks = []
    for variant in variants:
        for graph, spec in graph_specs.items():
            for proc_count in spec["processors"]:
                for memory in spec["memory"]:
                    tasks.append((variant, graph, proc_count, memory, graph_inputs[graph]))

    results = []
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        future_map = {
            executor.submit(run_task, build_dir, variant, graph, proc_count, memory, input_dir, out_root): (variant.key, graph, proc_count, memory)
            for variant, graph, proc_count, memory, input_dir in tasks
        }
        completed = 0
        total = len(future_map)
        for future in as_completed(future_map):
            result = future.result()
            results.append(result)
            completed += 1
            print(f"[{completed}/{total}] {result['variant']} {result['graph']} P={result['processors']} M={result['memory']} -> {result['makespan']} ({result['status']})")

    write_outputs(out_root, graph_specs, variants, results)


if __name__ == "__main__":
    main()
