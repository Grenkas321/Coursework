#!/usr/bin/env python3
"""
Convert graphs from the old format to three timed new-format graphs.

Input old format:
    node size children
    0    44   10 11 12

Generated outputs for file graph.txt:
    graph_1.txt          - one output buffer for all outgoing edges of each vertex
    graph_N.txt          - ceil(sqrt(out_degree)) buffers, random child distribution
    graph_N_uniform.txt  - ceil(sqrt(out_degree)) buffers, uniform child distribution

All outputs use the timed new format:
    prog_id prog_time weight_1: child_1 ... child_n, ...

By default:
    prog_time is random in [10, 100]
    buffer weight is random in [1, 100]

Important:
    The same prog_time is used for the same vertex in all three generated graphs.
"""

from __future__ import annotations

import argparse
import math
import random
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_TIME_MIN = 10
DEFAULT_TIME_MAX = 100
DEFAULT_WEIGHT_MIN = 1
DEFAULT_WEIGHT_MAX = 100


@dataclass(frozen=True)
class Node:
    node_id: int
    old_size: int
    children: list[int]


@dataclass(frozen=True)
class Buffer:
    weight: int
    children: list[int]


def is_int_token(token: str) -> bool:
    try:
        int(token)
        return True
    except ValueError:
        return False


def parse_old_format(path: Path) -> list[Node]:
    """Read a graph in the old format: node size children..."""
    nodes: list[Node] = []

    with path.open("r", encoding="utf-8") as fin:
        for raw_line in fin:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            parts = line.split()
            if len(parts) < 2:
                continue

            # Skip header or malformed rows.
            if not is_int_token(parts[0]) or not is_int_token(parts[1]):
                continue

            node_id = int(parts[0])
            old_size = int(parts[1])

            children: list[int] = []
            for token in parts[2:]:
                if is_int_token(token):
                    children.append(int(token))

            nodes.append(Node(node_id=node_id, old_size=old_size, children=children))

    return sorted(nodes, key=lambda node: node.node_id)


def random_int_in_range(left: int, right: int) -> int:
    return random.randint(left, right)


def n_buffers_count(children: list[int]) -> int:
    """Return ceil(sqrt(out_degree)); for leaves the real count is 0."""
    if not children:
        return 0
    return math.ceil(math.sqrt(len(children)))


def make_one_buffer(children: list[int], weight_min: int, weight_max: int) -> list[Buffer]:
    """One buffer for all outgoing edges. Leaves get a dummy zero buffer."""
    if not children:
        return [Buffer(weight=0, children=[])]

    return [
        Buffer(
            weight=random_int_in_range(weight_min, weight_max),
            children=list(children),
        )
    ]


def distribute_children_to_n_buffers_random(children: list[int]) -> list[list[int]]:
    """
    Split children into ceil(sqrt(len(children))) non-empty random groups.

    Distribution is intentionally random: after every buffer gets one child,
    all remaining children are placed into random buffers.
    """
    buffer_count = n_buffers_count(children)
    if buffer_count == 0:
        return []

    shuffled = list(children)
    random.shuffle(shuffled)

    groups = [[child] for child in shuffled[:buffer_count]]

    for child in shuffled[buffer_count:]:
        random.choice(groups).append(child)

    random.shuffle(groups)
    return groups


def distribute_children_to_n_buffers_uniform(children: list[int]) -> list[list[int]]:
    """
    Split children into ceil(sqrt(len(children))) groups as evenly as possible.

    The children order is shuffled first, then children are distributed round-robin.
    Therefore group sizes differ by at most 1, while the exact composition remains random.
    """
    buffer_count = n_buffers_count(children)
    if buffer_count == 0:
        return []

    shuffled = list(children)
    random.shuffle(shuffled)

    groups = [[] for _ in range(buffer_count)]
    for index, child in enumerate(shuffled):
        groups[index % buffer_count].append(child)

    # All groups are non-empty because buffer_count <= len(children).
    random.shuffle(groups)
    return groups


def make_buffers_from_groups(
    groups: list[list[int]],
    weight_min: int,
    weight_max: int,
) -> list[Buffer]:
    if not groups:
        return [Buffer(weight=0, children=[])]

    return [
        Buffer(
            weight=random_int_in_range(weight_min, weight_max),
            children=group,
        )
        for group in groups
    ]


def make_n_buffers_random(children: list[int], weight_min: int, weight_max: int) -> list[Buffer]:
    """ceil(sqrt(out_degree)) buffers with random child distribution."""
    groups = distribute_children_to_n_buffers_random(children)
    return make_buffers_from_groups(groups, weight_min, weight_max)


def make_n_buffers_uniform(children: list[int], weight_min: int, weight_max: int) -> list[Buffer]:
    """ceil(sqrt(out_degree)) buffers with uniform child distribution."""
    groups = distribute_children_to_n_buffers_uniform(children)
    return make_buffers_from_groups(groups, weight_min, weight_max)


def format_buffers(buffers: list[Buffer]) -> str:
    parts: list[str] = []

    for buffer in buffers:
        if buffer.children:
            child_part = " ".join(str(child) for child in buffer.children)
            parts.append(f"{buffer.weight}: {child_part}")
        else:
            parts.append(f"{buffer.weight}:")

    return ", ".join(parts)


