import os
import matplotlib
import matplotlib.pyplot as plt
import numpy as np


dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_layered_time_P_and_M',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_random_time_P_and_M',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_triadags_time_P_and_M']

algos = ['greedy', 'greedy2']

for directory in dirs:
    files = os.listdir(directory)
    if '.DS_Store' in files:
        files.remove('.DS_Store')
    files.sort(key=lambda x: (x.split('_')[2], int(x.split('_')[1]), x.split('_')[0], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))
    
    print(directory.split('/')[-1])
    
    files1, filesN = [], []
    for fn in files:
        if '_1_' in fn:
            files1.append(fn)
        else:
            filesN.append(fn)
    
    data_makespan = []
    one_size = []
    i = 0
    for file_name in files1:
        triple = []
        for alg in algos:
            try:
                with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{alg}/schedules/best/{file_name[:-4]}.json') as f:
                    text = f.readline()
                    dct = eval(text)
                    name = list(dct.keys())[0]
                    dct = dct[name]
                    makespan = int(dct['makespan'])
                    runtime_sec = float(dct['runtime_sec'])
                    size = dct['size']
                triple.append(makespan)
            except Exception:
                pass
        if len(triple) == 2:
            one_size.append(round((triple[0] - triple[1]) / triple[0] * 100, 2))
        else:
            one_size.append('?')
        i += 1
        if i == 9:
            data_makespan.append(one_size)
            one_size = []
            i = 0
    print('      1')
    for mas in data_makespan:
        print(*mas)
    print()
    
    data_makespan = []
    one_size = []
    i = 0
    for file_name in filesN:
        triple = []
        for alg in algos:
            try:
                with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{alg}/schedules/best/{file_name[:-4]}.json') as f:
                    text = f.readline()
                    dct = eval(text)
                    name = list(dct.keys())[0]
                    dct = dct[name]
                    makespan = int(dct['makespan'])
                    runtime_sec = float(dct['runtime_sec'])
                    size = dct['size']
                triple.append(makespan)
            except Exception:
                pass
        if len(triple) == 2:
            one_size.append(round((triple[0] - triple[1]) / triple[0] * 100, 2))
        else:
            one_size.append('?')
        i += 1
        if i == 9:
            data_makespan.append(one_size)
            one_size = []
            i = 0
    
    print('      N_uni')
    for mas in data_makespan:
        print(*mas)
    print()
