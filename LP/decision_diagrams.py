import os
import matplotlib
import matplotlib.pyplot as plt
import numpy as np


dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_layered_time_P_and_M_2',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_random_time_P_and_M_2',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_sp_time_P_and_M_2']

'''
dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_layered_time_P_and_M',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_random_time_P_and_M',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_triadags_time_P_and_M']
'''
algos = ['greedy', 'greedy2', 'lp']

for directory in dirs:
    files = os.listdir(directory)
    if '.DS_Store' in files:
        files.remove('.DS_Store')
    files.sort(key=lambda x: (int(x.split('_')[1]), x.split('_')[0], x.split('_')[2], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))
    '''
    pickup_graph_num = dirs.index(directory)
    pickup_buf_type = pickup_graph_num % 2

    pickup_graph_num = 7
    pickup_buf_type = 0
    '''
    pickup_lst = []
    pickup_buf_type = 0
    data_runtime = [[], [], []]
    data_makespan = [[], [], []]
    x_names = [[], [], []]
    
    for file_name0 in files:
        
        if int(file_name0.split('_')[1]) > 21:
            break
        
        pickup_lst.append(file_name0)
        if len(pickup_lst) < 9:
            continue
    
        P_and_M_lst = []
        
        
        for file_name in pickup_lst:
            if pickup_buf_type:
                graph_name, P_and_M = file_name.split('_N_uniform_lp2_')
                # graph_name, P_and_M = file_name.split('_N_uniform_')
            else:
                graph_name, P_and_M = file_name.split('_1_lp2_')
                # graph_name, P_and_M = file_name.split('_1_')
                
                    
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
                    x_names[algos.index(alg)].append(P_and_M[:-4])
                    data_runtime[algos.index(alg)] += [runtime_sec]
                    data_makespan[algos.index(alg)] += [makespan]
                except Exception:
                    pass
                    
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 8))

        fig.suptitle(f'Значения целевой функции разных алгоритмов на графе {graph_name}:{"N_uniform" if pickup_buf_type else 1} на разных P и M')

        width = 0.2
        
        ids = [[], [], []]
        for k in range(3):
            ids[k] = [x_names[0].index(obj) + 1 for obj in x_names[k]]

        ax1.bar([i - width for i in ids[0]], data_makespan[0], width, label='greedy_EFT')
        ax1.bar(ids[1], data_makespan[1], width, label='greedy_STS')
        ax1.bar([i + width for i in ids[2]], data_makespan[2], width, label='lp')

        ax1.set_xticks(ids[0])
        ax1.set_xticklabels(x_names[0])

        ax1.set_xlabel('Кол-во вершин')
        ax1.set_ylabel('ЦФ')
        ax1.grid(True)
        
        ax1.set_xlim(0.5, 10.5)
        
        alg_dct = {'greedy': 'ЖА с EFT', 'greedy2': 'ЖА с STS', 'lp': 'ЛП'}

        for alg in algos:
            ax2.plot(ids[algos.index(alg)], data_runtime[algos.index(alg)], marker='o', label=alg_dct[alg])

        ax2.set_xlabel('Кол-во вершин')
        ax2.set_ylabel('Время выполнения')
        ax2.set_yscale('log')
        ax2.grid(True)
        
        ax2.set_xticks(ids[0])
        ax2.set_xticklabels(x_names[0])
        
        ax2.set_xlim(0.5, 10.5)

        ax1.legend()

        plt.tight_layout()
        plt.show()
        
        pickup_lst = []
        pickup_buf_type = 1 - pickup_buf_type
        data_runtime = [[], [], []]
        data_makespan = [[], [], []]
        x_names = [[], [], []]
