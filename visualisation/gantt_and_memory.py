import os

import matplotlib

if os.environ.get("GANTT_SAVE", "0") == "1":
    matplotlib.use("Agg")

import matplotlib.pyplot as plt
import random
import matplotlib.patches as patches
from matplotlib.patches import FancyArrowPatch, Circle, Ellipse
from matplotlib.patches import ConnectionPatch
from collections import defaultdict
from matplotlib.patches import Polygon



def draw_horiz_arrow(ax, from_vertex, to_vertex, schedule, color='blue', style='->'):
    """Прямая линия"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    to_coords = get_task_coordinates(schedule, to_vertex)
    
    arrow = FancyArrowPatch(
        (from_coords['x_finish'] - from_coords['duration'] * 0.3, from_coords['y_center'] + 0.1),
        (to_coords['x_start'] + to_coords['duration'] * 0.3, to_coords['y_center'] + 0.1),
        arrowstyle=style,
        color=color,
        linewidth=1,
        mutation_scale=11,
        connectionstyle="arc3,rad=0"
    )
    ax.add_patch(arrow)


def draw_loop_arrow_ellipse(ax, vertex, schedule, color='purple', style='->'):
    """Эллиптическая петля на одной задаче"""
    coords = get_task_coordinates(schedule, vertex)
    
    # Петля выходит из правого конца и возвращается в левый
    x_start = coords['x_finish']
    x_end = coords['x_start']
    y = coords['y_center']
    
    # Используем Angle3 для создания дуги с заданным углом
    arrow = FancyArrowPatch(
        (x_start, y),
        (x_end, y),
        arrowstyle=style,
        color=color,
        linewidth=2,
        connectionstyle="angle3,angleA=90,angleB=90"  # вертикальные углы
    )
    ax.add_patch(arrow)


def get_task_coordinates(schedule, vertex, processor=None):
    """Получить координаты задачи на Gantt-диаграмме"""
    params = schedule[vertex]
    proc = processor if processor else params['processor']
    start = params['start']
    finish = params['finish']
    duration = finish - start
    
    # Y-координата: процессоры идут сверху вниз, но с инвертированной осью
    y_center = 0.75 + proc  # Центр полосы
    y_top = y_center + 0.25  # Верх полосы
    y_bottom = y_center - 0.25  # Низ полосы
    
    return {
        'start': start,
        'finish': finish,
        'duration': duration,
        'x_center': start + duration/2,
        'x_start': start,
        'x_finish': finish,
        'y_center': y_center,
        'y_top': y_top,
        'y_bottom': y_bottom
    }


def draw_vertical_arrow(ax, from_vertex, to_vertex, schedule, color='red', style='->'):
    """Вертикальная стрелка между задачами"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    to_coords = get_task_coordinates(schedule, to_vertex)
    
    y1, y2 = (from_coords['y_bottom'] + 0.02, to_coords['y_top'] - 0.04) if from_coords['y_center'] > to_coords['y_center'] else (from_coords['y_top'] - 0.03, to_coords['y_bottom'] + 0.03)
    
    arrow = FancyArrowPatch(
        (from_coords['x_finish'], y1),
        (to_coords['x_start'], y2),
        arrowstyle=style,
        color=color,
        linewidth=1,
        mutation_scale=11,
        connectionstyle="arc3,rad=0"  # rad=0 для прямой
    )
    ax.add_patch(arrow)


