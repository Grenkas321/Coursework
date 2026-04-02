import os
import shutil


directory2 = "/Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs"
directory = '/Users/maxbig/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux/translator_inputs'
# Получаем список файлов
files = os.listdir(directory)

if '.DS_Store' in files:
    files.remove('.DS_Store')
files2 = os.listdir(directory2)
files.sort()
ii = 0
for file_name in files:
    for file_name2 in files2:
        os.remove(directory2 + '/' + file_name2)
    shutil.copy(directory + '/' + file_name, directory2 + '/' + file_name)
    files2 = os.listdir(directory2)
    if '.DS_Store' in files2:
        files2.remove('.DS_Store')
    p, m = (int(file_name.split('_P')[1][0]), int(file_name.split('_M')[1].split('.')[0]))
    os.system(f'/Users/maxbig/Coursework_multiprocessing/Coursework/build/application --command run --algo greedy --input /Users/maxbig/Coursework_multiprocessing/Coursework/build/Graphs --output /Users/maxbig/Coursework_multiprocessing/Coursework/build/Answer --threads 1 --dups 1 --sample 1 --batch 1 --processors {p} --memory {m}')

