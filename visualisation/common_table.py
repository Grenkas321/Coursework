import os
import matplotlib
import matplotlib.pyplot as plt
import numpy as np


dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_layered_time_P_and_M_2',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_random_time_P_and_M_2']

algos = ['greedy', 'greedy2', 'lp']

for directory in dirs:
    files = os.listdir(directory)
    if '.DS_Store' in files:
        files.remove('.DS_Store')
    files.sort(key=lambda x: x.split('_')[2])
    
    files1, filesN = [], []
    for fn in files:
        if '_1_' in fn:
            files1.append(fn)
        else:
            filesN.append(fn)
    
    data_makespan = []
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
        if len(triple) == 3:
            data_makespan.append(triple)
    
    d_eft, d_sts = [], []
    for eft, sts, lp in data_makespan:
        d_eft.append((eft - lp) / eft)
        d_sts.append((sts - lp) / sts)
    print(round(sum(d_eft) / len(d_eft) * 100, 2), '&', round(max(d_eft) * 100, 2), '&', round(sum(d_sts) / len(d_sts) * 100, 2), '&', round(max(d_sts) * 100, 2))
    
    data_makespan = []
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
        if len(triple) == 3:
            data_makespan.append(triple)
    
    d_eft, d_sts = [], []
    for eft, sts, lp in data_makespan:
        d_eft.append((eft - lp) / eft)
        d_sts.append((sts - lp) / sts)
    print(round(sum(d_eft) / len(d_eft) * 100, 2), '&', round(max(d_eft) * 100, 2), '&', round(sum(d_sts) / len(d_sts) * 100, 2), '&', round(max(d_sts) * 100, 2))