def draw_diagonal_arrow(ax, from_vertex, to_vertex, schedule, color='blue', style='->'):
    """Диагональная стрелка (прямая линия)"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    to_coords = get_task_coordinates(schedule, to_vertex)
    
    y1, y2 = (from_coords['y_bottom'] + 0.02, to_coords['y_top'] - 0.04) if from_coords['y_center'] > to_coords['y_center'] else (from_coords['y_top'] - 0.03, to_coords['y_bottom'] + 0.03)
    
    arrow = FancyArrowPatch(
        (from_coords['x_finish'], y1),
        (to_coords['x_start'] + 0.1, y2),
        arrowstyle=style,
        color=color,
        linewidth=1,
        mutation_scale=11,
        connectionstyle="arc3,rad=0"
    )
    ax.add_patch(arrow)


def draw_arc_arrow(ax, from_vertex, to_vertex, schedule, color='green', style='->', curvature=0.3):
    """Дугообразная стрелка"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    to_coords = get_task_coordinates(schedule, to_vertex)
    
    length = to_coords['x_start'] - from_coords['x_finish']
    
    arrow = FancyArrowPatch(
        (from_coords['x_finish'] - 0.1, from_coords['y_bottom']),
        (to_coords['x_start'] + 0.2, to_coords['y_bottom']),
        arrowstyle=style,
        color=color,
        linewidth=1,
        mutation_scale=11,
        connectionstyle=f"arc3,rad={-curvature * 10 / (length**0.8)}"  # положительное значение = дуга вверх
    )
    ax.add_patch(arrow)


def draw_vert_arc_arrow(ax, from_vertex, to_vertex, schedule, color='green', style='->', curvature=0.3):
    """Дугообразная стрелка"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    to_coords = get_task_coordinates(schedule, to_vertex)
    
    arrow = FancyArrowPatch(
        (from_coords['x_finish'], from_coords['y_bottom']),
        (to_coords['x_start'], to_coords['y_bottom']),
        arrowstyle=style,
        color=color,
        linewidth=1,
        mutation_scale=11,
        connectionstyle=f"arc3,rad={-curvature}"  # положительное значение = дуга вверх
    )
    ax.add_patch(arrow)


def draw_loop_arrow(ax, from_vertex, to_vertex, schedule, color='purple', style='->'):
    """Петлеобразная стрелка"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    
    if from_vertex == to_vertex:
        # Петля на себе
        # Создаем круговую петлю
        x = from_coords['x_finish']
        y = from_coords['y_bottom']
        
        # Используем ConnectionPatch с круговым путем
        '''
        circle = Circle((x, y - 0.2), 0.2, fill=False, 
                       edgecolor=color, linewidth=1.3)
        ax.add_patch(circle)
        '''
        ellipse = Ellipse(
            (x, y - 0.2),  # центр
            width=0.5,
            height=0.4,
            angle=0,
            fill=False,
            edgecolor=color,
            linewidth=1
        )
        ax.add_patch(ellipse)
        
        # Добавляем стрелку на конце
        arrow = FancyArrowPatch(
            (x + 0.25, y - 0.07),
            (x - 0.1, y + 0.05),
            arrowstyle=style,
            color=color,
            linewidth=1,
            mutation_scale=11,
        )
        ax.add_patch(arrow)
    else:
        # Петля с возвратом
        to_coords = get_task_coordinates(schedule, to_vertex)
        mid_x = (from_coords['x_finish'] + to_coords['x_start']) / 2
        mid_y = min(from_coords['y_center'], to_coords['y_center']) - 0.5
        
        # Путь через точку ниже
        arrow = FancyArrowPatch(
            (from_coords['x_finish'], from_coords['y_center']),
            (to_coords['x_start'], to_coords['y_center']),
            arrowstyle=style,
            color=color,
            linewidth=1,
            mutation_scale=11,
            connectionstyle=f"arc3,rad=-0.5"  # отрицательное значение = дуга вниз
        )
        ax.add_patch(arrow)


def draw_bezier_arrow(ax, from_vertex, to_vertex, schedule, color='orange', style='->'):
    """Стрелка по кривой Безье"""
    from_coords = get_task_coordinates(schedule, from_vertex)
    to_coords = get_task_coordinates(schedule, to_vertex)
    
    from matplotlib.path import Path
    import matplotlib.patches as patches
    
    # Контрольные точки для кривой Безье
    verts = [
        (from_coords['x_finish'], from_coords['y_center']),  # начало
        (from_coords['x_finish'] + 1, from_coords['y_center'] + 0.5),  # контрольная 1
        (to_coords['x_start'] - 1, to_coords['y_center'] + 0.5),  # контрольная 2
        (to_coords['x_start'], to_coords['y_center'])  # конец
    ]
    
    codes = [Path.MOVETO, Path.CURVE4, Path.CURVE4, Path.CURVE4]
    
    path = Path(verts, codes)
    patch = patches.PathPatch(path, facecolor='none', edgecolor=color, linewidth=2)
    ax.add_patch(patch)
    
    # Добавляем стрелку в конце (можно использовать FancyArrowPatch)
    arrow = FancyArrowPatch(
        verts[2], verts[3],
        arrowstyle=style,
        color=color,
        linewidth=2
    )
    ax.add_patch(arrow)


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

