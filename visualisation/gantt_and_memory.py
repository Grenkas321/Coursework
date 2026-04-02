import matplotlib.pyplot as plt
import random
import matplotlib.patches as patches
from collections import defaultdict

class ResourceVisualizer:
    def __init__(self):
        self.data = defaultdict(list)
        self.horizontal_lines = []
        
    def add_usage(self, time, resource, value):
        """Добавить использование ресурса в момент времени"""
        self.data[time].append((resource, value))
        
    def visualize(self, ax):
        """Построить график динамики использования ресурсов на переданной оси"""
        if not self.data:
            print("Нет данных для отображения")
            return
        
        data2 = {}
        j = 1
        i0 = 0
        times_list = sorted(self.data.keys())
        for i in range(len(times_list) - 1):
            if self.data[times_list[i]] == self.data[times_list[i + 1]]:
                j += 1
            else:
                data2[(times_list[i0], j)] = self.data[times_list[i]]
                i0 = i + 1
                j = 1
        data2[(times_list[i0], j)] = self.data[times_list[i + 1]]
        '''
        print(self.data)
        print(data2)
        '''
        self.data = data2
        
        # Сортируем моменты времени
        times = sorted(self.data.keys())
        
        # Для каждого момента времени строим столбец
        for time in times:
            resources = self.data[time]
            
            bottom = 0
            for resource, value in resources:
                # Создаем прямоугольник
                rect = patches.Rectangle(
                    (time[0], bottom),
                    time[1],
                    value,
                    linewidth=1,
                    edgecolor='black',
                    facecolor='dodgerblue',
                    alpha=0.7
                )
                ax.add_patch(rect)
                
                # Добавляем подпись
                ax.text(
                    time[0] + 0.5 * time[1], bottom + value/2, 
                    f"{resource}",
                    ha='center', va='center',
                    fontsize=6,
                    fontweight='bold'
                )
                
                bottom += value
        
        # Добавляем горизонтальные линии
        for y_pos, linestyle, color, linewidth in self.horizontal_lines:
            ax.axhline(y=y_pos, linestyle=linestyle, color=color, 
                      linewidth=linewidth, alpha=0.8)
        
        # Настройка осей
        ax.set_xlabel('Время', fontsize=12)
        ax.set_ylabel('Память', fontsize=12)
        
        # Устанавливаем границы по оси Y
        max_value = max(sum(v for _, v in self.data[t]) for t in times)
        ax.set_ylim(0, self.horizontal_lines[0][0] * 1.1)
        
        # Добавляем сетку
        ax.grid(True, axis='y', alpha=0.3, linestyle='--')
        ax.set_axisbelow(True)
        
        ax.set_xlim(0, times[-1][0] + times[-1][1])
    
    def add_horizontal_line(self, y_position, 
                           linestyle='--', color='red', linewidth=2):
        self.horizontal_lines.append((y_position, linestyle, color, linewidth))


# Создаем фигуру с двумя подграфиками (2 строки, 1 колонка)
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 8))

file_name = 'layered_10_buf_times_P2_M2500'
algo = 'lp'

with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/inputs/new_no_tr/order/{file_name}_down_left_input.lp') as f:
    lines = f.readlines()
    prev_line = ''
    q = 0
    for line in lines:
        if '    p_1' in line and ' = 1' in line and q == 0:
            P = line.count('+') + 1
            q = 1
        elif line == 'Bounds\n':
            M = int(prev_line.split('<=')[1])
        prev_line = line