def make_timed_lines(
    nodes: list[Node],
    node_times: dict[int, int],
    mode: str,
    weight_min: int,
    weight_max: int,
) -> list[str]:
    """Build output lines in timed new format."""
    lines = ["prog_id prog_time weight_1: child_1 ... child_n, ..."]

    for node in nodes:
        if mode == "one":
            buffers = make_one_buffer(node.children, weight_min, weight_max)
        elif mode == "n_random":
            buffers = make_n_buffers_random(node.children, weight_min, weight_max)
        elif mode == "n_uniform":
            buffers = make_n_buffers_uniform(node.children, weight_min, weight_max)
        else:
            raise ValueError(f"Unknown conversion mode: {mode}")

        rhs = format_buffers(buffers)
        lines.append(f"{node.node_id}\t{node_times[node.node_id]}\t{rhs}")

    return lines


def write_lines(path: Path, lines: Iterable[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fout:
        fout.write("\n".join(lines) + "\n")


def output_file_path(input_file: Path, output_dir: Path, suffix: str) -> Path:
    extension = input_file.suffix if input_file.suffix else ".txt"
    return output_dir / f"{input_file.stem}{suffix}{extension}"


def convert_file(
    input_file: Path,
    output_dir: Path,
    time_min: int,
    time_max: int,
    weight_min: int,
    weight_max: int,
) -> tuple[Path, Path, Path]:
    nodes = parse_old_format(input_file)
    if not nodes:
        raise ValueError(f"Input file is empty or not recognized as old format: {input_file}")

    # The same execution times are used in all generated graphs so that
    # experiments differ only by buffer structure and buffer weights.
    node_times = {
        node.node_id: random_int_in_range(time_min, time_max)
        for node in nodes
    }

    one_lines = make_timed_lines(
        nodes=nodes,
        node_times=node_times,
        mode="one",
        weight_min=weight_min,
        weight_max=weight_max,
    )
    '''
    n_random_lines = make_timed_lines(
        nodes=nodes,
        node_times=node_times,
        mode="n_random",
        weight_min=weight_min,
        weight_max=weight_max,
    )
    '''
    n_uniform_lines = make_timed_lines(
        nodes=nodes,
        node_times=node_times,
        mode="n_uniform",
        weight_min=weight_min,
        weight_max=weight_max,
    )

    one_path = output_file_path(input_file, output_dir, "_1")
    # n_random_path = output_file_path(input_file, output_dir, "_N")
    n_uniform_path = output_file_path(input_file, output_dir, "_N_uniform")

    write_lines(one_path, one_lines)
    # write_lines(n_random_path, n_random_lines)
    write_lines(n_uniform_path, n_uniform_lines)

    # return one_path, n_random_path, n_uniform_path
    return one_path, n_uniform_path


def iter_input_files(path: Path) -> Iterable[Path]:
    if path.is_file():
        if path.suffix.lower() != ".txt":
            raise ValueError(f"Input file must have .txt extension: {path}")
        yield path
        return

    if path.is_dir():
        for item in sorted(path.iterdir()):
            if item.is_file() and item.suffix.lower() == ".txt":
                yield item
        return

    raise ValueError(f"Path is neither file nor directory: {path}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Convert old-format graph files to three timed new-format files: "
            "*_1.txt, *_N.txt and *_N_uniform.txt."
        )
    )
    parser.add_argument(
        "input_path",
        help="Old-format input .txt file or directory with .txt files.",
    )
    parser.add_argument(
        "output_dir",
        help="Directory where generated files will be written.",
    )
    parser.add_argument(
        "--time-min",
        type=int,
        default=DEFAULT_TIME_MIN,
        help=f"Minimum random execution time, default {DEFAULT_TIME_MIN}.",
    )
    parser.add_argument(
        "--time-max",
        type=int,
        default=DEFAULT_TIME_MAX,
        help=f"Maximum random execution time, default {DEFAULT_TIME_MAX}.",
    )
    parser.add_argument(
        "--weight-min",
        type=int,
        default=DEFAULT_WEIGHT_MIN,
        help=f"Minimum random buffer weight, default {DEFAULT_WEIGHT_MIN}.",
    )
    parser.add_argument(
        "--weight-max",
        type=int,
        default=DEFAULT_WEIGHT_MAX,
        help=f"Maximum random buffer weight, default {DEFAULT_WEIGHT_MAX}.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Random seed for reproducible generation.",
    )
    return parser.parse_args()


def validate_range(name: str, left: int, right: int) -> None:
    if left > right:
        raise ValueError(f"{name}_min must be <= {name}_max")


def main() -> int:
    args = parse_args()

    try:
        validate_range("time", args.time_min, args.time_max)
        validate_range("weight", args.weight_min, args.weight_max)

        input_path = Path(args.input_path)
        output_dir = Path(args.output_dir)

        if not input_path.exists():
            raise ValueError(f"Input path not found: {input_path}")

        random.seed(args.seed)

        converted = 0
        for input_file in iter_input_files(input_path):
            one_path, n_uniform_path = convert_file(
                input_file=input_file,
                output_dir=output_dir,
                time_min=args.time_min,
                time_max=args.time_max,
                weight_min=args.weight_min,
                weight_max=args.weight_max,
            )
            print(f"{input_file} -> {one_path}")
            print(f"{input_file} -> {n_uniform_path}")
            converted += 1

        if converted == 0:
            raise ValueError(f"No .txt files found in directory: {input_path}")

        return 0

    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
