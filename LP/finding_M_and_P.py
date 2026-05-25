import os
import shutil
import subprocess

directory2 = "/Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs"
# directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/G'
directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/scalability/new_triadags_time'
directory3 = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/scalability/new_triadags_time_P_and_M'

dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_layered_time',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_random_time',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_triadags_time',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/decision_quality/new_sp_time']

dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/scalability/new_layered_time'] + dirs

for directory in dirs:
    directory3 = directory + '_P_and_M'

    # Получаем список файлов
    files = os.listdir(directory)

    if '.DS_Store' in files:
        files.remove('.DS_Store')
    files2 = os.listdir(directory2)

    files3 = os.listdir(directory3)

    for file_name3 in files3:
        os.remove(directory3 + '/' + file_name3)


    files.sort(key=lambda x: int(x.split('_')[1]))
    dct0 = {}
    ii = 0
    for file_name in files:
        '''
        ii += 1
        if ii == 5:
            break
        '''
        print(file_name)
        
        bfrs, times = {}, {}
        f = open(f'{directory}/{file_name}' , "r")
        nodes = []
        sizes = {}
        children = {}
        nodes_with_par = set()
        lines = f.readlines()[1:]
        for line in lines:
            split_line = line.split()
            node = split_line[0]
            time = split_line[1]
            times[int(node)] = int(time)
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
        
        R = {}
        for key, value in bfrs.items():
            for j in range(1, value[0] + 1):
                R[(int(key), j)] = int(value[j])
        # print(R)
        
        for file_name2 in files2:
            os.remove(directory2 + '/' + file_name2)
        shutil.copy(directory + '/' + file_name, directory2 + '/' + file_name)
        files2 = os.listdir(directory2)
        if '.DS_Store' in files2:
            files2.remove('.DS_Store')
        Max_Ms = []
        Max_Ps = []
        for alg in ['greedy']:
            output = subprocess.check_output('/Users/maxbig/Coursework_multiprocessing/Coursework/build/application ' \
                f'--command run --algo {alg} --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs ' \
                '--output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 ' \
                f'--batch 1 --processors {99} --memory {9999999}', shell=True, text=True)
            
            with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{alg}/schedules/best/{file_name[:-4]}.json') as f:
                text = f.readline()
                dct = eval(text)
                name = list(dct.keys())[0]
                dct = dct[name]
                makespan = dct['makespan']
                # print('makespan on extreme big P and M:', makespan)
                runtime_sec = dct['runtime_sec']
                size = dct['size']
                schedule = dct['schedule']
                
                tasks = {}
                max_fin = 0
                processors = set()
                for vertex, params in schedule.items():
                    processor = params['processor']
                    processors.add(processor)
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
            max_M_usage = 0
            max_P_usage = 0
            active_works = []
            for t in range(max_fin + 1):
                if t in tasks:
                    for v, status in tasks[t]:
                        if status == 'start':
                            active_works.append(v)
                            childs = children[v]
                            for num in range(1, bfrs[v][0] + 1):
                                usage[(v, num, bfrs[v][num])] = []
                                for v2 in childs:
                                    if sizes.get((v, v2)) == num:
                                        usage[(v, num, bfrs[v][num])] += [v2]
                        else:
                            active_works.remove(v)
                            usage2 = usage.copy()
                            for key, value in usage2.items():
                                if v in value:
                                    usage[key].remove(v)
                                    if usage[key] == []:
                                        del usage[key]
                max_P_usage = max(max_P_usage, len(active_works))
                usage_val = 0
                for v, num, value in list(usage.keys()):
                    usage_val += int(value)
                max_M_usage = max(max_M_usage, usage_val)
            max_P_real = len(processors)
            if max_P_real >= 50:
                PP = [4, (4 + max_P_real) // 2, max_P_real]
            else:
                PP = [2, (2 + max_P_real) // 2, max_P_real]
            
            output = subprocess.check_output('/Users/maxbig/Coursework_multiprocessing/Coursework/build/application ' \
                f'--command run --algo {alg} --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs ' \
                '--output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 ' \
                f'--batch 1 --processors {PP[0]} --memory {9999999}', shell=True, text=True)
            
            with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{alg}/schedules/best/{file_name[:-4]}.json') as f:
                text = f.readline()
                dct = eval(text)
                name = list(dct.keys())[0]
                dct = dct[name]
                makespan = dct['makespan']
                # print('makespan on extreme big P and M:', makespan)
                runtime_sec = dct['runtime_sec']
                size = dct['size']
                schedule = dct['schedule']
                
                tasks = {}
                max_fin = 0
                processors = set()
                for vertex, params in schedule.items():
                    processor = params['processor']
                    processors.add(processor)
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
            max_M_usage_1 = 0
            max_P_usage = 0
            active_works = []
            for t in range(max_fin + 1):
                if t in tasks:
                    for v, status in tasks[t]:
                        if status == 'start':
                            active_works.append(v)
                            childs = children[v]
                            for num in range(1, bfrs[v][0] + 1):
                                usage[(v, num, bfrs[v][num])] = []
                                for v2 in childs:
                                    if sizes.get((v, v2)) == num:
                                        usage[(v, num, bfrs[v][num])] += [v2]
                        else:
                            active_works.remove(v)
                            usage2 = usage.copy()
                            for key, value in usage2.items():
                                if v in value:
                                    usage[key].remove(v)
                                    if usage[key] == []:
                                        del usage[key]
                max_P_usage = max(max_P_usage, len(active_works))
                usage_val = 0
                for v, num, value in list(usage.keys()):
                    usage_val += int(value)
                max_M_usage_1 = max(max_M_usage_1, usage_val)
            
            output = subprocess.check_output('/Users/maxbig/Coursework_multiprocessing/Coursework/build/application ' \
                f'--command run --algo {alg} --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs ' \
                '--output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 ' \
                f'--batch 1 --processors {PP[1]} --memory {9999999}', shell=True, text=True)
            
            with open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/{alg}/schedules/best/{file_name[:-4]}.json') as f:
                text = f.readline()
                dct = eval(text)
                name = list(dct.keys())[0]
                dct = dct[name]
                makespan = dct['makespan']
                # print('makespan on extreme big P and M:', makespan)
                runtime_sec = dct['runtime_sec']
                size = dct['size']
                schedule = dct['schedule']
                
                tasks = {}
                max_fin = 0
                processors = set()
                for vertex, params in schedule.items():
                    processor = params['processor']
                    processors.add(processor)
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
            max_M_usage_2 = 0
            max_P_usage = 0
            active_works = []
            for t in range(max_fin + 1):
                if t in tasks:
                    for v, status in tasks[t]:
                        if status == 'start':
                            active_works.append(v)
                            childs = children[v]
                            for num in range(1, bfrs[v][0] + 1):
                                usage[(v, num, bfrs[v][num])] = []
                                for v2 in childs:
                                    if sizes.get((v, v2)) == num:
                                        usage[(v, num, bfrs[v][num])] += [v2]
                        else:
                            active_works.remove(v)
                            usage2 = usage.copy()
                            for key, value in usage2.items():
                                if v in value:
                                    usage[key].remove(v)
                                    if usage[key] == []:
                                        del usage[key]
                max_P_usage = max(max_P_usage, len(active_works))
                usage_val = 0
                for v, num, value in list(usage.keys()):
                    usage_val += int(value)
                max_M_usage_2 = max(max_M_usage_2, usage_val)
            
            Max_Ms.append(max(max_M_usage, max_M_usage_1, max_M_usage_2))
            Max_Ps.append(max_P_real)
        
        max_M_usage = max(Max_Ms)
        max_P_real = max(Max_Ps)
        if max_P_real >= 50:
            PP = [4, (4 + max_P_real) // 2, max_P_real]
        else:
            PP = [2, (2 + max_P_real) // 2, max_P_real]
        
        if len(set(PP)) < 3:
            PP = [2, 3, 4]
        
        # min_M_usage = max_M_usage // 10
        # gap = max_M_usage // 100
        min_M_usage = 1
        gap = 1
        flag0 = 0
        while not flag0:
            min_M_usage += gap
            try:
                for alg in ['greedy']:
                    for p in PP:
                        for m in [min_M_usage, (min_M_usage + max_M_usage) // 2, max_M_usage]:
                            # print(alg, p, m)
                            output = subprocess.check_output('/Users/maxbig/Coursework_multiprocessing/Coursework/build/application ' \
                                f'--command run --algo {alg} --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs ' \
                                '--output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 ' \
                                f'--batch 1 --processors {p} --memory {m}', shell=True, text=True)
                flag0 = 1
            except Exception:
                pass
        
        MM = [min_M_usage, (min_M_usage + max_M_usage) // 2, max_M_usage]
        
        if len(set(MM)) < 3:
            MM = [min_M_usage, int(min_M_usage * 1.01 + 1), int(min_M_usage * 1.02 + 1)]
        
        for p in PP:
            for m in MM:
                shutil.copy(directory + '/' + file_name, directory3 + '/' + file_name[:-4] + f'_{p}_{m}.txt')
        
        print(PP, MM)
        
