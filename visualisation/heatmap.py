import matplotlib.pyplot as plt
import numpy as np

def parse_data(data_str):
    """Преобразует многострочную строку в матрицу чисел 4x9.
    Заменяет '?' на np.nan."""
    lines = data_str.strip().split('\n')
    # Ожидаем 4 строки для размеров 55, 210, 496, 990
    matrix = []
    for line in lines:
        parts = line.strip().split()
        row = []
        for p in parts:
            if p == '?':
                row.append(np.nan)
            else:
                row.append(float(p))
        matrix.append(row)
    return np.array(matrix)

def plot_heatmap(data_matrix, size_labels, param_labels, title="Тепловая карта Δ для треугольных графов, тип буферов N_uniform"):
    mx, mn = max([elem for elem in data_matrix[~np.isnan(data_matrix)]]), -min([elem for elem in data_matrix[~np.isnan(data_matrix)]])
    fig, ax = plt.subplots(figsize=(12, 6))
    # Создаём тепловую карту
    im = ax.imshow(data_matrix, cmap='RdYlGn', aspect='auto', vmin=-max(mn, mx), vmax=max(mn, mx))
    # Настройка осей
    ax.set_xticks(np.arange(len(param_labels)))
    ax.set_yticks(np.arange(len(size_labels)))
    ax.set_xticklabels(param_labels, rotation=45, ha='right', fontsize=10)
    ax.set_yticklabels(size_labels, fontsize=10)
    ax.set_xlabel("Параметры (P, M)", fontsize=12)
    ax.set_ylabel("Размер графа (число вершин)", fontsize=12)
    ax.set_title(title, fontsize=14)
    # Добавляем цветовую шкалу
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label("Δ, %", fontsize=10)
    # Отображаем значения в ячейках (кроме NaN)
    for i in range(len(size_labels)):
        for j in range(len(param_labels)):
            val = data_matrix[i, j]
            if not np.isnan(val):
                text_color = 'black' if abs(val) < max(mn, mx) / 2 else 'white'
                ax.text(j, i, f"{val:.1f}", ha='center', va='center', color=text_color, fontsize=8)
    plt.tight_layout()
    plt.show()

def main():
    # Входные данные (пример из условия)
    data_str = """
2.63 1.59 1.59 14.26 1.91 1.91 15.3 -1.4 -2.12
4.2 2.7 3.42 13.25 1.33 0.33 18.15 -0.86 0.0
3.96 2.21 2.21 19.95 10.37 5.63 28.78 11.58 0.0
10.48 7.42 7.42 22.6 9.91 9.0 29.99 2.98 0.0
    """
    # Размеры графов (по порядку строк)
    size_labels = [55, 210, 496, 990]
    # Метки для параметров P и M (9 комбинаций)
    param_labels = [
        "малые P и M", "малое P,\nсредний M", "малое P,\nбольшой M",
        "среднее P,\nмалый M", "средние P и M", "среднее P,\nбольшой M",
        "большое P,\nнизкий M", "большое P,\nсредний M", "большие P и M"
    ]
    # Парсим данные
    data_matrix = parse_data(data_str)
    # Строим тепловую карту
    plot_heatmap(data_matrix, size_labels, param_labels)

if __name__ == "__main__":
    main()
