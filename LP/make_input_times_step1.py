from re import findall
import os
import argparse
from copy import deepcopy
from pathlib import Path
from typing import List, Dict


def tier_sort(
    nodes: List[str],
    children: Dict[str, List[str]],
    reverse_nodes: bool = False
) -> (List[str], Dict[str, List[str]]):
    """
    Сортирует узлы графа по уровням (tiers) и возвращает отсортированные узлы и их потомков.

    :param nodes: Список узлов графа.
    :param children: Словарь, где ключ — узел, значение — список его потомков.
    :param reverse_nodes: Если True, сортировка выполняется в обратном порядке.
    :return: Кортеж из двух элементов:
    - sort_nodes: Отсортированный список узлов.
    - sort_children: Словарь, где ключ — узел, значение — отсортированный список его потомков.
    """
    tiers = {}
    node_tier = {}
    sort_nodes = []
    lvl = 0
    count = 0
    parents = {}
    connections = children
    for node in nodes:
        parents[node] = []
    if reverse_nodes:
        for node in nodes:
            for child in children[node]:
                parents[child].append(node)
        connections = parents

    while count != len(nodes):
        tiers[lvl] = []
        if lvl == 0:
            for i in range(len(nodes)):
                flag = True
                for group in connections.values():
                    if nodes[i] in group:
                        flag = False
                        break
                if flag and nodes[i] not in sort_nodes:
                    tiers[lvl].append(nodes[i])
                    node_tier[nodes[i]] = lvl
                    sort_nodes.append(nodes[i])
                    count += 1
        else:
            for node in tiers[lvl - 1]:
                for child in connections[node]:
                    if child not in sort_nodes:
                        tiers[lvl].append(child)
                        node_tier[child] = lvl
                        sort_nodes.append(child)
                        count += 1
        lvl += 1
    sort_children = {}
    for node in sort_nodes:
        prev = None
        tmp_node_tier = {}
        sort_children[node] = children[node]
        for child in children[node]:
            tmp_node_tier[child] = node_tier[child]
        if sort_children[node] is not None:
            tmp_node_tier = dict(
                sorted(tmp_node_tier.items(), key=lambda item: item[1])
            )
            sort_children[node] = list(tmp_node_tier.keys())
    return sort_nodes, sort_children


def read_parser_input(
    file_path: str,
    sort: str
) -> (List[str], Dict[str, str], Dict[str, List[str]], List[str]):
    """
    Читает входной файл и возвращает данные в виде списка узлов, словаря размеров, словаря потомков и списка родителей.

    :param file_path: Путь к входному файлу.
    :param sort: Способ сортировки узлов. Возможные значения: "default", "tiers", "reverse_tiers", "up_right", "down_left".
    :return: Кортеж из четырех элементов:
    - nodes: Список узлов.
    - sizes: Словарь размеров узлов.
    - children: Словарь потомков.
    - root_parents: Список узлов, не имеющих родителей.
    """
    global times
    f = open(file_path, "r")
    nodes = []
    sizes = {}
    children = {}
    nodes_with_par = set()
    lines = f.readlines()[1:]
    if sort == "up_right":
        lines.reverse()
    for line in lines:
        split_line = line.split()
        node = split_line[0]
        time = split_line[1]
        times[node] = time
        nodes.append(node)
        children[node] = []
        buffers = ' '.join(split_line[2:]).split(', ')
        if buffers == ['0:']:
            continue
        for buf in buffers:
            size, v = buf.split(': ')
            sizes[(node, v)] = size
            nodes_with_par.add(v)
            children[node].append(v)
        if sort == "down_left":
            children[node].reverse()
    f.close()
    if sort == "tiers":
        nodes, children = tier_sort(nodes, children)
    elif sort == "reverse_tiers":
        nodes, children = tier_sort(nodes, children, True)
    root_parents = list(set(nodes) - nodes_with_par)
    # print(nodes, sizes, children, root_parents, times, sep='\n\n')
    return nodes, sizes, children, root_parents


