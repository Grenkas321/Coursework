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
    f = open(file_path, "r")
    nodes = []
    sizes = {}
    children = {}
    nodes_with_par = set()
    lines = f.readlines()[1:]
    if sort == "up_right":
        lines.reverse()
    for line in lines:
        nums = findall(r"\d+", line)
        if not nums:
            continue
        nodes.append(nums[0])
        children[nums[0]] = []
        if len(nums) == 2:
            continue
        for i in range(1, len(nums), 2):
            sizes[(nums[0], nums[i + 1])] = nums[i]
            children[nums[0]].append(nums[i + 1])
            nodes_with_par.add(nums[i + 1])
        if sort == "down_left":
            children[nums[0]].reverse()
    f.close()
    if sort == "tiers":
        nodes, children = tier_sort(nodes, children)
    elif sort == "reverse_tiers":
        nodes, children = tier_sort(nodes, children, True)
    root_parents = list(set(nodes) - nodes_with_par)
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
    m_bounds = []
    m_binary = []
    m_subj = []
    all_children = deepcopy(children)
    visited = {}
    for node in nodes:
        visited[node] = False
    for node in root_parents:
        visited[node] = True
        find_all_children(node, all_children, visited)
    for i in nodes:
        for j in nodes:
            if args.transitive:
                if i == j or j in all_children[i]:
                    m_bounds.append("m_" + str(i) + "_" + str(j) + " = 1")
            else:
                if i == j or j in children[i]:
                    m_bounds.append("m_" + str(i) + "_" + str(j) + " = 1")
            if i != j:
                m_subj.append(
                    "m_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " + "
                    + "m_"
                    + str(j)
                    + "_"
                    + str(i)
                    + " = 1"
                )
            m_binary.append("m_" + str(i) + "_" + str(j))
    return m_bounds, m_binary, m_subj


def define_s(
    nodes: List[str]
) -> (List[str], List[str], List[str], List[str]):
    """
     Генерирует ограничения для переменных s, которые представляют порядковые номера узлов в расписании.

    :param nodes: Список узлов.
    :return: Кортеж из четырех элементов:
    - s_int: Целочисленные переменные s.
    - s_subj_1, s_subj_4, s_subj_5: Уравнения для переменных s.
    """
    s_int = []
    s_subj_1 = []
    s_subj_4 = []
    s_subj_5 = []
    for i in nodes:
        s1 = "s_" + str(i)
        s_int.append(s1)
        for j in nodes:
            s1 += " - m_" + str(j) + "_" + str(i)
            if int(i) < int(j):
                s_subj_4.append(
                    "s_"
                    + str(i)
                    + " + "
                    + str(len(nodes))
                    + " m_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " - s_"
                    + str(j)
                    + " <= "
                    + str(len(nodes) - 1)
                )
                s_subj_5.append(
                    "s_"
                    + str(j)
                    + " - s_"
                    + str(i)
                    + " - "
                    + str(len(nodes))
                    + " m_"
                    + str(i)
                    + "_"
                    + str(j)
                    + " <= -1"
                )
        s1 += " = 0"
        s_subj_1.append(s1)
    return s_int, s_subj_1, s_subj_4, s_subj_5


def define_f(
    sizes: Dict[str, str],
    nodes: List[str]
) -> List[str]:
    """
    Генерирует ограничения для целевой функции F.

    :param sizes: Словарь размеров узлов.
    :param nodes: Список узлов.
    :return: Список строк, представляющих ограничения для целевой функции F.
    """
    f_subj = []
    var = " w_"
    if args.reduce == "new":
        var = " y_"
    for k in nodes:
        s = "F"
        for key, value in sizes.items():
            s += " - " + value + var + key[0] + "_" + key[1] + '_' + k
        s += " >= 0"
        f_subj.append(s)
    return f_subj


