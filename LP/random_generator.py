import random

random.seed(42)
num_of_v = 20 # от 10 до 21 для качества решения для маленьких графов, 55-210-496-990 для качества решения для больших графов
variants = [1, 0]
lst = []
roots = set([i for i in range(num_of_v)])

for v1 in range(num_of_v - 2, -1, -1):
    strk = f'{v1}\t{random.randint(1, 100)}'
    for v2 in range(v1 + 1, num_of_v):
        p = (num_of_v - v1) * v2 / num_of_v ** 2 if v2 in roots else random.random() / num_of_v
        probabilities = [p, 1 - p]
        result = random.choices(variants, weights=probabilities, k=1)[0]
        if result:
            strk += f'\t{v2}'
            roots.discard(v2)
    if len(strk.split()) == 2:
        vv = random.choices([i for i in range(v1 + 1, num_of_v)], weights=[i ** 2 for i in range(num_of_v - 1, v1, -1)], k=1)[0]
        strk += f'\t{vv}'
        roots.discard(vv)
    lst.append(strk)
lst.append(f'{num_of_v - 1}\t{random.randint(1, 100)}')

random.shuffle(lst)
lst = ['node\tsize\tchildren'] + lst

f = open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/oldest_random/random_{num_of_v}.txt', 'w')
f.write('\n'.join(lst))
f.close()

print(len(roots))
