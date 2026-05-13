import os
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/scalability/new_triadags_time_P_and_M'
directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/scalability/new_layered_time_P_and_M_2'

files = os.listdir(directory)
if '.DS_Store' in files:
    files.remove('.DS_Store')
files.sort(key=lambda x: (int(x.split('_')[1]), x.split('_')[0], x.split('_')[2], x.split('_')[3], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))

algos = ['greedy', 'greedy2', 'lp']

for perem1 in [0, 1]:
    for perem2 in range(9):
        sizes = [[], [], []]
        data_runtime = [[], [], []]
        data_makespan = [[], [], []]
        params_idx = perem2
        params = ['малые P и M', 'малое P, средний M', 'малое P, большой M', 'среднее P, малый M', 'средние P и M', 'среднее P, большой M', 'большое P, низкий M', 'большое P, средний M', 'большие P и M']
        use_mode_N_uniform = perem1
        pickup_lst = []
        pickup_file = ''
        i = -1

        for file_name in files:
            
            if int(file_name.split('_')[1]) > 21:
                break
            
            if use_mode_N_uniform and 'N_uniform' not in file_name or not use_mode_N_uniform and 'N_uniform' in file_name:
                continue
            
            i = (i + 1) % 9
            if i != params_idx:
                continue
            
            # print(file_name)
            for alg in algos:
                try:
                    with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{alg}/schedules/best/{file_name[:-4]}.json') as f:
                        text = f.readline()
                        dct = eval(text)
                        name = list(dct.keys())[0]
                        dct = dct[name]
                        makespan = dct['makespan']
                        runtime_sec = float(dct['runtime_sec'])
                        size = dct['size']
                    # print('\n', file_name, runtime_sec, '\n')
                    data_runtime[algos.index(alg)] += [runtime_sec]
                    data_makespan[algos.index(alg)] += [makespan]
                    sizes[algos.index(alg)].append(size)
                except Exception:
                    pass

        '''
        print(data_runtime)
        print(data_makespan)
        print(data_names)
        print(sizes)
        '''
        alg_dct = {'greedy': 'ЖА с EFT', 'greedy2': 'ЖА с STS', 'lp': 'ЛП'}

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 8))

        fig.suptitle(f'{params[params_idx]}, тип буферов = {"N_uniform" if perem1 else 1}')

        for alg in algos:
            ax1.plot(sizes[algos.index(alg)], data_makespan[algos.index(alg)], marker='o', label=alg_dct[alg])

        ax1.set_xlabel('Кол-во вершин')
        ax1.set_ylabel('ЦФ')
        ax1.set_ylim(0)
        ax1.grid(True)

        for alg in algos:
            ax2.plot(sizes[algos.index(alg)], data_runtime[algos.index(alg)], marker='o', label=alg_dct[alg])

        ax2.set_xlabel('Кол-во вершин')
        ax2.set_ylabel('Время выполнения')
        ax2.set_yscale('log')
        ax2.grid(True)

        ax1.legend()

        plt.tight_layout()
        plt.show()
