import os
from re import findall

# directory = "/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/mix2"
directory = "/Users/maxbig/Coursework_multiprocessing/Coursework/LP/G"

files = os.listdir(directory)
if '.DS_Store' in files:
    files.remove('.DS_Store')
# files.sort(key=lambda x: (int(x.split('_')[1]), x.split('_')[0], x.split('_')[2], x.split('_')[3], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))

for file_n in files:
    '''
    if int(file_n.split('_')[1]) > 21:
        break
    '''
    file = file_n[:-4]
    try:
        f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/{file}_default_input.log", 'r')
        lines = f_lp.read()
        if 'problem is solved [optimal solution found]' not in lines:
            f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/{file}_up_right_input.log", 'r')
            lines = f_lp.read()
            if 'problem is solved [optimal solution found]' not in lines:
                f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/{file}_down_left_input.log", 'r')
                lines = f_lp.read()
                if 'problem is solved [optimal solution found]' not in lines:
                    f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_tr/{file}_tiers_input.log", 'r')
                    lines = f_lp.read()
                    if 'problem is solved [optimal solution found]' not in lines:
                        f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/{file}_default_input.log", 'r')
                        lines = f_lp.read()
                        if 'problem is solved [optimal solution found]' not in lines:
                            f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/{file}_up_right_input.log", 'r')
                            lines = f_lp.read()
                            if 'problem is solved [optimal solution found]' not in lines:
                                f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/{file}_down_left_input.log", 'r')
                                lines = f_lp.read()
                                if 'problem is solved [optimal solution found]' not in lines:
                                    f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/{file}_tiers_input.log", 'r')
                                    lines = f_lp.read()
                                    if 'problem is solved [optimal solution found]' not in lines: # not solved!
                                        continue
                                        f_lp = open(f"/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/logs/new_no_tr/{file}_default_input.log", 'r')
                                        lines = f_lp.read()
    except Exception:
        continue
    lines = lines.split('\n')
    lines2 = lines.copy()
    
    lines = lines[lines.index(' time | node  | left  |LP iter|LP it/n|mem/heur|mdpt |vars |cons |rows |cuts |sepa|confs|strbr|  dualbound   | primalbound  |  gap   | compl. '):lines.index('primal solution (original space):')]
    i = 0
    time, gap = [0], [100]
    while not lines[i].startswith('Solving Time (sec) :'):
        if lines[i].count('|') == 17 and not lines[i].startswith(' time'):
            splt = lines[i].split('|')
            i1 = 0
            while not splt[0][i1].isdigit():
                i1 += 1
            i2 = i1 + 1
            while splt[0][i2] != 's' and splt[0][i2] != 'm':
                i2 += 1
            time.append(float(splt[0][i1:i2]))
            if splt[0][i2] == 'm':
                time[-1] *= 60
            if 'Inf' in splt[16]:
                gap.append(100)
            else:
                gap.append(float(splt[16].split('%')[0]))
        i += 1
    fin_time = float(lines[i].split(' : ')[1])
    i = 0
    while i < len(gap) and gap[i] > 20:
        i += 1
    if i == len(gap):
        time_20 = '-'
    else:
        time_20 = time[i]
    while i < len(gap) and gap[i] > 15:
        i += 1
    if i == len(gap):
        time_15 = '-'
    else:
        time_15 = time[i]
    while i < len(gap) and gap[i] > 10:
        i += 1
    if i == len(gap):
        time_10 = '-'
    else:
        time_10 = time[i]
    print(f'{file} = [{time_20}, {time_15}, {time_10}, {fin_time}]')
    f_lp.close()
    
    
    file0 = file_n
    ff = open(directory + '/' + file0, 'r')
    liness = ff.readlines()[1:]
    m = {}
    VVV = []
    for line in liness:
        VVV.append(int(line.split()[0]))
        m[int(line.split()[0])] = round(float(line.split()[1]))
    m_d = m
    ff.close()
    
    lines = lines2.copy()
    
    makespan = 0
    m = {}
    # vertexes = [0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 14, 15, 17, 19]
    vertexes = VVV.copy()
    for line in lines:
        if line[:2] == 's_':
            m[round(float(line.split()[0][2:]))] = round(float(line.split()[1]))
            vertexes.remove(round(float(line.split()[0][2:])))
            makespan = max(makespan, round(float(line.split()[1])) + m_d[round(float(line.split()[0][2:]))])
    for v in vertexes:
        m[v] = 0
        makespan = max(makespan, m_d[v])
    m_s = m

    m = {}
    for line in lines:
        if line[:2] == 'p_':
            qq = line.split()[0][2:].split('_')
            m[int(qq[1])] = int(qq[0]) - 1
    m_p = m
    # print(m_d, m_s, m_p, makespan)
    
    # vertexes = [0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 14, 15, 17, 19]
    vertexes = VVV.copy()
    strk = '{"' + file + '":{"makespan":' + str(makespan) + ',"runtime_sec":' + str(fin_time) + ',"size":' + str(len(vertexes)) + ',"schedule":{'
    lst = []
    for v in vertexes:
        lst.append('"' + str(v) + '":{"processor":' + str(m_p[v]) + ',"start":' + str(m_s[v]) + ',"finish":' + str(m_s[v] + m_d[v]) + '}')
    strk += ','.join(lst)
    strk += '}}}'
    fff = open(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer/lp/schedules/best/{file}.json', 'w')
    fff.write(strk)
    fff.close()
