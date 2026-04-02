import os
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import shutil
 
# Указываем путь к директории
# directory = "/Users/maxbig/ASVK/coursework/LP/old_tests_sp_buf_times/mix"
directory = "/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs"

'''
l1 = ['P2', 'P3', 'P4']
l2 = ['M2600', 'M4000', 'M10000']
for i in l1:
    for j in l2:
        directory2 = f"/Users/maxbig/ASVK/coursework/LP/{i}_{j}"
        files2 = os.listdir(directory2)
        for file_name2 in files2:
            if not('14' in file_name2) and not('15' in file_name2) and i == 'P2' and j in ['M4000', 'M10000']:
                continue
            shutil.copy(directory2 + '/' + file_name2, directory + '/' + file_name2)
'''


directory3 = "/Users/maxbig/Coursework_multiprocessing/Coursework/LP/G"

files = sorted(os.listdir(directory3))
if '.DS_Store' in files:
    files.remove('.DS_Store')

l1 = ['P2', 'P3', 'P4']
# l2 = ['M2600', 'M4000', 'M10000']
# l2 = ['M8500', 'M11000', 'M13500']
l2 = ['M1050', 'M1350', 'M2550']
for file_name in files:
    for i in l1:
        for j in l2:
            directory2 = f"/Users/maxbig/Coursework_multiprocessing/Coursework/LP/{i}_{j}"
            shutil.copy(directory2 + '/' + file_name[:-4] + f'_{i}_{j}.txt', directory + '/' + file_name[:-4] + f'_{i}_{j}.txt')

# Получаем список файлов
files = sorted(os.listdir(directory), key=lambda x: (int(x.split('_P')[1][0]), int(x.split('_M')[1].split('.')[0])))
if '.DS_Store' in files:
    files.remove('.DS_Store')

# Выводим список файлов
# print(files)

data_names = []
data_sp = []
data_lp = []
data_greedy = []

for file_name in files:
    graph_name = file_name[:-4]
    # f_sp = open("/Users/maxbig/Coursework/build/Answer/sp0/schedules/best/" + file_name[:-4] + '.json', 'r')
    # f_greedy = open("/Users/maxbig/Coursework/build/Answer/greedy/schedules/best/" + file_name[:-4] + '.json', 'r')
    f_lp_new_tr_default = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/" + file_name[:-4] + '_default_input.log', 'r')
    f_lp_new_tr_up_right = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/" + file_name[:-4] + '_up_right_input.log', 'r')
    f_lp_new_tr_down_left = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/" + file_name[:-4] + '_down_left_input.log', 'r')
    f_lp_new_tr_tiers = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/" + file_name[:-4] + '_tiers_input.log', 'r')
    # f_lp_new_tr_reverse_tiers = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/" + file_name[:-4] + '_reverse_tiers_input.log', 'r')
    f_lp_new_no_tr_default = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/" + file_name[:-4] + '_default_input.log', 'r')
    f_lp_new_no_tr_up_right = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/" + file_name[:-4] + '_up_right_input.log', 'r')
    f_lp_new_no_tr_down_left = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/" + file_name[:-4] + '_down_left_input.log', 'r')
    f_lp_new_no_tr_tiers = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/" + file_name[:-4] + '_tiers_input.log', 'r')
    # f_lp_new_no_tr_reverse_tiers = open("/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/" + file_name[:-4] + '_reverse_tiers_input.log', 'r')
    # lst_greedy = f_greedy.readline().split(':')
    # graph_name = lst_greedy[0][2:-1][6:11]
    # graph_name = lst_greedy[0][2:-1].split('_new')[0]
    # cost_greedy = int(lst_greedy[2].split(',')[0])
    # lst_sp = f_sp.readline().split(':')
    # ost_sp = int(lst_sp[2].split(',')[0])
    text = f_lp_new_tr_default.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    cost_lp = []
    files = []
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_tr_default')
    text = f_lp_new_tr_up_right.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_tr_up_right')
    text = f_lp_new_tr_down_left.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_tr_down_left')
    text = f_lp_new_tr_tiers.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_tr_tiers')
    '''
    text = f_lp_new_tr_reverse_tiers.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_tr_reverse_tiers')
    '''
    text = f_lp_new_no_tr_default.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_no_tr_default')
    text = f_lp_new_no_tr_up_right.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_no_tr_up_right')
    text = f_lp_new_no_tr_down_left.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_no_tr_down_left')
    text = f_lp_new_no_tr_tiers.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_no_tr_tiers')
    '''
    text = f_lp_new_no_tr_reverse_tiers.read()
    string = text.split('SCIP Status')[1].split('\n')[0]
    if 'solved' in string and 'infeasible' not in string:
        cost_lp.append(int(text.split('objective value:')[1].split()[0]))
        files.append('f_lp_new_no_tr_reverse_tiers')
    '''
    if 'infeasible' in string:
        print('infeasible')
    elif len(set(cost_lp)) == 0:
        print('not solved')
    elif len(set(cost_lp)) > 1:
        print('different answers')
    else:
        cost_lp = cost_lp[0]
        files = files[0]
    print(graph_name, cost_lp, files)
    data_names.append(graph_name)
    # data_greedy.append(cost_greedy)
    # data_sp.append(cost_sp)
    data_lp.append(cost_lp)
    # f_sp.close()
    # f_greedy.close()
    f_lp_new_tr_default.close()
    f_lp_new_tr_up_right.close()
    f_lp_new_tr_down_left.close()
    f_lp_new_tr_tiers.close()
    # f_lp_new_tr_reverse_tiers.close()
    f_lp_new_no_tr_default.close()
    f_lp_new_no_tr_up_right.close()
    f_lp_new_no_tr_down_left.close()
    f_lp_new_no_tr_tiers.close()
    # f_lp_new_no_tr_reverse_tiers.close()

"""
# задаем размеры
plt.figure(figsize=(15,6))

# заголовок
plt.title('Значения целевой функции разных алгоритмов на разных графах')

# ширина столбцов
width = 0.4

# координаты столбцов
ids = np.arange(1, len(data_names) + 1)

# рисуем графики
'''
plt.bar(ids - width / 2, data_greedy, width / 2, label='greedy')
plt.bar(ids, data_sp, width / 2, label='sp')
plt.bar(ids + width / 2, data_lp, width / 2, label='lp')
'''

plt.bar(ids - width / 4, data_sp, width / 2, label='sp')
plt.bar(ids + width / 4, data_lp, width / 2, label='lp')
# метки по оси x
plt.xticks(ids, data_names, rotation=45)

# подписи осей
plt.xlabel('Граф')
plt.ylabel('Целевая функция')

# легенда для разных цветов
plt.legend()

# сетка графика
plt.grid(True)

plt.show()
"""