# ==================== ВЕРХНИЙ ГРАФИК (Gantt) ====================
with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{algo}/schedules/best/{file_name}.json') as f:
    text = f.readline()
    dct = eval(text)
    name = list(dct.keys())[0]
    dct = dct[name]
    makespan = dct['makespan']
    runtime_sec = dct['runtime_sec']
    size = dct['size']
    schedule = dct['schedule']
    
    tasks = {}
    for vertex, params in schedule.items():
        processor = params['processor']
        start = params['start']
        finish = params['finish']
        duration = finish - start
        if processor not in tasks:
            tasks[processor] = [(start, duration)]
        else:
            tasks[processor] += [(start, duration)]
    tasks = dict(sorted(tasks.items()))
    
    # Рисуем полосы
    Y = 0.5
    H = 0.5
    stage = 0.5
    all_colors = [(1, 0, 0), (1, 0.5, 0), (1, 1, 0), (0.5, 1, 0), (0, 1, 0), (0, 1, 0.5), (0, 1, 1), (0, 0.5, 1), (0, 0, 1), (0.5, 0, 1), (1, 0, 1), (1, 0, 0.5)]
    random_colors = []
    nums = [i for i in range(12)]
    random.seed(42)
    for i in range(1000):
        c = random.choice(nums)
        random_colors.append(all_colors[c])
        nums = [j for j in range(12)]
        for j in range(c - 2, c + 3):
            nums.remove((12 + j) % 12)
    i = 0
    
    for task in list(tasks.values()):
        c = random_colors[i:i + len(task)]
        i += len(task)
        ax1.broken_barh(sorted(task), (Y, H), facecolors=c)
        Y += 1
    
    for vertex, params in schedule.items():
        processor = params['processor']
        start = params['start']
        finish = params['finish']
        duration = finish - start
        
        x_center = start + duration / 2
        y_center = 0.75 + processor
        
        ax1.text(x_center, y_center, vertex, 
                ha='center', va='center', color='white', fontweight='bold')

    # Настройка осей для верхнего графика
    ax1.set_yticks([0.75 + k for k in range(P)])
    ax1.set_yticklabels([f'Процессор {k + 1}' for k in range(P)])
    ax1.set_ylabel('Процессоры', fontsize=12)
    # ax1.set_title('Gantt-диаграмма выполнения задач', fontsize=14, fontweight='bold')
    # ax1.invert_yaxis()
    ax1.grid(True, axis='x', linestyle='--', alpha=0.6)
    ax1.set_xlim(0, makespan)
    ax1.set_ylim(0, 0.75 + P - 1 + 0.75)
    ax1.invert_yaxis()


# ==================== НИЖНИЙ ГРАФИК (Memory Usage) ====================
viz = ResourceVisualizer()

with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs/{file_name}.txt') as f:
    bfrs, times = {}, {}
    nodes = []
    sizes = {}
    children = {}
    nodes_with_par = set()
    lines = f.readlines()[1:]
    for line in lines:
        split_line = line.split()
        node = split_line[0]
        time = split_line[1]
        times[node] = time
        nodes.append(node)
        children[node] = []
        buffers = ' '.join(split_line[2:]).split(', ')
        if buffers == ['0:']:
            bfrs[node] = (0,)
            continue
        i = 0
        bfrs[node] = (len(buffers),)
        for buf in buffers:
            i += 1
            size, vertexes = buf.split(': ')
            vertexes = vertexes.split()
            for v in vertexes:
                nodes_with_par.add(v)
                sizes[(node, v)] = i
            children[node] += vertexes
            bfrs[node] += (size,)
    f.close()
    root_parents = list(set(nodes) - nodes_with_par)

with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{algo}/schedules/best/{file_name}.json') as f:
    text = f.readline()
    dct = eval(text)
    name = list(dct.keys())[0]
    dct = dct[name]
    makespan = dct['makespan']
    runtime_sec = dct['runtime_sec']
    size = dct['size']
    schedule = dct['schedule']
    
    tasks = {}
    max_fin = 0
    for vertex, params in schedule.items():
        processor = params['processor']
        start = params['start']
        finish = params['finish']
        max_fin = max(max_fin, finish)
        if start not in tasks:
            tasks[start] = [(vertex, 'start')]
        else:
            tasks[start] += [(vertex, 'start')]
        if finish not in tasks:
            tasks[finish] = [(vertex, 'finish')]
        else:
            tasks[finish] += [(vertex, 'finish')]
    tasks = dict(sorted(tasks.items()))

usage = {}
for t in range(max_fin + 1):
    if t in tasks:
        for v, status in tasks[t]:
            if status == 'start':
                childs = children[v]
                for num in range(1, bfrs[v][0] + 1):
                    usage[(v, num, bfrs[v][num])] = []
                    for v2 in childs:
                        if sizes.get((v, v2)) == num:
                            usage[(v, num, bfrs[v][num])] += [v2]
            else:
                usage2 = usage.copy()
                for key, value in usage2.items():
                    if v in value:
                        usage[key].remove(v)
                        if usage[key] == []:
                            del usage[key]
    usage_val = 0
    for v, num, value in list(usage.keys()):
        viz.add_usage(t, f'{v}:{num}', int(value))
        usage_val += int(value)

viz.add_horizontal_line(M, '-', 'red', 1)

# Отрисовываем нижний график на ax2
viz.visualize(ax2)
# ax2.set_title('Динамика использования памяти', fontsize=14, fontweight='bold')

plt.tight_layout()
plt.show()
