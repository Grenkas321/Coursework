import matplotlib
import matplotlib.pyplot as plt
import math
'''
gaps_2_15 = [9.1, 9.3, 9.3, 11.1]
gaps_2_17 = [4.8, 5.1, 22.7, 25.89]
gaps_2_20 = [18.6, 19.6, 133.0, 135.51]
gaps_2 = [gaps_2_15, gaps_2_17, gaps_2_20]
gaps_3_15 = [7.3, 7.3, 7.4, 7.4]
gaps_3_17 = [14.0, 14.2, 14.6, 14.91]
gaps_3_20 = [27.1, 27.2, 27.6, 28.37]
gaps_3 = [gaps_3_15, gaps_3_17, gaps_3_20]
gaps_4_15 = [2.6, 2.9, 3.2, 3.25]
gaps_4_17 = [8.7, 8.7, 9.3, 9.34]
gaps_4_20 = [10.8, 10.9, 11.2, 11.35]
gaps_4 = [gaps_4_15, gaps_4_17, gaps_4_20]

x_vals = [15, 17, 20]
'''

sp_20_buf_time_p2 = [14.2, 14.5, 15.2, 16.43]
sp_20_buf_time_p3 = [15.8, 16.0, 16.5, 16.58]
sp_20_buf_time_p4 = [16.7, 16.8, 18.0, 18.91]
sp_21_buf_time_p2 = [18.6, 18.7, 19.3, 20.2]
sp_21_buf_time_p3 = [22.0, 22.0, 23.3, 23.78]
sp_21_buf_time_p4 = [22.5, 22.5, 22.5, 23.87]
sp_22_buf_time_p2 = [50.3, 51.3, 51.6, 53.4]
sp_22_buf_time_p3 = [46.0, 46.0, 46.0, 46.7]
sp_22_buf_time_p4 = [30.8, 31.0, 32.3, 32.34]
sp_23_buf_time_p2 = [25.8, 26.2, 28.1, 33.16]
sp_23_buf_time_p3 = [35.1, 35.1, 35.5, 36.69]
sp_23_buf_time_p4 = [31.2, 31.2, 31.3, 33.44]
sp_24_buf_time_p2 = [26.0, 27.4, 29.8, 59.93]
sp_24_buf_time_p3 = [41.9, 42.0, 42.0, 44.31]
sp_24_buf_time_p4 = [42.0, 44.1, 45.8, 47.8]
sp_25_buf_time_p2 = [33.0, 37.5, 45.7, 154.32]
sp_25_buf_time_p3 = [57.8, 58.0, 58.9, 60.57]
sp_25_buf_time_p4 = [51.3, 51.3, 51.7, 56.37]
sp_26_buf_time_p2 = [85.5, 86.1, 122.0, 7958.51]
sp_26_buf_time_p3 = [84.6, 91.8, 95.3, 97.8]
sp_26_buf_time_p4 = [79.0, 81.1, 83.1, 84.73]
sp_27_buf_time_p2 = [159.0, 179.0]
sp_27_buf_time_p3 = [72.8, 72.8, 73.6, 79.68]
sp_27_buf_time_p4 = [76.5, 77.5, 77.7, 79.28]
sp_28_buf_time_p3 = [117.0, 120.0, 122.0, 126.7]
sp_28_buf_time_p4 = [76.5, 76.5, 76.5, 76.93]
sp_29_buf_time_p3 = [99.1, 102.0, 104.0, 111.67]
sp_29_buf_time_p4 = [76.7, 77.5, 78.4, 80.61]
sp_30_buf_time_p3 = [101.0, 116.0, 119.0, 125.01]
sp_30_buf_time_p4 = [86.2, 86.2, 86.5, 92.9]
sp_31_buf_time_p3 = [104.0, 107.0, 109.0, 123.46]
sp_31_buf_time_p4 = [104.0, 105.0, 106.0, 108.53]
sp_32_buf_time_p3 = [134.0, 142.0, 147.0, 167.33]
sp_32_buf_time_p4 = [89.7, 98.8, 103.0, 110.75]
sp_33_buf_time_p3 = [146.0, 151.0, 152.0, 201.31]
sp_33_buf_time_p4 = [139.0, 142.0, 145.0, 151.46]
sp_34_buf_time_p3 = [243.0, 291.0, 306.0, 379.36]
sp_34_buf_time_p4 = [131.0, 136.0, 143.0, 160.55]
sp_35_buf_time_p3 = [117.0, 125.0, 127.0, 205.93]
sp_35_buf_time_p4 = [164.0, 178.0, 191.0, 205.03]
sp_36_buf_time_p3 = [5626.0, 5718.0, 5787.0]
sp_36_buf_time_p4 = [171.0, 187.0, 199.0, 236.32]
sp_37_buf_time_p4 = [168.0, 1056.0, 1078.0, 2762.44]
p2 = [sp_20_buf_time_p2, sp_21_buf_time_p2, sp_22_buf_time_p2, sp_23_buf_time_p2, sp_24_buf_time_p2, sp_25_buf_time_p2, sp_26_buf_time_p2, sp_27_buf_time_p2]
p3 = [sp_20_buf_time_p3, sp_21_buf_time_p3, sp_22_buf_time_p3, sp_23_buf_time_p3, sp_24_buf_time_p3, sp_25_buf_time_p3, sp_26_buf_time_p3, sp_27_buf_time_p3, sp_28_buf_time_p3, sp_29_buf_time_p3, sp_30_buf_time_p3, sp_31_buf_time_p3, sp_32_buf_time_p3, sp_33_buf_time_p3, sp_34_buf_time_p3, sp_35_buf_time_p3, sp_36_buf_time_p3]
p4 = [sp_20_buf_time_p4, sp_21_buf_time_p4, sp_22_buf_time_p4, sp_23_buf_time_p4, sp_24_buf_time_p4, sp_25_buf_time_p4, sp_26_buf_time_p4, sp_27_buf_time_p4, sp_28_buf_time_p4, sp_29_buf_time_p4, sp_30_buf_time_p4, sp_31_buf_time_p4, sp_32_buf_time_p4, sp_33_buf_time_p4, sp_34_buf_time_p4, sp_35_buf_time_p4, sp_36_buf_time_p4, sp_37_buf_time_p4]

