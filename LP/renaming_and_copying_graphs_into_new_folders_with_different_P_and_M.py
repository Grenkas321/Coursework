import os
import time
import shutil


directory = "/Users/maxbig/Coursework_multiprocessing/Coursework/LP/G"
files = os.listdir(directory)
if '.DS_Store' in files:
    files.remove('.DS_Store')
l1 = ['P2', 'P3', 'P4']
# l2 = ['M2600', 'M4000', 'M10000']
# l2 = ['M8500', 'M11000', 'M13500']
# l2 = ['M1050', 'M1350', 'M2550']
# l2 = ['M2500', 'M3500', 'M6000']
l2 = ['M1100', 'M1350', 'M2200']
for i in l1:
    for j in l2:
        directory2 = f"/Users/maxbig/Coursework_multiprocessing/Coursework/LP/{i}_{j}"
        os.system(f'mkdir {directory2}')
        files2 = os.listdir(directory2)
        for file_name2 in files2:
            os.remove(directory2 + '/' + file_name2)
        for file_name in files:
            shutil.copy(directory + '/' + file_name, directory2 + '/' + file_name[:-4] + f'_{i}_{j}.txt')
