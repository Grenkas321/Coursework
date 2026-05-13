import os
import shutil
import time


directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/mix2'
directory2 = '/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs'

files = sorted(os.listdir(directory))
if '.DS_Store' in files:
    files.remove('.DS_Store')

files.sort(key=lambda x: (int(x.split('_')[1]), x.split('_')[0], x.split('_')[2], x.split('_')[3], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))

ii = 1
jj = 1

for graph_name in files:
    
    if int(graph_name.split('_')[1]) < 17:
        continue
    if int(graph_name.split('_')[1]) == 17 and graph_name.split('_')[0] == 'layered' and int(graph_name.split('_')[-2]) == 2:
        continue
    if int(graph_name.split('_')[1]) > 17 and int(graph_name.split('_')[-2]) == 2:
        continue
    
    files2 = os.listdir(directory2)
    if '.DS_Store' in files2:
        files2.remove('.DS_Store')
    for file_name2 in files2:
        os.remove(directory2 + '/' + file_name2)
    shutil.copy(directory + '/' + graph_name, directory2 + '/' + graph_name)
    files2 = os.listdir(directory2)
    if '.DS_Store' in files2:
        files2.remove('.DS_Store')
    os.system('bash /Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/run_scip2.sh')
    time.sleep(1 + jj)
    ii += 1
    if ii == 72:
        ii = 0
        jj += 0.5

"""
directory = "/Users/maxbig/Coursework_multiprocessing/Coursework/LP/G"
# directory = "/Users/maxbig/ASVK/coursework/LP/old_tests_sp_buf_times/noP"
directory2 = '/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs'
# Получаем список файлов
files = os.listdir(directory)

if '.DS_Store' in files:
    files.remove('.DS_Store')
files2 = os.listdir(directory2)
files.sort()
ii = 0
for file_name in files:
    l1 = ['P2', 'P3', 'P4']
    # l2 = ['M2600', 'M4000', 'M10000']
    # l2 = ['M10000']
    # l2 = ['M8500', 'M11000', 'M13500']
    # l2 = ['M1050', 'M1350', 'M2550']
    # l2 = ['M2500', 'M3500', 'M6000']
    l2 = ['M1100', 'M1350', 'M2200']
    for i in l1:
        for j in l2:
            '''
            if ii >= 2 and i == 'P2' and j in ['M4000', 'M10000']:
                continue
            '''
            for file_name2 in files2:
                os.remove(directory2 + '/' + file_name2)
            shutil.copy(directory + '/' + file_name, directory2 + '/' + file_name[:-4] + f'_{i}_{j}.txt')
            # shutil.copy(directory + '/' + file_name, directory2 + '/' + file_name[:-4] + f'_p{i[1]}.txt')
            files2 = os.listdir(directory2)
            if '.DS_Store' in files2:
                files2.remove('.DS_Store')
            os.system('bash /Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/run_scip2.sh')
            time.sleep(2 + ii * 2)
    ii += 1
"""