x_vals1 = [i for i in range(20, 36)]
x_vals2 = [i for i in range(20, 37)]
x_vals3 = [i for i in range(20, 38)]
x_vals4 = [i for i in range(20, 28)]
x_vals5 = [i for i in range(20, 27)]

y_vals_20 = []
y_vals_15 = []
y_vals_10 = []
y_vals_0 = []
for i in range(len(p2)):
    y_vals_20.append(p2[i][0])
    y_vals_15.append(p2[i][1])
    if len(p2[i]) >= 3:
        y_vals_10.append(p2[i][2])
    if len(p2[i]) == 4:
        y_vals_0.append(p2[i][3])

# задаем размеры
plt.figure(figsize=(10,4))

# заголовок
plt.title('График зависимости времени выполнения от числа вершин в SP-графах для различной точности для P = 2', fontsize=10)

# рисуем графики
plt.plot(x_vals4, y_vals_20, marker='o', label='20%')
plt.plot(x_vals4, y_vals_15, marker='o', label='15%')
plt.plot(x_vals5, y_vals_10, marker='o', label='10%')
plt.plot(x_vals5, y_vals_0, marker='o', label='exact')

# x_vals - значения по оси x
# y_vals - значения по оси y
# linestyle: '--' - dashed line style, '-.'- dash-dot line style, ...
# color: r, g, b, ...
# marker: '.', 'o', '+', ...

# сетка графика
plt.grid(True)

# подписи осей
plt.ylabel('время работы, с',  fontsize=10)
plt.xlabel('число вершин в графе',  fontsize=10)

plt.yscale('log')

x_ticks = [i for i in range(20, 28)]  # значения 10^1 и 10^4
x_labels = list(map(str, x_ticks))  # подписи в красивом формате
plt.xticks(x_ticks, x_labels)

y_ticks = [10**1, 10**2, 10**3, 10**4]  # значения 10^1 и 10^4
y_labels = ['$10^1$', '$10^2$', '$10^3$', '$10^4$']  # подписи в красивом формате
plt.yticks(y_ticks, y_labels)

# легенда
plt.legend(fontsize=10)

plt.show()