def find_all_children(
    i: str,
    children: Dict[str, List[str]],
    visited: Dict[str, bool]):
    """
    Рекурсивно находит всех потомков для заданного узла i и обновляет словарь children.

    :param i: Узел, для которого ищутся потомки.
    :param children: Словарь потомков.
    :param visited: Словарь для отслеживания посещенных узлов.
    """
    for j in children[i]:
        if not visited[j]:
            visited[j] = True
            find_all_children(j, children, visited)
        for transit_child in children[j]:
            if transit_child not in children[i]:
                children[i].append(transit_child)


def define_m(
    children: Dict[str, List[str]],
    nodes: List[str],
    root_parents: List[str]
) -> (List[str], List[str], List[str]):
    """
    Генерирует ограничения для переменных m, которые представляют отношения частичного порядка между узлами.

    :param children: Словарь потомков.
    :param nodes: Список узлов.
    :param root_parents: Список узлов, не имеющих родителей.
    :return: Кортеж из трех элементов:
    - m_bounds: Ограничения для переменных m.
    - m_binary: Бинарные переменные m.
    - m_subj: Уравнения для переменных m.
    """
    global times
    D = sum(list(map(int, list(times.values()))))
    m_bounds = []
    m_binary = []
    m_subj = []
    for i in nodes:
        for j in nodes:
            if i == j:
                m_bounds.append("m_" + str(i) + "_" + str(j) + " = 1")
            else:
                m_subj.append(
                    str(D) +
                    " m_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " - s_"
                    + str(j)
                    + " + s_"
                    + str(i)
                    + " >= 0"
                )
                m_subj.append(f's_{j} - s_{i} - {D} m_{i}_{j} >= {1 - D}')
            m_binary.append("m_" + str(i) + "_" + str(j))
    return m_bounds, m_binary, m_subj


def define_p(
    nodes: List[str],
) -> (List[str], List[str]):
    """
    Генерирует ограничения для переменных m, которые представляют отношения частичного порядка между узлами.

    :param children: Словарь потомков.
    :param nodes: Список узлов.
    :param root_parents: Список узлов, не имеющих родителей.
    :return: Кортеж из трех элементов:
    - m_bounds: Ограничения для переменных m.
    - m_binary: Бинарные переменные m.
    - m_subj: Уравнения для переменных m.
    """
    global P
    p_binary = []
    p_subj = []
    for j in nodes:
        m = []
        for i in range(1, P + 1):
            p_binary.append("p_" + str(i) + "_" + str(j))
            m.append("p_" + str(i) + "_" + str(j))
        p_subj.append(' + '.join(m) + ' = 1')
    return p_binary, p_subj


def define_t(
    nodes: List[str],
) -> (List[str], List[str]):
    """
    Генерирует ограничения для переменных m, которые представляют отношения частичного порядка между узлами.

    :param children: Словарь потомков.
    :param nodes: Список узлов.
    :param root_parents: Список узлов, не имеющих родителей.
    :return: Кортеж из трех элементов:
    - m_bounds: Ограничения для переменных m.
    - m_binary: Бинарные переменные m.
    - m_subj: Уравнения для переменных m.
    """
    global P, times
    D = sum(list(map(int, list(times.values()))))
    t_binary = []
    t_subj = []
    for j in nodes:
        for k in nodes:
            if j != k:
                t_binary.append("z_" + str(j) + "_" + str(k))
                t_subj.append(f'{D} z_{j}_{k} - s_{j} + s_{k} >= {1 - int(times[k])}')
                t_subj.append(f's_{j} - s_{k} - {D} z_{j}_{k} >= {int(times[k]) - D}')
            for i in range(1, P + 1):
                t_subj.append(
                    "t_"
                    + str(j)
                    + "_"
                    + str(k)
                    + " - p_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " - p_"
                    + str(i)
                    + "_"
                    + str(k)
                    + " >= -1"
                )
                if j != k:
                    t_subj.append(
                        "t_"
                        + str(j)
                        + "_"
                        + str(k)
                        + " - p_"
                        + str(i)
                        + "_"
                        + str(j)
                        + " + p_"
                        + str(i)
                        + "_"
                        + str(k)
                        + " <= 1"
                    )
            t_binary.append("t_" + str(j) + "_" + str(k))
    return t_binary, t_subj