# file_name = 'layered_14_buf_times'
# pm = '_P4_M2200'
file_name = os.environ.get('GANTT_FILE_NAME', 'intro_graph_example')
pm = os.environ.get('GANTT_PM', '')
# p_m = pm + '_for_kbh'
p_m = pm
algo = os.environ.get('GANTT_ALGO', 'sao')
title = os.environ.get('GANTT_TITLE', 'SAO with fed')
diag_name = os.environ.get('GANTT_OUTPUT', 'sao_12_fed.png')
save0 = int(os.environ.get('GANTT_SAVE', '0'))

P = int(os.environ.get('GANTT_P', '3'))
M = int(os.environ.get('GANTT_M', '50'))

'''
with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/inputs/new_no_tr/order/{file_name + pm}_down_left_input.lp') as f:
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
'''

# ==================== ВЕРХНИЙ ГРАФИК (Gantt) ====================

with open(f'../build/Answer/{algo}/schedules/best/{file_name + p_m}.json') as f:
    text = f.readline()
    dct = eval(text)
    name = list(dct.keys())[0]
    dct = dct[name]
    makespan = dct['makespan']
    runtime_sec = dct['runtime_sec']
    size = dct['size']
    schedule = dct['schedule']
    
    tasks = {}
    tasks_v = {}
    for vertex, params in schedule.items():
        processor = params['processor']
        start = params['start']
        finish = params['finish']
        duration = finish - start
        if processor not in tasks:
            tasks[processor] = [(start, duration)]
        else:
            tasks[processor] += [(start, duration)]
        tasks_v[(processor, start, duration)] = vertex
    tasks = dict(sorted(tasks.items()))
    
    # Рисуем полосы
    Y = 0.5
    H = 0.5
    stage = 0.5
    all_colors = [(1, 0, 0), (1, 0.5, 0), (0.9, 0.9, 0.1), (0.5, 1, 0), (0, 1, 0), (0, 1, 0.5), (0, 1, 1), (0, 0.5, 1), (0, 0, 1), (0.5, 0, 1), (1, 0, 1), (1, 0, 0.5)]
    random_colors = []
    nums = [i for i in range(12)]
    random.seed(42)
    for i in range(1000):
        c = random.choice(nums)
        random_colors.append(all_colors[c])
        nums = [j for j in range(12)]
        for j in range(c - 2, c + 3):
            nums.remove((12 + j) % 12)
    
    random_colors = [all_colors[0], all_colors[3], all_colors[6], all_colors[10], all_colors[1], all_colors[4], all_colors[7], all_colors[11], all_colors[2], all_colors[9], all_colors[5], all_colors[8]] * 100
    '''
    i = 0
    
    for num, task in tasks.items():
        c = random_colors[i:i + len(task)]
        i += len(task)
        ax1.broken_barh(sorted(task), (Y, H), facecolors=c)
        Y += 1
    '''
    
    for prc, task0 in tasks.items():
        task = sorted(task0)
        c = [random_colors[int(tasks_v[(prc, strt, dur)])] for strt, dur in task]
        ax1.broken_barh(sorted(task), (Y, H), facecolors=c)
        white_bars = [(tsk[0] - 0.05, 0.1) for tsk in task]
        ax1.broken_barh(white_bars, (Y, H), facecolors=['white'] * len(white_bars))
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
    # ax1.set_ylabel('Процессоры', fontsize=12)
    ax1.set_title(title, fontsize=14, fontweight='bold')
    # ax1.invert_yaxis()
    ax1.grid(True, axis='x', linestyle='--', alpha=0.6)
    ax1.set_xlim(0, makespan)
    ax1.set_ylim(0, 0.75 + P - 1 + 0.75)
    ax1.invert_yaxis()
    ax1.text(
        x=makespan - 0.5,  # позиция по X (в долях от ширины графика, 0.02 = 2% от левого края)
        y=(0.75 + P - 1 + 0.75) * 1.029,     # позиция по Y (на уровне линии)
        s=f'{makespan}',  # текст подписи
        color='black',
        fontsize=10,
        fontweight='bold',
        va='bottom' if M < ax1.get_ylim()[1] * 0.9 else 'top',  # автоматическое выравнивание
        ha='left'
    )


