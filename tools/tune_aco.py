#!/usr/bin/env python3
import argparse
import csv
import itertools
import json
import math
import shutil
import statistics
import subprocess
import sys
from pathlib import Path


def expand_axis(spec):
    if isinstance(spec, list):
        return spec
    if not isinstance(spec, dict):
        raise ValueError(f"Unsupported search-space entry: {spec!r}")

    kind = spec.get("kind", "float")
    if kind == "categorical":
        values = spec.get("values")
        if not isinstance(values, list) or not values:
            raise ValueError(f"Categorical axis requires non-empty 'values': {spec!r}")
        return values

    low = spec["low"]
    high = spec["high"]
    step = spec.get("step")
    if step in (None, 0):
        raise ValueError(f"Interval axis requires non-zero 'step': {spec!r}")

    values = []
    current = low
    if kind == "int":
        current = int(current)
        high = int(high)
        step = int(step)
        while current <= high:
            values.append(current)
            current += step
    else:
        current = float(current)
        high = float(high)
        step = float(step)
        while current <= high + 1e-12:
            values.append(round(current, 10))
            current += step
    return values


def iter_grid(space):
    keys = list(space.keys())
    axes = [expand_axis(space[key]) for key in keys]
    for combo in itertools.product(*axes):
        yield dict(zip(keys, combo))


def clone_jsonish(value):
    return json.loads(json.dumps(value))


def is_numeric_space(spec):
    if not isinstance(spec, dict):
        return False
    return spec.get("kind", "float") in {"float", "int"}


def space_low(spec):
    return float(spec["low"])


def space_high(spec):
    return float(spec["high"])


def space_step(spec):
    step = spec.get("step")
    return None if step is None else float(step)


def to_wsl_path(path):
    path = Path(path).resolve()
    drive = path.drive.rstrip(":").lower()
    tail = str(path).replace("\\", "/")[2:]
    return f"/mnt/{drive}{tail}"


