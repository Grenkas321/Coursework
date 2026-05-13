from random import random, randint, seed

def next_vertex():
    cur_v = 1
    while True:
        yield cur_v
        cur_v += 1

def rec_build(G):
    global itr
    
    if G == 'e':
        v1 = next(itr)
        v2 = next(itr)
        return [[v1, v2]], v1, v2
    depth = 0
    for i in range(2, len(G)):
        if G[i] == '(':
            depth += 1
        elif G[i] == ')':
            depth -= 1
        elif G[i] == ',' and depth == 0:
            G1 = G[2:i]
            G2 = G[i + 2:-1]
            break
    str_G1, source1, target1 = rec_build(G1)
    str_G2, source2, target2 = rec_build(G2)
    if G[0] == 'S':
        for i in range(len(str_G2)):
            if str_G2[i][0] == source2:
                str_G2[i][0] = target1
        return str_G1 + str_G2, source1, target2
    else:
        for i in range(len(str_G2)):
            if str_G2[i][0] == source2:
                str_G2[i][0] = source1
            if str_G2[i][1] == target2:
                str_G2[i][1] = target1
        return str_G1 + str_G2, source1, target1

def generate_sp_graph(m, p):
    global G, edges
    # m - целевое число рёбер, p - вероятность параллельной операции (0 <= p <= 1)
    # G = граф из одного ребра (2 вершины, 1 ребро)
    while len(edges) < m:
        # выбрать случайное ребро e из G
        edge = edges[randint(0, len(edges) - 1)]
        if random() < p:
            # Параллельная композиция
            # заменить e двумя рёбрами, параллельными e (между теми же вершинами)
            G = G.replace(edge, f'P({edge}1, {edge}2)')
        else:
            # Последовательная композиция
            # разделить e новой вершиной, добавив два новых ребра (образующих путь)
            G = G.replace(edge, f'S({edge}1, {edge}2)')
        edges.remove(edge)
        edges += [f'{edge}1', f'{edge}2']

seed(42)

for m in range(10, 40):
    G = 'e1'
    edges = ['e1']
    # m = 10
    generate_sp_graph(m, 0.4)
    for edge in edges:
        G = G.replace(edge, 'e')
    print(G)
    itr = next_vertex()
    lst_G, s, t = rec_build(G)
    for i in range(len(lst_G)):
        lst_G[i] = str(lst_G[i][0]) + ' -> ' + str(lst_G[i][1])
    str_G = '\n'.join(lst_G)
    print(str_G)

    dct = {}
    lst = str_G.split('\n')
    V = set()
    for e in lst:
        edge = e.split(' -> ')
        if edge[0] not in dct:
            dct[edge[0]] = [edge[1]]
        else:
            dct[edge[0]] += [edge[1]]
        V.add(edge[0])
        V.add(edge[1])
    print(dct)
    print()
    text = 'node    size    children\n'
    for v in list(V):
        if v in dct:
            text += f'{v}\t{randint(100, 1000)}\t{"\t".join(dct[v])}\n'
        else:
            text += f'{v}\t{randint(100, 1000)}\n'

    print(text)

    lines = text.split('\n')
    # lines[0] = lines[0][:-1]
    for i in range(1, len(lines)):
        lines[i] = '\t'.join(lines[i].split()[:2] + list(map(str, sorted(map(int, list(set(lines[i].split()[2:])))))))

    text = '\n'.join(lines)
    print(text)

    with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/oldest_sp/sp_{len(lines) - 2}.txt', "w", encoding="utf-8") as f:
        f.write(text)