# ==================== НИЖНИЙ ГРАФИК (Memory Usage) ====================
viz = ResourceVisualizer()

with open(f'../build/Graphs/{file_name}.txt') as f:
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

# print(sizes)

for root_v in root_parents:
    crds = get_task_coordinates(schedule, root_v)
    
    width = min(0.007 * makespan, 1)
    height = min(0.05 * (0.75 + P - 1 + 0.75), 0.5)
    
    pts = [[crds['x_center'] - width / 2, crds['y_bottom'] - height], [crds['x_center'] + width / 2, crds['y_bottom'] - height], [crds['x_center'], crds['y_bottom']]]
    triangle = Polygon(pts, closed=True, color='black', alpha=0.5)
    ax1.add_patch(triangle)


for v1, v2 in list(sizes.keys()):
    coords1 = get_task_coordinates(schedule, v1)
    coords2 = get_task_coordinates(schedule, v2)
    if coords1['x_finish'] == coords2['x_start']:
        if coords1['y_center'] == coords2['y_center']:
            # draw_loop_arrow(ax1, v1, v1, schedule, color='black')  # петля на себе
            draw_horiz_arrow(ax1, v1, v2, schedule, color='black')
            # draw_arc_arrow(ax1, v1, v2, schedule, color='black', curvature=15, style='->')
            # draw_loop_arrow_ellipse(ax1, v1, schedule)
        else:
            if abs(coords1['y_center'] - coords2['y_center']) == 1:
                draw_vertical_arrow(ax1, v1, v2, schedule, color='black')
            else:
                draw_vert_arc_arrow(ax1, v1, v2, schedule, color='black', curvature=0.2, style='->')

    elif coords1['y_center'] == coords2['y_center']:
        draw_arc_arrow(ax1, v1, v2, schedule, color='black', curvature=0.2, style='->')
    else:
        draw_diagonal_arrow(ax1, v1, v2, schedule, color='black')


with open(f'../build/Answer/{algo}/schedules/best/{file_name + p_m}.json') as f:
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

# Добавить подпись к красной горизонтальной линии
ax2.text(
    x=0.2,  # позиция по X (в долях от ширины графика, 0.02 = 2% от левого края)
    y=M*1.05,     # позиция по Y (на уровне линии)
    s=f'{M}',  # текст подписи
    color='red',
    fontsize=10,
    va='bottom' if M < ax2.get_ylim()[1] * 0.9 else 'top',  # автоматическое выравнивание
    ha='left'
)

ax2.text(
    x=makespan - 0.5,  # позиция по X (в долях от ширины графика, 0.02 = 2% от левого края)
    y=-M*0.03,     # позиция по Y (на уровне линии)
    s=f'{makespan}',  # текст подписи
    color='black',
    fontsize=10,
    fontweight='bold',
    va='bottom' if M < ax2.get_ylim()[1] * 0.9 else 'top',  # автоматическое выравнивание
    ha='left'
)

# Отрисовываем нижний график на ax2
viz.visualize(ax2)
# ax2.set_title('Динамика использования памяти', fontsize=14, fontweight='bold')

plt.tight_layout()

if save0:
    plt.savefig(diag_name, dpi=300, bbox_inches='tight', facecolor=fig.get_facecolor())

plt.show()
