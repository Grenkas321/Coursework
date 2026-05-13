import os
import shutil
import subprocess

directory2 = "/Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs"

dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_layered_time_P_and_M',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_random_time_P_and_M',
        '/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/decision_quality/new_triadags_time_P_and_M']

dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/scalability/new_triadags_time_P_and_M'] + dirs

# dirs = ['/Users/maxbig/Coursework_multiprocessing/Coursework/LP/experiments/data_lp/mix2']

algo = 'greedy2'

for directory in dirs:
    files = os.listdir(directory)

    if '.DS_Store' in files:
        files.remove('.DS_Store')

    files.sort(key=lambda x: (int(x.split('_')[1]), x.split('_')[0], x.split('_')[2], x.split('_')[3], int(x.split('_')[-2]), int(x.split('_')[-1][:-4])))
    dct0 = {}
    ii = 0
    for file_name in files:
        # P, M = int(file_name.split('_')[-2]), int(file_name.split('_')[-1][:-4])
        P, M = int(file_name.split('_')[-2]), int(file_name.split('_')[-1][:-4])
        # triadag10_55_1_11_485
        '''
        ii += 1
        if ii == 37:
            break
        '''
        if int(file_name.split('_')[1]) > 1000:
            continue
        # print(file_name)
        
        files2 = os.listdir(directory2)
        for file_name2 in files2:
            os.remove(directory2 + '/' + file_name2)
        shutil.copy(directory + '/' + file_name, directory2 + '/' + file_name)
        files2 = os.listdir(directory2)
        if '.DS_Store' in files2:
            files2.remove('.DS_Store')
        flag0 = 0
        for proc in [1]:
            try:
                output = subprocess.check_output('/Users/maxbig/Coursework_multiprocessing/Coursework/build/application ' \
                f'--command run --algo {algo} --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs ' \
                '--output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 ' \
                f'--batch 1 --processors {P} --memory {int(M * proc)}', shell=True, text=True)
                flag0 = 1
            except Exception:
                pass
            if flag0:
                break
        if flag0 != 1:
            print(file_name, '!!!')
            print()
            print()
        """
        output = subprocess.check_output('/Users/maxbig/Coursework_multiprocessing/Coursework/build/application ' \
        '--command run --algo greedy2 --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs ' \
        '--output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 ' \
        f'--batch 1 --processors {P} --memory {M}', shell=True, text=True)
        """
