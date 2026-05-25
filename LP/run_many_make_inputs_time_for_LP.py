import os
import shutil
import time

prev_p = 'P = 6'
prev_m = 'M = 968'

directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/mix22'

files = sorted(os.listdir(directory))
if '.DS_Store' in files:
    files.remove('.DS_Store')

for graph_name in files:
    '''
    if graph_name.split('_')[0] != 'layered':
        continue
    '''
    P, M = int(graph_name.split('_')[-2]), int(graph_name.split('_')[-1][:-4])
    
    f = open("/Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2_2.py", 'r')
    text = f.read()
    f.close()
    text = text.replace(prev_p, f'P = {P}')
    prev_p = f'P = {P}'
    text = text.replace(prev_m, f'M = {M}')
    prev_m = f'M = {M}'
    f = open("/Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2_2.py", 'w+')
    f.write(text)
    f.close()
    
    os.system('python /Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2_2.py -i ' + directory + '/' + graph_name + ' -o /Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/inputs/new_no_tr/order')
    os.system('python /Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2_2.py -i ' + directory + '/' + graph_name + ' -o /Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/inputs/new_tr/order -tr')