def define_l_w(
    children: Dict[str, List[str]],
    nodes: List[str]
) -> (List[str], List[str], List[str], List[str], List[str]):
    """
    Генерирует ограничения для переменных l и w, используемых в сведении "old".

    :param children: Словарь потомков.
    :param nodes: Список узлов.
    :return: Кортеж из пяти элементов:
    - l_bounds: Ограничения для переменных l.
    - l_subj: Уравнения для переменных l.
    - w_subj: Уравнения для переменных w.
    - l_binary: Бинарные переменные l.
    - w_binary: Бинарные переменные w.
    """
    l_bounds = []
    l_subj = []
    w_subj = []
    l_binary = []
    w_binary = []
    for i in nodes:
        l_bounds.append("l_" + str(i) + "_" + str(i) + " = 1")
        for k in nodes:
            for j in children[i]:
                l_subj.append(
                    "l_"
                    + str(i)
                    + "_"
                    + str(k)
                    + " - m_"
                    + str(k)
                    + "_"
                    + str(j)
                    + " >= 0"
                )
            w_subj.append(
                "w_"
                + str(i)
                + "_"
                + str(k)
                + " - l_"
                + str(i)
                + "_"
                + str(k)
                + " - m_"
                + str(i)
                + "_"
                + str(k)
                + " >= -1"
            )
            l_binary.append("l_" + str(i) + "_" + str(k))
            w_binary.append("w_" + str(i) + "_" + str(k))
    return l_bounds, l_subj, w_subj, l_binary, w_binary


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
    y_bounds = []
    y_subj = []
    y_binary = []
    subj10, subj11, subj12 = 0, 0, 0
    for key, value in sizes.items():
        for k in nodes:
            i, j = key
            y_binary.append("y_" + i + '_' + j + "_" + str(k))
            if i == k or j == k:
                y_bounds.append("y_" + i + '_' + j + "_" + str(k) + " = 1")
                continue
            else:
                y_subj.append(
                    "y_"
                    + i
                    + "_"
                    + j
                    + "_"
                    + str(k)
                    + " - "
                    + "m_"
                    + str(i)
                    + "_"
                    + str(k)
                    + " <= 0"
                )
                subj10 += 1
                y_subj.append(
                    "y_"
                    + i
                    + "_"
                    + j
                    + "_"
                    + str(k)
                    + " - "
                    + "m_"
                    + str(k)
                    + "_"
                    + str(j)
                    + " <= 0"
                )
                subj11 += 1
                y_subj.append(
                    "y_"
                    + i
                    + "_"
                    + j
                    + "_"
                    + str(k)
                    + " - m_"
                    + str(i)
                    + "_"
                    + str(k)
                    + " - m_"
                    + str(k)
                    + "_"
                    + str(j)
                    + " >= -1"
                )
                subj12 += 1
    print(subj10, subj11, subj12)
    return y_bounds, y_subj, y_binary


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
    l_bounds = []
    l_subj = []
    w_subj = []
    l_binary = []
    w_binary = []
    y_binary = []
    y_subj = []
    y_bounds = []

    # m  (2)(3)(6)
    m_bounds, m_binary, m_subj = define_m(children, nodes, parents)
    # s  (1)(4)(5)
    s_int, s_subj_1, s_subj_4, s_subj_5 = define_s(nodes)
    # F  (7б) F >= P_k
    f_subj = define_f(sizes, nodes)
    if args.reduce == "old":
        # l w (8)(9)(10)
        l_bounds, l_subj, w_subj, l_binary, w_binary = define_l_w(children, nodes)
    else:
        # y
        y_bounds, y_subj, y_binary = define_y(children, nodes, sizes)
    print(file_path)

    f = open(file_path, "w+")

    f.write("Minimize\n")
    f.write("    F\n")

    f.write("Subject to\n")
    for i in m_subj:
        f.write("    " + i + "\n")
    for i in s_subj_1:
        f.write("    " + i + "\n")
    for i in s_subj_4:
        f.write("    " + i + "\n")
    for i in s_subj_5:
        f.write("    " + i + "\n")
    for i in l_subj:
        f.write("    " + i + "\n")
    for i in w_subj:
        f.write("    " + i + "\n")
    for i in f_subj:
        f.write("    " + i + "\n")
    for i in y_subj:
        f.write("    " + i + "\n")

    f.write("Bounds\n")
    for i in m_bounds:
        f.write("    " + i + "\n")
    for i in l_bounds:
        f.write("    " + i + "\n")
    for i in y_bounds:
        f.write("    " + i + "\n")

    f.write("Integer\n")
    f.write("    F\n")
    for i in s_int:
        f.write("    " + i + "\n")

    f.write("Binary\n")
    for i in m_binary:
        f.write("    " + i + "\n")
    for i in l_binary:
        f.write("    " + i + "\n")
    for i in w_binary:
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
            args.output = curpath + "/" + args.output
        if args.output[:-1] != '/':
            args.output += '/'
    Path.mkdir(Path(args.output), parents=True, exist_ok=True)
    if curpath not in args.input:
        args.input = curpath + "/" + args.input
    if not os.path.isfile(args.input) and not os.path.isdir(args.input):
        print("Received input path isn't a directory or file. Try to enter absolute path")
        exit(1)
    parse("default")
    parse("tiers")
    parse("reverse_tiers")
    parse("up_right")
    parse("down_left")