def define_s(
    nodes: List[str],
    sizes
) -> (List[str], List[str], List[str], List[str]):
    """
     Генерирует ограничения для переменных s, которые представляют порядковые номера узлов в расписании.

    :param nodes: Список узлов.
    :return: Кортеж из четырех элементов:
    - s_int: Целочисленные переменные s.
    - s_subj_1, s_subj_4, s_subj_5: Уравнения для переменных s.
    """
    global times
    D = sum(list(map(int, list(times.values()))))
    s_int = []
    s_subj_7 = []
    s_subj_8 = []
    s_subj_9 = []
    for i in nodes:
        s_int.append('s_' + str(i))
        for j in nodes:
            if (str(i), str(j)) in sizes:
                s_subj_7.append(
                    "s_"
                    + str(j)
                    + " - s_"
                    + str(i)
                    + " >= "
                    + str(times[i])
                )
            if i != j:
                s_subj_8.append(
                    "s_"
                    + str(i)
                    + " - s_"
                    + str(j)
                    + " + "
                    + str(D)
                    + " m_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " - "
                    + str(D)
                    + " t_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " >= "
                    + str(-1 * D + int(times[j]))
                )
                s_subj_9.append(
                    "s_"
                    + str(j)
                    + " - s_"
                    + str(i)
                    + " - "
                    + str(D)
                    + " m_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " - "
                    + str(D)
                    + " t_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " >= "
                    + str(-2 * D + int(times[i]))
                )
    return s_int, s_subj_7, s_subj_8, s_subj_9


def define_l(
    nodes: List[str]
) -> List[str]:
    """
    Генерирует ограничения для целевой функции l.

    :param nodes: Список узлов.
    :return: Список строк, представляющих ограничения для целевой функции l.
    """
    global times
    l_subj = []
    for k in nodes:
        l_subj.append('l - s_'
            + str(k)
            + ' >= '
            + str(times[k]))
    return l_subj


def define_y(
    children: Dict[str, List[str]],
    nodes: List[str],
    sizes: Dict[str, str]
) -> (List[str], List[str], List[str]):
    """
    Генерирует ограничения для переменных y, используемых в сведении "new".

    :param children: Словарь потомков.
    :param nodes: Список узлов.
    :return: Кортеж из трех элементов:
    - y_bounds: Ограничения для переменных y.
    - y_subj: Уравнения для переменных y.
    - y_binary: Бинарные переменные y.
    """
    global M, times
    D = sum(list(map(int, list(times.values()))))
    y_bounds = []
    y_subj = []
    y_binary = []
    r_subj = []
    for key, value in sizes.items():
        y_bounds.append("y_" + str(key[0]) + '_' + str(key[1]) + "_" + str(key[0]) + " = 1")
        y_bounds.append("y_" + str(key[0]) + '_' + str(key[1]) + "_" + str(key[1]) + " = 1")
    for k in nodes:
        r_k = []
        for key, value in sizes.items():
            i, j = key[0], key[1]
            r_k.append(sizes[(i, j)] + ' y_' + str(i) + '_' + str(j) + '_' + str(k))
            y_binary.append("y_" + str(i) + '_' + str(j) + "_" + str(k))
            if i != k and j != k:
                y_subj.append(f's_{k} - s_{i} - {D} y_{i}_{j}_{k} >= {1 - D - int(times[k])}')
                y_subj.append(f'{D} y_{i}_{j}_{k} + {D} z_{i}_{k} + s_{k} - s_{j} >= {int(times[j])}')
                y_subj.append(f's_{j} - s_{k} - {D} y_{i}_{j}_{k} >= {1 - D - int(times[j])}')
        r_subj.append(' + '.join(r_k) + ' <= ' + str(M))
    return y_bounds, y_subj, y_binary, r_subj


