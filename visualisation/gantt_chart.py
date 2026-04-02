import matplotlib.pyplot as plt
import random

fig, ax = plt.subplots(figsize=(10, 5))

with open('/Users/maxbig/Courseworw_multiprocessing/Coursework/build/Answer/lp/schedules/best/layered_15_buf_times_P4_M1350.json') as f:
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
    print(tasks)
    
    '''
    Selected task 0 on processor 0 from 0 to 5 (duration: 5)
    Selected task 1 on processor 1 from 0 to 7 (duration: 7)
    Selected task 2 on processor 0 from 5 to 13 (duration: 8)
    Selected task 3 on processor 1 from 7 to 17 (duration: 10)
    Selected task 4 on processor 0 from 13 to 23 (duration: 10)
    Selected task 7 on processor 1 from 17 to 22 (duration: 5)
    Selected task 5 on processor 1 from 22 to 30 (duration: 8)
    Selected task 11 on processor 0 from 23 to 28 (duration: 5)
    '''
    
    # Рисуем полосы
    # xranges: список (start, width), yrange: (нижняя граница Y, высота полосы)
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
    # print(tasks)
    for task in list(tasks.values()):
        c = random_colors[i:i + len(task)]
        # print(c)
        i += len(task)
        ax.broken_barh(sorted(task), (Y, H), facecolors=c)
        Y += 1
    for vertex, params in schedule.items():
        processor = params['processor']
        start = params['start']
        finish = params['finish']
        duration = finish - start
        
        # Считаем центр для текста
        x_center = start + duration / 2
        y_center = 0.75 + processor
        
        # Добавляем текст
        ax.text(x_center, y_center, vertex, 
                ha='center', va='center', color='white', fontweight='bold')

    # Настройка осей
    ax.set_yticks([0.75 + k for k in range(len(tasks))])
    ax.set_yticklabels([f'Процессор {k + 1}' for k in range(len(tasks))])
    ax.set_xlabel('Время')
    ax.invert_yaxis() # Чтобы первая задача была сверху
    ax.grid(True, axis='x', linestyle='--', alpha=0.6)
    
    ax.set_xlim(0, makespan)
    
    plt.tight_layout()
    plt.show()