def load_makespans(best_dir):
    results = {}
    for path in sorted(best_dir.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        top = next(iter(data.values()))
        results[path.stem] = {
            "makespan": top.get("makespan", top.get("cost")),
            "runtime_sec": top.get("runtime_sec", 0.0),
        }
    return results


def make_config(path, label, params):
    payload = {
        label: {
            "baseline": "greedy",
            **params,
            "label": label,
        }
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def mix_seed(base_seed, trial_idx, repeat_idx):
    seed = int(base_seed) & 0xFFFFFFFF
    seed ^= (int(trial_idx) + 1) * 0x9E3779B1
    seed ^= (int(repeat_idx) + 1) * 0x85EBCA77
    seed ^= seed >> 16
    return seed & 0xFFFFFFFF


def run_application(args, config_path, output_dir):
    build_dir = Path(args.build_dir).resolve()
    input_dir = Path(args.input_dir).resolve()
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    cmd = (
        f"./application --command run --algo {to_wsl_path(config_path)} "
        f"--input {to_wsl_path(input_dir)} --output {to_wsl_path(output_dir)} "
        f"--processors {args.processors} --memory {args.memory} "
        f"--dups {args.dups} --threads {args.threads} "
        f"--sample {args.sample} --batch {args.batch}"
    )
    if args.runner == "wsl":
        full_cmd = [
            "wsl",
            "bash",
            "-lc",
            f"cd {to_wsl_path(build_dir)} && {cmd}",
        ]
    else:
        full_cmd = cmd.split()

    completed = subprocess.run(full_cmd, capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(
            "ACO tuning run failed.\n"
            f"Command: {' '.join(full_cmd)}\n"
            f"STDOUT:\n{completed.stdout}\n"
            f"STDERR:\n{completed.stderr}"
        )
    return completed.stdout


def run_builtin_algo(args, algo_name, output_dir):
    build_dir = Path(args.build_dir).resolve()
    input_dir = Path(args.input_dir).resolve()
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    cmd = (
        f"./application --command run --algo {algo_name} "
        f"--input {to_wsl_path(input_dir)} --output {to_wsl_path(output_dir)} "
        f"--processors {args.processors} --memory {args.memory} "
        f"--dups {args.dups} --threads {args.threads} "
        f"--sample {args.sample} --batch {args.batch}"
    )
    if args.runner == "wsl":
        full_cmd = [
            "wsl",
            "bash",
            "-lc",
            f"cd {to_wsl_path(build_dir)} && {cmd}",
        ]
    else:
        full_cmd = cmd.split()

    completed = subprocess.run(full_cmd, capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(
            f"{algo_name} baseline run failed.\n"
            f"Command: {' '.join(full_cmd)}\n"
            f"STDOUT:\n{completed.stdout}\n"
            f"STDERR:\n{completed.stderr}"
        )
    return completed.stdout


def objective_from_results(results, reference, runtime_weight):
    makespans = [entry["makespan"] for entry in results.values()]
    mean_makespan = statistics.fmean(makespans) if makespans else math.inf
    runtimes = [entry["runtime_sec"] for entry in results.values()]
    mean_runtime_sec = statistics.fmean(runtimes) if runtimes else math.inf

    rel_gap = None
    if reference:
        gaps = []
        for name, entry in results.items():
            if name not in reference:
                continue
            ref_value = reference[name]["makespan"]
            if ref_value <= 0:
                continue
            gaps.append((entry["makespan"] - ref_value) / ref_value)
        if gaps:
            rel_gap = statistics.fmean(gaps)
    quality_objective = rel_gap if rel_gap is not None else mean_makespan
    runtime_penalty = runtime_weight * mean_runtime_sec
    objective = quality_objective + runtime_penalty
    return {
        "mean_makespan": mean_makespan,
        "mean_runtime_sec": mean_runtime_sec,
        "mean_rel_gap": rel_gap,
        "quality_objective": quality_objective,
        "runtime_penalty": runtime_penalty,
        "objective": objective,
    }


def compute_greedy_baseline(args, reference):
    work_root = Path(args.work_root).resolve()
    baseline_root = work_root / "_greedy_baseline"
    if baseline_root.exists():
        shutil.rmtree(baseline_root)
    run_builtin_algo(args, "greedy", baseline_root)
    best_dir = baseline_root / "greedy" / "schedules" / "best"
    results = load_makespans(best_dir)
    metrics = objective_from_results(results, reference, args.runtime_weight)
    return {
        "algo": "greedy",
        "results": results,
        **metrics,
        "baseline_root": str(baseline_root),
    }


def evaluate_trial(
    args,
    params,
    trial_idx,
    reference,
    greedy_baseline=None,
    stage_idx=None,
    stage_label=None,
    active_params=None,
    fixed_params=None,
    search_space_snapshot=None,
):
    work_root = Path(args.work_root).resolve()
    trial_root = work_root / f"trial_{trial_idx:03d}"
    if trial_root.exists():
        shutil.rmtree(trial_root)
    trial_root.mkdir(parents=True)

    repeat_summaries = []
    for repeat_idx in range(args.repeats):
        repeat_seed = mix_seed(args.seed_base, trial_idx, repeat_idx)
        repeat_label = f"{args.label_prefix}_{trial_idx:03d}_r{repeat_idx:02d}"
        config_path = trial_root / f"aco_config_r{repeat_idx:02d}.json"
        repeat_params = {**params, "seed": repeat_seed}
        make_config(config_path, repeat_label, repeat_params)
        repeat_output = trial_root / f"repeat_{repeat_idx:02d}" / "output"
        run_application(args, config_path, repeat_output)
        best_dir = repeat_output / repeat_label / "schedules" / "best"
        results = load_makespans(best_dir)
        metrics = objective_from_results(results, reference, args.runtime_weight)
        repeat_summaries.append({
            "repeat": repeat_idx,
            "seed": repeat_seed,
            "label": repeat_label,
            "params": repeat_params,
            "results": results,
            **metrics,
        })

    mean_makespan = statistics.fmean(item["mean_makespan"] for item in repeat_summaries)
    min_makespan = min(item["mean_makespan"] for item in repeat_summaries)
    max_makespan = max(item["mean_makespan"] for item in repeat_summaries)
    std_makespan = statistics.pstdev(item["mean_makespan"] for item in repeat_summaries) if len(repeat_summaries) > 1 else 0.0
    mean_runtime_sec = statistics.fmean(item["mean_runtime_sec"] for item in repeat_summaries)
    min_runtime_sec = min(item["mean_runtime_sec"] for item in repeat_summaries)
    max_runtime_sec = max(item["mean_runtime_sec"] for item in repeat_summaries)
    std_runtime_sec = statistics.pstdev(item["mean_runtime_sec"] for item in repeat_summaries) if len(repeat_summaries) > 1 else 0.0
    min_repeat_objective = min(item["objective"] for item in repeat_summaries)
    max_repeat_objective = max(item["objective"] for item in repeat_summaries)

    rel_gaps = [item["mean_rel_gap"] for item in repeat_summaries if item["mean_rel_gap"] is not None]
    mean_rel_gap = statistics.fmean(rel_gaps) if rel_gaps else None
    min_rel_gap = min(rel_gaps) if rel_gaps else None
    max_rel_gap = max(rel_gaps) if rel_gaps else None
    std_rel_gap = statistics.pstdev(rel_gaps) if len(rel_gaps) > 1 else 0.0

    quality_objective = mean_rel_gap if mean_rel_gap is not None else mean_makespan
    runtime_penalty = args.runtime_weight * mean_runtime_sec
    objective = quality_objective + runtime_penalty
    return {
        "trial": trial_idx,
        "label": f"{args.label_prefix}_{trial_idx:03d}",
        "stage": stage_idx,
        "stage_label": stage_label,
        "active_params": [] if active_params is None else list(active_params),
        "fixed_params": {} if fixed_params is None else clone_jsonish(fixed_params),
        "search_space": None if search_space_snapshot is None else clone_jsonish(search_space_snapshot),
        "params": params,
        "repeat_summaries": repeat_summaries,
        "mean_makespan": mean_makespan,
        "min_makespan": min_makespan,
        "max_makespan": max_makespan,
        "std_makespan": std_makespan,
        "mean_runtime_sec": mean_runtime_sec,
        "min_runtime_sec": min_runtime_sec,
        "max_runtime_sec": max_runtime_sec,
        "std_runtime_sec": std_runtime_sec,
        "mean_rel_gap": mean_rel_gap,
        "min_rel_gap": min_rel_gap,
        "max_rel_gap": max_rel_gap,
        "std_rel_gap": std_rel_gap,
        "quality_objective": quality_objective,
        "runtime_penalty": runtime_penalty,
        "objective": objective,
        "min_repeat_objective": min_repeat_objective,
        "max_repeat_objective": max_repeat_objective,
        "trial_root": str(trial_root),
        "greedy_baseline": greedy_baseline,
    }


def print_trial_summary(prefix, index, total, summary):
    gap_txt = (
        f"{summary['mean_rel_gap']:.4%}"
        if summary["mean_rel_gap"] is not None
        else "n/a"
    )
    print(
        f"[{prefix} {index}/{total}] objective={summary['objective']:.6f} "
        f"quality={summary['quality_objective']:.6f} runtime_penalty={summary['runtime_penalty']:.6f} "
        f"mean_makespan={summary['mean_makespan']:.3f} mean_runtime_sec={summary['mean_runtime_sec']:.3f} "
        f"mean_rel_gap={gap_txt} "
        f"params={summary['params']}"
    )


def run_grid(args, space, reference, greedy_baseline):
    trials = list(iter_grid(space))
    summaries = []
    for idx, params in enumerate(trials):
        summary = evaluate_trial(
            args,
            params,
            idx,
            reference,
            greedy_baseline=greedy_baseline,
            stage_idx=0,
            stage_label="grid",
            active_params=list(space.keys()),
            fixed_params={},
            search_space_snapshot=space,
        )
        summaries.append(summary)
        print_trial_summary("grid", idx + 1, len(trials), summary)
    return summaries


def make_sampler(args, optuna, seed, startup_trials=None):
    if args.sampler == "tpe":
        return optuna.samplers.TPESampler(
            seed=seed,
            n_startup_trials=args.startup_trials if startup_trials is None else startup_trials,
            multivariate=args.multivariate_tpe,
        )
    if args.sampler == "random":
        return optuna.samplers.RandomSampler(seed=seed)
    raise ValueError(f"Unsupported sampler: {args.sampler}")


def suggest_from_spec(trial, key, spec):
    if isinstance(spec, list):
        return trial.suggest_categorical(key, spec)
    if not isinstance(spec, dict):
        raise ValueError(f"Unsupported search-space entry: {spec!r}")
    kind = spec.get("kind", "float")
    if kind == "categorical":
        return trial.suggest_categorical(key, spec["values"])
    low = spec["low"]
    high = spec["high"]
    step = spec.get("step")
    log = bool(spec.get("log", False))
    if kind == "int":
        return trial.suggest_int(key, int(low), int(high), step=int(step or 1), log=log)
    return trial.suggest_float(key, float(low), float(high), step=step, log=log)


def build_study_kwargs(args, sampler, stage_idx=None):
    kwargs = {
        "direction": "minimize",
        "sampler": sampler,
    }
    if args.study_storage:
        kwargs["storage"] = args.study_storage
    if args.study_name:
        if stage_idx is None:
            kwargs["study_name"] = args.study_name
        else:
            kwargs["study_name"] = f"{args.study_name}_stage_{stage_idx:02d}"
    if args.study_storage and args.study_name:
        kwargs["load_if_exists"] = True
    return kwargs


def run_optuna(args, space, reference, greedy_baseline):
    try:
        import optuna
    except ImportError as exc:
        raise RuntimeError(
            "Mode 'optuna' requested, but optuna is not installed. "
            "Install it first with 'py -3 -m pip install optuna' or use '--mode grid'."
        ) from exc

    keys = list(space.keys())
    summaries = []

    def objective(trial):
        params = {key: suggest_from_spec(trial, key, space[key]) for key in keys}
        summary = evaluate_trial(
            args,
            params,
            trial.number,
            reference,
            greedy_baseline=greedy_baseline,
            stage_idx=0,
            stage_label="optuna",
            active_params=keys,
            fixed_params={},
            search_space_snapshot=space,
        )
        summaries.append(summary)
        return summary["objective"]

    sampler = make_sampler(args, optuna, args.optuna_seed)
    study = optuna.create_study(**build_study_kwargs(args, sampler))
    study.optimize(objective, n_trials=args.trials)
    return summaries


def build_refined_space(space, best_params, step_divisor, radius_steps):
    refined = clone_jsonish(space)
    for key, spec in refined.items():
        center = best_params.get(key)
        if isinstance(spec, list):
            refined[key] = [center] if center in spec else spec[:]
            continue
        if not isinstance(spec, dict):
            continue
        kind = spec.get("kind", "float")
        if kind == "categorical":
            values = spec.get("values", [])
            refined[key]["values"] = [center] if center in values else list(values)
            continue
        if kind not in {"float", "int"} or center is None:
            continue

        low = float(spec["low"])
        high = float(spec["high"])
        step = spec.get("step")
        if step in (None, 0):
            continue
        step_value = float(step)
        radius = abs(step_value) * max(0.0, float(radius_steps))
        local_low = max(low, float(center) - radius)
        local_high = min(high, float(center) + radius)

        if kind == "int":
            refined_step = max(1, int(round(abs(step_value) / max(float(step_divisor), 1.0))))
            local_low = int(math.floor(local_low))
            local_high = int(math.ceil(local_high))
            if local_high < local_low:
                local_low = int(center)
                local_high = int(center)
            if local_high == local_low:
                pad = max(1, refined_step)
                local_low = max(int(low), local_low - pad)
                local_high = min(int(high), local_high + pad)
            refined[key]["low"] = int(local_low)
            refined[key]["high"] = int(local_high)
            refined[key]["step"] = int(refined_step)
        else:
            refined_step = abs(step_value) / max(float(step_divisor), 1.0)
            if refined_step <= 0:
                refined_step = abs(step_value)
            if local_high <= local_low:
                local_low = max(low, float(center) - abs(step_value))
                local_high = min(high, float(center) + abs(step_value))
            refined[key]["low"] = round(float(local_low), 10)
            refined[key]["high"] = round(float(local_high), 10)
            refined[key]["step"] = round(float(refined_step), 10)
    return refined


def run_two_stage_optuna(args, space, reference, greedy_baseline):
    try:
        import optuna
    except ImportError as exc:
        raise RuntimeError(
            "Mode 'two_stage' requested, but optuna is not installed. "
            "Install it first with 'py -3 -m pip install optuna'."
        ) from exc

    all_keys = list(space.keys())
    summaries = []
    total_trials = max(1, int(args.trials))
    coarse_trials = max(1, int(math.floor(total_trials * args.coarse_fraction)))
    if total_trials > 1:
        coarse_trials = min(coarse_trials, total_trials - 1)
    refine_trials = max(0, total_trials - coarse_trials)
    global_trial_idx = 0

    def run_stage(stage_idx, stage_label, stage_space, n_trials, sampler_seed, enqueue_params=None, startup_trials=None):
        nonlocal global_trial_idx
        if n_trials <= 0:
            return []
        stage_summaries = []
        space_snapshot = clone_jsonish(stage_space)
        format_stage_header(stage_idx, stage_label, all_keys, {}, n_trials)
        sampler = make_sampler(args, optuna, sampler_seed, startup_trials=startup_trials)
        study = optuna.create_study(**build_study_kwargs(args, sampler, stage_idx=stage_idx))
        if enqueue_params is not None:
            study.enqueue_trial(enqueue_params)

        def objective(trial):
            nonlocal global_trial_idx
            params = {key: suggest_from_spec(trial, key, stage_space[key]) for key in all_keys}
            summary = evaluate_trial(
                args,
                params,
                global_trial_idx,
                reference,
                greedy_baseline=greedy_baseline,
                stage_idx=stage_idx,
                stage_label=stage_label,
                active_params=all_keys,
                fixed_params={},
                search_space_snapshot=space_snapshot,
            )
            stage_summaries.append(summary)
            summaries.append(summary)
            global_trial_idx += 1
            print_trial_summary(stage_label, len(stage_summaries), n_trials, summary)
            return summary["objective"]

        study.optimize(objective, n_trials=n_trials)
        return stage_summaries

    coarse_space = clone_jsonish(space)
    coarse_summaries = run_stage(
        0,
        "coarse_optuna",
        coarse_space,
        coarse_trials,
        args.optuna_seed,
        enqueue_params=None,
        startup_trials=min(args.startup_trials, max(0, coarse_trials - 1)),
    )
    if not coarse_summaries:
        return summaries

    best_stage0 = min(coarse_summaries, key=lambda item: item["objective"])
    refined_space = build_refined_space(
        coarse_space,
        best_stage0["params"],
        args.refine_step_divisor,
        args.refine_radius_steps,
    )
    print(
        "\nRefining around the best coarse-stage trial:\n"
        + json.dumps(
            {
                "best_trial": best_stage0["trial"],
                "best_objective": best_stage0["objective"],
                "best_params": best_stage0["params"],
                "refined_space": refined_space,
            },
            ensure_ascii=True,
            indent=2,
        )
    )

    if refine_trials > 0:
        run_stage(
            1,
            "fine_optuna",
            refined_space,
            refine_trials,
            args.optuna_seed + 1,
            enqueue_params=best_stage0["params"],
            startup_trials=min(args.refine_startup_trials, max(0, refine_trials - 1)),
        )
    return summaries


def ensure_positive(value, fallback):
    if value > 0:
        return value
    return fallback if fallback > 0 else 1e-12


def numeric_tolerance(spec, args):
    low = space_low(spec)
    high = space_high(spec)
    span = abs(high - low)
    step = space_step(spec)
    base = 0.0
    if step is not None:
        base = max(base, abs(step) * args.boundary_eps_steps)
    if span > 0:
        base = max(base, span * args.boundary_eps_fraction)
    if base <= 0:
        base = args.boundary_eps_fraction if args.boundary_eps_fraction > 0 else 1e-12
    return base


def boundary_side_for_value(value, spec, args):
    tol = numeric_tolerance(spec, args)
    low = space_low(spec)
    high = space_high(spec)
    near_low = abs(float(value) - low) <= tol
    near_high = abs(float(value) - high) <= tol
    if near_low and near_high:
        return "both"
    if near_low:
        return "low"
    if near_high:
        return "high"
    return None


def top_k_count(total, fraction):
    if total <= 0:
        return 0
    return max(1, math.ceil(total * fraction))


def choose_top_trials(summaries, fraction):
    ordered = sorted(summaries, key=lambda item: item["objective"])
    return ordered[:top_k_count(len(ordered), fraction)]


def detect_boundary_params(stage_summaries, active_keys, current_space, args):
    top = choose_top_trials(stage_summaries, args.top_fraction)
    if not top:
        return {}, []

    threshold = len(top) * args.boundary_fraction
    counts = {}
    boundary_keys = []
    for key in active_keys:
        spec = current_space[key]
        if not is_numeric_space(spec):
            continue
        low_hits = 0
        high_hits = 0
        for summary in top:
            side = boundary_side_for_value(summary["params"][key], spec, args)
            if side in {"low", "both"}:
                low_hits += 1
            if side in {"high", "both"}:
                high_hits += 1
        hit_side = None
        if low_hits > threshold and high_hits > threshold:
            hit_side = "both"
        elif low_hits > threshold:
            hit_side = "low"
        elif high_hits > threshold:
            hit_side = "high"
        counts[key] = {
            "top_trials": len(top),
            "low_hits": low_hits,
            "high_hits": high_hits,
            "threshold": threshold,
            "side": hit_side,
        }
        if hit_side is not None:
            boundary_keys.append(key)
    return counts, boundary_keys


def expand_numeric_spec(spec, side, args):
    updated = clone_jsonish(spec)
    kind = updated.get("kind", "float")
    low = float(updated["low"])
    high = float(updated["high"])
    step = updated.get("step")
    step_value = None if step is None else float(step)
    span = max(abs(high - low), abs(step_value) if step_value else 0.0)

    if bool(updated.get("log", False)) and low > 0 and high > 0:
        factor = max(args.log_expand_factor, 1.000001)
        if side in {"low", "both"}:
            low = max(low / factor, low * 0.5)
        if side in {"high", "both"}:
            high = max(high * factor, high + (step_value or 0.0))
    else:
        expand_by = max(span * args.expand_ratio, (step_value or 1.0) * args.min_expand_steps)
        if side in {"low", "both"}:
            low -= expand_by
        if side in {"high", "both"}:
            high += expand_by

    if kind == "int":
        step_int = int(step) if step is not None else 1
        low_int = math.floor(low / step_int) * step_int
        high_int = math.ceil(high / step_int) * step_int
        if high_int <= low_int:
            high_int = low_int + step_int
        updated["low"] = int(low_int)
        updated["high"] = int(high_int)
    else:
        if bool(updated.get("log", False)):
            fallback = float(step_value) if step_value and step_value > 0 else 1e-12
            low = ensure_positive(low, fallback)
            high = ensure_positive(high, max(low * 1.01, fallback))
        if high <= low:
            high = low + (step_value if step_value not in (None, 0) else max(abs(low) * 0.1, 1.0))
        updated["low"] = round(float(low), 10)
        updated["high"] = round(float(high), 10)
    return updated


def format_stage_header(stage_idx, stage_label, active_keys, fixed_params, trials_in_stage):
    fixed_txt = ", ".join(f"{key}={value}" for key, value in fixed_params.items()) if fixed_params else "-"
    active_txt = ", ".join(active_keys) if active_keys else "-"
    print(
        f"\n=== stage {stage_idx}: {stage_label} ===\n"
        f"trials={trials_in_stage}\n"
        f"active params: {active_txt}\n"
        f"fixed params: {fixed_txt}"
    )


def run_staged_tpe(args, space, reference, greedy_baseline):
    try:
        import optuna
    except ImportError as exc:
        raise RuntimeError(
            "Mode 'staged_tpe' requested, but optuna is not installed. "
            "Install it first with 'py -3 -m pip install optuna'."
        ) from exc

    original_space = clone_jsonish(space)
    current_space = clone_jsonish(space)
    all_keys = list(space.keys())
    active_keys = list(all_keys)
    fixed_params = {}
    summaries = []
    global_trial_idx = 0

    for stage_idx in range(args.max_stages):
        if not active_keys:
            break

        stage_label = "initial_tpe" if stage_idx == 0 else "boundary_refine_tpe"
        stage_trials = args.initial_trials if stage_idx == 0 else args.refine_trials
        stage_trials = max(1, stage_trials)
        stage_summaries = []
        space_snapshot = clone_jsonish(current_space)
        format_stage_header(stage_idx, stage_label, active_keys, fixed_params, stage_trials)

        sampler_seed = args.optuna_seed + stage_idx
        startup_trials = min(args.startup_trials, max(0, stage_trials - 1))
        sampler = make_sampler(args, optuna, sampler_seed, startup_trials=startup_trials)
        study = optuna.create_study(**build_study_kwargs(args, sampler, stage_idx=stage_idx))

        def objective(trial):
            nonlocal global_trial_idx
            params = {}
            for key in all_keys:
                if key in fixed_params:
                    params[key] = fixed_params[key]
                else:
                    params[key] = suggest_from_spec(trial, key, current_space[key])
            summary = evaluate_trial(
                args,
                params,
                global_trial_idx,
                reference,
                greedy_baseline=greedy_baseline,
                stage_idx=stage_idx,
                stage_label=stage_label,
                active_params=active_keys,
                fixed_params=fixed_params,
                search_space_snapshot=space_snapshot,
            )
            stage_summaries.append(summary)
            summaries.append(summary)
            global_trial_idx += 1
            print_trial_summary(stage_label, len(stage_summaries), stage_trials, summary)
            return summary["objective"]

        study.optimize(objective, n_trials=stage_trials)

        if stage_idx >= args.max_stages - 1:
            print("Reached max stage limit; stopping staged TPE.")
            break

        boundary_stats, boundary_keys = detect_boundary_params(stage_summaries, active_keys, current_space, args)
        best_overall = min(summaries, key=lambda item: item["objective"])
        print(
            "Boundary analysis:\n"
            + json.dumps(boundary_stats, ensure_ascii=True, indent=2)
        )

        if not boundary_keys:
            print("No parameters were consistently close to a boundary in the top trials; stopping staged TPE.")
            break

        new_space = clone_jsonish(current_space)
        for key in boundary_keys:
            side = boundary_stats[key]["side"]
            new_space[key] = expand_numeric_spec(new_space[key], side, args)

        fixed_params = {key: best_overall["params"][key] for key in all_keys if key not in boundary_keys}
        active_keys = list(boundary_keys)
        current_space = new_space
        print(
            "Expanding boundary-touching parameters and fixing the rest to the current best:\n"
            + json.dumps(
                {
                    "best_trial": best_overall["trial"],
                    "best_objective": best_overall["objective"],
                    "active_params_next_stage": active_keys,
                    "fixed_params_next_stage": fixed_params,
                    "updated_ranges": {key: current_space[key] for key in active_keys},
                },
                ensure_ascii=True,
                indent=2,
            )
        )

    return summaries


def write_summary_csv(path, summaries):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh, delimiter=";")
        writer.writerow(
            [
                "trial",
                "stage",
                "stage_label",
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
                "active_params_json",
                "fixed_params_json",
                "search_space_json",
                "greedy_baseline_json",
                "params_json",
                "repeat_summaries_json",
                "trial_root",
            ]
        )
        for item in sorted(summaries, key=lambda row: row["objective"]):
            writer.writerow(
                [
                    item["trial"],
                    "" if item.get("stage") is None else item.get("stage"),
                    item.get("stage_label", ""),
                    item["objective"],
                    item["quality_objective"],
                    item["runtime_penalty"],
                    item["mean_makespan"],
                    item["min_makespan"],
                    item["max_makespan"],
                    item["std_makespan"],
                    item["mean_runtime_sec"],
                    item["min_runtime_sec"],
                    item["max_runtime_sec"],
                    item["std_runtime_sec"],
                    "" if item["mean_rel_gap"] is None else item["mean_rel_gap"],
                    "" if item["min_rel_gap"] is None else item["min_rel_gap"],
                    "" if item["max_rel_gap"] is None else item["max_rel_gap"],
                    "" if item["mean_rel_gap"] is None else item["std_rel_gap"],
                    item["min_repeat_objective"],
                    item["max_repeat_objective"],
                    json.dumps(item.get("active_params", []), ensure_ascii=True, sort_keys=True),
                    json.dumps(item.get("fixed_params", {}), ensure_ascii=True, sort_keys=True),
                    json.dumps(item.get("search_space"), ensure_ascii=True, sort_keys=True),
                    json.dumps(item.get("greedy_baseline"), ensure_ascii=True, sort_keys=True),
                    json.dumps(item["params"], ensure_ascii=True, sort_keys=True),
                    json.dumps(item["repeat_summaries"], ensure_ascii=True, sort_keys=True),
                    item["trial_root"],
                ]
            )


def write_summary_json(path, summaries):
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = sorted(summaries, key=lambda row: row["objective"])
    path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")


def parse_args():
    parser = argparse.ArgumentParser(description="Tune ACO parameters on a set of graphs.")
    parser.add_argument("--build-dir", required=True, help="Coursework_mod/build directory.")
    parser.add_argument("--input-dir", required=True, help="Directory with graph .txt files.")
    parser.add_argument("--space", required=True, help="JSON file describing the search space.")
    parser.add_argument("--work-root", required=True, help="Where trial outputs will be written.")
    parser.add_argument("--summary-csv", help="Optional CSV file with all trial summaries.")
    parser.add_argument("--summary-json", help="Optional JSON file with all trial summaries.")
    parser.add_argument("--processors", type=int, required=True, help="Processor count for all graphs.")
    parser.add_argument("--memory", type=int, default=10**9, help="Memory limit.")
    parser.add_argument("--dups", type=int, default=1, help="Application duplicates per graph.")
    parser.add_argument("--threads", type=int, default=1, help="Application thread count.")
    parser.add_argument("--sample", type=int, required=True, help="Application sample size.")
    parser.add_argument("--batch", type=int, required=True, help="Application batch size.")
    parser.add_argument("--mode", choices=("grid", "optuna", "staged_tpe", "two_stage"), default="two_stage")
    parser.add_argument("--trials", type=int, default=25, help="Optuna trial count for --mode optuna.")
    parser.add_argument("--coarse-fraction", type=float, default=0.75,
                        help="Fraction of total --trials used on the initial coarse search in --mode two_stage.")
    parser.add_argument("--refine-step-divisor", type=float, default=10.0,
                        help="How many times to reduce the parameter step on the second stage in --mode two_stage.")
    parser.add_argument("--refine-radius-steps", type=float, default=1.0,
                        help="Refine each numeric parameter within +/- this many original steps around the best coarse-stage value.")
    parser.add_argument("--refine-startup-trials", type=int, default=3,
                        help="Warm-up trial count for the second stage in --mode two_stage.")
    parser.add_argument("--sampler", choices=("tpe", "random"), default="tpe",
                        help="Optuna sampler to use in Optuna-based modes.")
    parser.add_argument("--optuna-seed", type=int, default=42,
                        help="Seed for Optuna sampler reproducibility.")
    parser.add_argument("--startup-trials", type=int, default=8,
                        help="Number of random warm-up trials before TPE kicks in.")
    parser.add_argument("--multivariate-tpe", action="store_true",
                        help="Enable multivariate TPE sampler.")
    parser.add_argument("--repeats", type=int, default=1,
                        help="How many independent ACO runs to average inside each trial.")
    parser.add_argument("--seed-base", type=int, default=42,
                        help="Base seed for deterministic per-trial/per-repeat ACO seeds.")
    parser.add_argument("--runtime-weight", type=float, default=0.001,
                        help="Small additive weight for mean runtime in the final objective.")
    parser.add_argument("--study-storage",
                        help="Optional Optuna storage URL, e.g. sqlite:///aco_tpe.db.")
    parser.add_argument("--study-name",
                        help="Optional Optuna study name. With storage, enables resume/load.")
    parser.add_argument("--runner", choices=("wsl", "direct"), default="wsl")
    parser.add_argument("--reference-dir", help="Optional directory with reference schedules to measure relative gap.")
    parser.add_argument("--label-prefix", default="aco_tune", help="Prefix for trial labels.")

    parser.add_argument("--initial-trials", type=int, default=20,
                        help="Mandatory first TPE series length for --mode staged_tpe.")
    parser.add_argument("--refine-trials", type=int, default=10,
                        help="Additional TPE series length after boundary expansion in --mode staged_tpe.")
    parser.add_argument("--max-stages", type=int, default=4,
                        help="Maximum number of TPE stages in --mode staged_tpe.")
    parser.add_argument("--top-fraction", type=float, default=0.20,
                        help="Best-trial fraction analyzed after each staged TPE series.")
    parser.add_argument("--boundary-fraction", type=float, default=0.50,
                        help="A parameter is treated as boundary-hitting if more than this fraction of top trials are epsilon-close to one boundary.")
    parser.add_argument("--boundary-eps-fraction", type=float, default=0.05,
                        help="Relative epsilon neighborhood near numeric boundaries, measured as a fraction of current range.")
    parser.add_argument("--boundary-eps-steps", type=float, default=1.0,
                        help="Boundary epsilon lower bound in units of the parameter step.")
    parser.add_argument("--expand-ratio", type=float, default=0.50,
                        help="How much to expand a numeric range when top trials hug a boundary.")
    parser.add_argument("--min-expand-steps", type=float, default=2.0,
                        help="Minimum numeric range expansion in units of step.")
    parser.add_argument("--log-expand-factor", type=float, default=2.0,
                        help="Multiplicative expansion factor for positive log-scaled ranges.")
    return parser.parse_args()


def validate_args(args):
    if args.repeats < 1:
        raise ValueError("--repeats must be at least 1.")
    if args.runtime_weight < 0:
        raise ValueError("--runtime-weight must be non-negative.")
    if args.mode == "two_stage":
        if args.sampler != "tpe":
            raise ValueError("--mode two_stage expects --sampler tpe.")
        if args.trials < 2:
            raise ValueError("--mode two_stage expects --trials to be at least 2.")
        if not (0 < args.coarse_fraction < 1):
            raise ValueError("--coarse-fraction must be in (0, 1).")
        if args.refine_step_divisor <= 1:
            raise ValueError("--refine-step-divisor must be greater than 1.")
        if args.refine_radius_steps <= 0:
            raise ValueError("--refine-radius-steps must be positive.")
        if args.refine_startup_trials < 0:
            raise ValueError("--refine-startup-trials must be non-negative.")
    if args.mode == "staged_tpe":
        if args.sampler != "tpe":
            raise ValueError("--mode staged_tpe expects --sampler tpe.")
        if not (0 < args.top_fraction <= 1):
            raise ValueError("--top-fraction must be in (0, 1].")
        if not (0 < args.boundary_fraction < 1):
            raise ValueError("--boundary-fraction must be in (0, 1).")
        if args.initial_trials < 1:
            raise ValueError("--initial-trials must be at least 1.")
        if args.refine_trials < 1:
            raise ValueError("--refine-trials must be at least 1.")
        if args.max_stages < 1:
            raise ValueError("--max-stages must be at least 1.")
        if args.boundary_eps_fraction <= 0:
            raise ValueError("--boundary-eps-fraction must be positive.")
        if args.boundary_eps_steps <= 0:
            raise ValueError("--boundary-eps-steps must be positive.")
        if args.expand_ratio <= 0:
            raise ValueError("--expand-ratio must be positive.")
        if args.min_expand_steps <= 0:
            raise ValueError("--min-expand-steps must be positive.")
        if args.log_expand_factor <= 1:
            raise ValueError("--log-expand-factor must be greater than 1.")


def main():
    args = parse_args()
    validate_args(args)

    space = json.loads(Path(args.space).read_text(encoding="utf-8-sig"))
    reference = None
    if args.reference_dir:
        reference = load_makespans(Path(args.reference_dir))

    Path(args.work_root).mkdir(parents=True, exist_ok=True)
    greedy_baseline = compute_greedy_baseline(args, reference)

    if args.mode == "grid":
        summaries = run_grid(args, space, reference, greedy_baseline)
    elif args.mode == "optuna":
        summaries = run_optuna(args, space, reference, greedy_baseline)
    elif args.mode == "two_stage":
        summaries = run_two_stage_optuna(args, space, reference, greedy_baseline)
    else:
        summaries = run_staged_tpe(args, space, reference, greedy_baseline)

    if not summaries:
        raise RuntimeError("No trial summaries were produced.")

    if args.summary_csv:
        write_summary_csv(Path(args.summary_csv), summaries)
    if args.summary_json:
        write_summary_json(Path(args.summary_json), summaries)

    best = min(summaries, key=lambda item: item["objective"])
    print(
        "Greedy baseline:\n"
        + json.dumps(
            {
                "objective": greedy_baseline["objective"],
                "quality_objective": greedy_baseline["quality_objective"],
                "mean_makespan": greedy_baseline["mean_makespan"],
                "mean_runtime_sec": greedy_baseline["mean_runtime_sec"],
                "results": greedy_baseline["results"],
                "baseline_root": greedy_baseline["baseline_root"],
            },
            ensure_ascii=True,
            indent=2,
        )
    )
    print("\nBest trial:")
    print(json.dumps(
        {
            "trial": best["trial"],
            "stage": best.get("stage"),
            "stage_label": best.get("stage_label"),
            "objective": best["objective"],
            "quality_objective": best["quality_objective"],
            "runtime_penalty": best["runtime_penalty"],
            "mean_makespan": best["mean_makespan"],
            "min_makespan": best["min_makespan"],
            "max_makespan": best["max_makespan"],
            "std_makespan": best["std_makespan"],
            "mean_runtime_sec": best["mean_runtime_sec"],
            "min_runtime_sec": best["min_runtime_sec"],
            "max_runtime_sec": best["max_runtime_sec"],
            "std_runtime_sec": best["std_runtime_sec"],
            "mean_rel_gap": best["mean_rel_gap"],
            "min_rel_gap": best["min_rel_gap"],
            "max_rel_gap": best["max_rel_gap"],
            "std_rel_gap": best["std_rel_gap"],
            "min_repeat_objective": best["min_repeat_objective"],
            "max_repeat_objective": best["max_repeat_objective"],
            "active_params": best.get("active_params"),
            "fixed_params": best.get("fixed_params"),
            "params": best["params"],
            "repeat_summaries": best["repeat_summaries"],
            "trial_root": best["trial_root"],
        },
        ensure_ascii=True,
        indent=2,
    ))


if __name__ == "__main__":
    sys.exit(main())
