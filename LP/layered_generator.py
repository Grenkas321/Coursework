import random
# от 10 до 21 для масштабируемости и качества решения для маленьких графов
for num_of_v in range(10, 22):
    random.seed(42)
    lst = []
    # num_of_v = 10

    num_of_l = int(num_of_v ** (2 / 3))
    middle_num_of_v_in_l = num_of_v // num_of_l
    layers = []
    V = [i for i in range(num_of_v)]
    ii = 0
    for i in range(num_of_l):
        if i == num_of_l - 1:
            num_of_v_in_l_i = num_of_v - ii
        else:
            num_of_v_in_l_i = random.randint(middle_num_of_v_in_l * 2 // 3, middle_num_of_v_in_l * 3 // 2)
            if (num_of_v - ii - num_of_v_in_l_i) / (num_of_l - i - 1) < middle_num_of_v_in_l * 2 // 3:
                num_of_v_in_l_i = middle_num_of_v_in_l * 2 // 3
        layers.append(V[ii:ii + num_of_v_in_l_i])
        ii += num_of_v_in_l_i

    print(layers)

    '''
    roots = set(V)
    input_bufs = [0] * num_of_v
    available_v = set()
    for layer in list(reversed(layers)):
        enough = set()
        for v in list(reversed(layer)):
            if available_v == []:
                lst.append(f"{v}\t{random.randint(1, 100)}")
            else:
                num_of_b = min(random.randint(1, 3), len(available_v))
                children = random.sample(list(available_v), num_of_b)
                roots -= set(children)
                for child in children:
                    input_bufs[child] += 1
                    if input_bufs[child] == 3:
                        enough.add(child)
                lst.append(f"{v}\t{random.randint(1, 100)}\t{'\t'.join([str(i) for i in sorted(children)])}")
        available_v |= set(layer)
        available_v -= enough
    '''
    graph = {}
    available_v = set(V)
    for layer in list(reversed(layers))[:-1]:
        available_v -= set(layer)
        for v in list(reversed(layer)):
            num_of_b = min(1, len(available_v))
            parents = sorted(set(random.choices(sorted(list(available_v)), weights=[i ** 2 for i in sorted(list(available_v))], k=num_of_b)))
            for par in parents:
                if par not in graph:
                    graph[par] = [v]
                else:
                    graph[par] += [v]

    available_v = set(V)
    for layer in layers[:-1]:
        available_v -= set(layer)
        for v in layer:
            num_of_b = min(1, len(available_v))
            children = sorted(set(random.choices(sorted(list(available_v)), weights=[i ** 2 for i in sorted(list(available_v))], k=num_of_b)))
            for child in children:
                if v not in graph:
                    graph[v] = [child]
                else:
                    graph[v] += [child]

    lst.append('node\tsize\tchildren')

    graph = dict(sorted(graph.items()))
    print(graph)
    for v, children in graph.items():
        lst.append(f"{v}\t{random.randint(1, 100)}\t{'\t'.join([str(i) for i in sorted(set(children))])}")

    for v in layers[-1]:
        lst.append(f"{v}\t{random.randint(1, 100)}")

    # print(len(roots))

    # print('\n'.join(lst))

    f = open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/oldest_layered/layered_{num_of_v}.txt', 'w')
    f.write('\n'.join(lst))
    f.close()

