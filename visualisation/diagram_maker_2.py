import os
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
'''
# Указываем путь к директории
directory = "/Users/maxbig/single-proc-alg/SchedulingProblemLib/build/Answer/greedy/schedules/best"
directory2 = "/Users/maxbig/Coursework/build/Answer/Greedy/schedules/best"

# Получаем список файлов
files = os.listdir(directory)
if '.DS_Store' in files:
    files.remove('.DS_Store')

# Выводим список файлов
print(files)

data_names = []
data_greedy_old = []
data_greedy_new = []

for file_name in files:
    f_greedy_old = open(directory + "/" + file_name, 'r')
    f_greedy_new = open(directory2 + "/" + file_name[:-5] + '_new.json', 'r')
    lst_greedy_old = f_greedy_old.readline().split(':')
    graph_name = lst_greedy_old[0][2:-1]
    cost_greedy_old = int(lst_greedy_old[2].split(',')[0])
    lst_greedy_new = f_greedy_new.readline().split(':')
    cost_greedy_new = int(lst_greedy_new[2].split(',')[0])
    print(graph_name, cost_greedy_old, cost_greedy_new)
    data_names.append(graph_name)
    data_greedy_old.append(cost_greedy_old)
    data_greedy_new.append(cost_greedy_new)
    f_greedy_old.close()
    f_greedy_new.close()

# задаем размеры
plt.figure(figsize=(15,6))

# заголовок
plt.title('Значения целевой функции разных алгоритмов на разных графах')

# ширина столбцов
width = 0.2

# координаты столбцов
ids = np.arange(1, len(data_names) + 1)

# рисуем графики
plt.bar(ids - width / 2, data_greedy_old, width, label='greedy old')
plt.bar(ids + width / 2, data_greedy_new, width, label='greedy new')

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
'''

# Указываем путь к директории
directory = "/Users/maxbig/single-proc-alg/SchedulingProblemLib/build/Answer/aco/schedules/best"
directory2 = "/Users/maxbig/Coursework/build/Answer/iterative/schedules/best"
"""
# Получаем список файлов
files = os.listdir(directory)
if '.DS_Store' in files:
    files.remove('.DS_Store')

# Выводим список файлов
print(files)

data_names = []
data_greedy_old = []
data_greedy_new = []

for file_name in files:
    f_greedy_old = open(directory + "/" + file_name, 'r')
    f_greedy_new = open(directory2 + "/" + file_name[:-5] + '_new.json', 'r')
    lst_greedy_old = f_greedy_old.readline().split(':')
    graph_name = lst_greedy_old[0][2:-1]
    cost_greedy_old = int(lst_greedy_old[2].split(',')[0])
    lst_greedy_new = f_greedy_new.readline().split(':')
    cost_greedy_new = int(lst_greedy_new[2].split(',')[0])
    print(graph_name, cost_greedy_old, cost_greedy_new)
    if cost_greedy_old != cost_greedy_new:
        data_names.append(graph_name)
        data_greedy_old.append(cost_greedy_old)
        data_greedy_new.append(cost_greedy_new)
    f_greedy_old.close()
    f_greedy_new.close()
"""

data_names = ['2', '3', '4']
data_lp = [71, 54, 50]
data_greedy = [73, 67, 50]

# задаем размеры
plt.figure(figsize=(15,6))

# заголовок
plt.title('ЦФ на графе sp_14_buf_times при M = 10000')

# ширина столбцов
width = 0.2

# координаты столбцов
ids = np.arange(1, len(data_names) + 1)

# рисуем графики
plt.bar(ids - width / 4, data_greedy, width / 2, label='greedy')
plt.bar(ids + width / 4, data_lp, width / 2, label='lp')

# метки по оси x
plt.xticks(ids, data_names)

# подписи осей
plt.xlabel('Кол-во процессоров')
plt.ylabel('Целевая функция')

# легенда для разных цветов
plt.legend()

# сетка графика
plt.grid(True)

plt.tight_layout()

plt.show()
