import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
from collections import defaultdict

class ResourceVisualizer:
    def __init__(self):
        self.data = defaultdict(list)  # {время: [(ресурс, величина), ...]}
        self.horizontal_lines = []
        
    def add_usage(self, time, resource, value):
        """Добавить использование ресурса в момент времени"""
        self.data[time].append((resource, value))
        
    def visualize(self):
        """Построить график динамики использования ресурсов"""
        if not self.data:
            print("Нет данных для отображения")
            return
            
        # Сортируем моменты времени
        times = sorted(self.data.keys())
        
        fig, ax = plt.subplots(figsize=(12, 6))
        
        # Для каждого момента времени строим столбец
        for i, time in enumerate(times):
            resources = self.data[time]
            # Сортируем ресурсы по величине (опционально)
            # resources.sort(key=lambda x: x[1], reverse=True)
            
            bottom = 0
            for resource, value in resources:
                # Создаем прямоугольник
                rect = patches.Rectangle(
                    (i, bottom),  # x, y (ширина столбца 0.8)
                    1,                 # ширина
                    value,               # высота
                    linewidth=1,
                    edgecolor='black',
                    facecolor='dodgerblue',
                    alpha=0.7
                )
                ax.add_patch(rect)
                
                # Добавляем подпись, если прямоугольник достаточно высокий
                ax.text(
                        i + 0.5, bottom + value/2, 
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
        # ax.set_title('Динамика использования ресурсов', fontsize=14, fontweight='bold')
        
        # Устанавливаем метки по оси X
        '''
        ax.set_xticks(range(len(times) + 1))
        ax.set_xticklabels(times + [times[-1] + 1])
        '''
        
        # Устанавливаем границы по оси Y
        max_value = max(sum(v for _, v in self.data[t]) for t in times)
        ax.set_ylim(0, max_value * 1.1)
        
        # Добавляем сетку
        ax.grid(True, axis='y', alpha=0.3, linestyle='--')
        ax.set_axisbelow(True)
        
        ax.set_xlim(0, len(times))
        
        plt.tight_layout()
        plt.show()
    
    def add_horizontal_line(self, y_position, 
                           linestyle='--', color='red', linewidth=2):
        """
        Добавить горизонтальную линию на график
        
        Параметры:
        y_position: float - высота линии
        label: str - подпись линии
        linestyle: str - стиль линии ('-', '--', '-.', ':')
        color: str - цвет линии
        linewidth: float - толщина линии
        """
        self.horizontal_lines.append((y_position, linestyle, color, linewidth))


viz = ResourceVisualizer()

file_name = 'layered_12_2_buf_times_P2_M8500'

with open(f'/Users/maxbig/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs/{file_name}.txt') as f:
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
    # print(nodes, sizes, children, root_parents, times, bfrs, sep='\n\n')

with open(f'/Users/maxbig/Courseworw_multiprocessing/Coursework/build/Answer/greedy/schedules/best/{file_name}.json') as f:
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
    # print(tasks)

usage = {}
max_usage = 0
for t in range(max_fin + 1):
    if t in tasks:
        for v, status in tasks[t]:
            if status == 'start':
                childs = children[v]
                for num in range(1, bfrs[v][0] + 1):
                    usage[(v, num, bfrs[v][num])] = []
                    for v2 in childs:
                        if sizes[(v, v2)] == num:
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
    max_usage = max(max_usage, usage_val)

viz.add_horizontal_line(max_usage, '-', 'red', 1)

viz.visualize()