def write_solver_input(
    file_path: str,
    children: Dict[str, List[str]],
    sizes: Dict[str, str],
    nodes: List[str],
    parents: List[str]):
    """
    Записывает сгенерированные данные в файл в формате `.lp`.

    :param file_path: Путь к выходному файлу.
    :param children: Словарь потомков.
    :param sizes: Словарь размеров узлов.
    :param nodes: Список узлов.
    :param parents: Список узлов, не имеющих родителей.
    """

    # m  (1)(2)(3)
    m_bounds, m_binary, m_subj = define_m(children, nodes, parents)
    # s  (7)(8)(9)
    s_int, s_subj_7, s_subj_8, s_subj_9 = define_s(nodes, sizes)
    # l  (16)
    l_subj = define_l(nodes)
    # y
    y_bounds, y_subj, y_binary, r_subj = define_y(children, nodes, sizes)
    p_binary, p_subj = define_p(nodes)
    t_binary, t_subj = define_t(nodes)
    print(file_path)

    f = open(file_path, "w+")

    f.write("Minimize\n")
    f.write("    l\n")

    f.write("Subject to\n")
    for i in m_subj:
        f.write("    " + i + "\n")
    for i in s_subj_7:
        f.write("    " + i + "\n")
    for i in s_subj_8:
        f.write("    " + i + "\n")
    for i in s_subj_9:
        f.write("    " + i + "\n")
    for i in l_subj:
        f.write("    " + i + "\n")
    for i in p_subj:
        f.write("    " + i + "\n")
    for i in t_subj:
        f.write("    " + i + "\n")
    for i in y_subj:
        f.write("    " + i + "\n")
    for i in r_subj:
        f.write("    " + i + "\n")

    f.write("Bounds\n")
    for i in m_bounds:
        f.write("    " + i + "\n")
    for i in y_bounds:
        f.write("    " + i + "\n")

    f.write("Integer\n")
    f.write("    l\n")
    for i in s_int:
        f.write("    " + i + "\n")

    f.write("Binary\n")
    for i in m_binary:
        f.write("    " + i + "\n")
    for i in p_binary:
        f.write("    " + i + "\n")
    for i in t_binary:
        f.write("    " + i + "\n")
    for i in y_binary:
        f.write("    " + i + "\n")

    f.write("End\n")
    f.close()


def parse(sort: str):
    """
    Основная функция, которая вызывает read_parser_input для чтения входных данных и write_solver_input для записи выходных данных.

    :param sort: Способ сортировки узлов. Возможные значения: "default", "tiers", "reverse_tiers", "up_right", "down_left".
    """
    files = []
    input_path = args.input
    if os.path.isdir(args.input):
        files = os.listdir(args.input)
    else:
        files.append(args.input[args.input.rfind("/")+1:])
        input_path = args.input[:args.input.rfind("/")+1]
    for file_name in files:
        nodes, sizes, children, parents = read_parser_input(input_path + file_name, sort)
        if file_name.find(".") != -1:
            file_name = file_name[:file_name.rfind(".")]
        path = args.output + file_name + "_" + sort + "_input.lp"
        write_solver_input(path, children, sizes, nodes, parents)


if __name__ == "__main__":
    M = 55
    P = 2
    bfrs = {}
    times = {}
    """
    Основной блок, который выполняется при запуске скрипта. Обрабатывает аргументы командной строки и вызывает функцию parse.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-tr',
        '--transitive',
        action='store_true',
        help='set m_i_j=1 for transitive edges'
    )
    parser.add_argument(
        '-r',
        '--reduce',
        type=str,
        default='new',
        help='set reduce "new" or "old"'
    )
    parser.add_argument(
        '-o',
        '--output',
        type=str,
        default=None,
        help='output directory for .lp files'
    )
    parser.add_argument(
        '-i',
        '--input',
        required=True,
        type=str,
        help='path to input file or directory'
    )
    args = parser.parse_args()
    curpath = os.getcwd()
    if args.reduce != "old" and args.reduce != "new":
        print('Please, use only "new" or "old" for reduce flag')
        exit(1)
    if args.output is None:
        if args.transitive:
            args.output = curpath + "/" + "outputs/" + args.reduce + "_tr/order/"
        else:
            args.output = curpath + "/" + "outputs/" + args.reduce + "_no_tr/order/"
    else:
        if curpath not in args.output:
            # args.output = curpath + "/" + args.output
            args.output = args.output
        if args.output[:-1] != '/':
            args.output += '/'
    Path.mkdir(Path(args.output), parents=True, exist_ok=True)
    if curpath not in args.input:
        # args.input = curpath + "/" + args.input
        args.input = args.input
    if not os.path.isfile(args.input) and not os.path.isdir(args.input):
        print("Received input path isn't a directory or file. Try to enter absolute path")
        exit(1)
    parse("default")
    parse("tiers")
    parse("reverse_tiers")
    parse("up_right")
    parse("down_left")
