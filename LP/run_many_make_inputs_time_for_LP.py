import os
import shutil
import time

# directory = "/Users/maxbig/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs/"
l1 = ['2', '3', '4']
# l2 = ['2600', '4000', '10000']
# l2 = ['8500', '11000', '13500']
# l2 = ['10000']
# l2 = ['1050', '1350', '2550']
# l2 = ['2500', '3500', '6000']
l2 = ['1100', '1350', '2200']
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

'''
for i in l1:
    for j in l2:
        f = open("/Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2.py", 'r')
        text = f.read()
        f.close()
        text = text.replace(prev_p, f'P = {i}')
        prev_p = f'P = {i}'
        text = text.replace(prev_m, f'M = {j}')
        prev_m = f'M = {j}'
        f = open("/Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2.py", 'w+')
        f.write(text)
        f.close()

        # directory = f"/Users/maxbig/Coursework_multiprocessing/Coursework/LP/P{i}_M{j}/"
        

        files = sorted(os.listdir(directory))

        if '.DS_Store' in files:
            files.remove('.DS_Store')

        for graph_name in files:
            os.system('python /Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2.py -i ' + directory + graph_name + ' -o /Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/inputs/new_no_tr/order')
            os.system('python /Users/maxbig/Coursework_multiprocessing/Coursework/LP/make_input_times_step2.py -i ' + directory + graph_name + ' -o /Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/inputs/new_tr/order -tr')
'''
