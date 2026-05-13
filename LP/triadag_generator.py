import random

random.seed(42)
lst = ['node\tsize\tchildren']
num_of_layeres = 44 # 10-14-17-20-24-31-38-44 для масштабируемости, 10-20-31-44 для качества решения, все для больших графов
num_of_l = num_of_layeres
next_layer_v = 0
sm = (1 + num_of_layeres) * num_of_layeres // 2
for v in range(sm):
    if v == next_layer_v:
        next_layer_v += num_of_l
        num_of_l -= 1
        lst.append(f"{v}\t{random.randint(1, 100)}\t{'\t'.join([str(i) for i in range(next_layer_v, next_layer_v + num_of_l)])}")
    else:
        lst.append(f"{v}\t{random.randint(1, 100)}\t{v + num_of_l}")

f = open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/scalability/oldest_triadags/triadag{num_of_layeres}_{sm}_S.txt', 'w')
f.write('\n'.join(lst))
f.close()
