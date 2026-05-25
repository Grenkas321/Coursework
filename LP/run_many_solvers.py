import os
import shutil
import time


directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/mix22'
directory2 = '/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs'

files = sorted(os.listdir(directory))
if '.DS_Store' in files:
    files.remove('.DS_Store')

files.sort(key=lambda x: (int(x.split('_')[1]), x.split('_')[0], x.split('_')[2], x.split('_')[3], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))

ii = 1
jj = 10

for graph_name in files:
    
    if int(graph_name.split('_')[1]) < 16:
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

