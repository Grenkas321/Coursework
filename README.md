# Coursework_mod

Проект содержит C++-реализацию алгоритмов построения расписаний для направленного
ациклического графа работ при ограничении на число процессоров `P` и объем памяти
`M`. Основная цель экспериментов - сравнить качество расписаний и время работы
жадного алгоритма, имитации отжига, параллельной версии имитации отжига,
муравьиного алгоритма и алгоритма для последовательно-параллельных графов.

## Что реализовано

- `greedy` - базовый жадный алгоритм.
- `sao` - алгоритм имитации отжига.
- `csao` - параллельная/конвейерная версия имитации отжига.
- `aco` - муравьиный алгоритм.
- `sp`, `sp0` - алгоритмы для последовательно-параллельных графов.

Алгоритмы можно запускать с параметрами по умолчанию или через JSON-конфиги из
каталога `data/`.

## Структура проекта

```text
Coursework_mod/
  main.cpp                 # точка входа CLI-приложения
  CMakeLists.txt           # сборка C++-части
  README.md                # описание проекта и инструкции
  requirements.txt         # Python-зависимости для вспомогательных скриптов

  algorithms/              # реализации алгоритмов расписания
  additionals/             # чтение/генерация DAG, утилиты, таблицы, thread pool
  experiments/             # запуск серий экспериментов и сохранение результатов
  include/                 # общие типы, модели расписания и CLI-парсер
  src/                     # реализации общих структур и парсинга параметров
  json/                    # vendored nlohmann/json

  data/                    # готовые JSON-конфиги алгоритмов и примеры команд
  tools/                   # Python-скрипты для запуска и анализа экспериментов
  visualisation/           # построение диаграмм расписаний и памяти
  build/                   # локальная сборка и сгенерированные результаты
  SCIP/                    # внешние/экспериментальные материалы SCIP
```

Каталог `build/` считается рабочим: туда попадают бинарник, временные входы,
результаты запусков и графики. Его содержимое не нужно считать исходным кодом
проекта.

## Требования

C++-часть:

- CMake 3.22 или новее;
- компилятор с поддержкой C++20;
- Boost 1.70 или новее, компонент `program_options`;
- Linux/WSL рекомендуется для текущих экспериментальных скриптов.

Python-часть:

- Python 3.10+;
- зависимости из `requirements.txt`.

Установка Python-зависимостей:

```powershell
py -3 -m pip install -r requirements.txt
```

## Сборка

В WSL/Linux из корня проекта:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
```

В PowerShell можно вызвать те же команды через WSL:

```powershell
wsl bash -lc "cd /mnt/c/Users/tutor/MSU/Nauchka/single-proc-alg/Coursework_mod && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -- -j2"
```

Проверка CLI:

```bash
./build/application --help
```

## Формат входных данных

Программа принимает `--input` как путь к каталогу с `.txt`-файлами графов или
как путь к JSON-файлу с параметрами генерации синтетических графов.

Поддерживаются два текстовых формата графа.

Старый формат:

```text
node size children
0 5 1 2
1 3 3
2 4 3
3 2
```

Новый формат с буферами:

```text
prog_id prog_time weight_1: child_1 ... child_n, weight_2: child_1 ... child_n
0 10 4: 1 2, 3: 3
1 5 2: 3
2 6 1: 3
3 4
```

В новом формате каждая группа `weight: children` описывает отдельный буфер,
который создается текущей работой и используется указанными потомками.

JSON для генерации синтетических графов имеет вид:

```json
{
  "vertex_edge_map": {
    "20": 40,
    "50": 120
  },
  "weights": [1, 10],
  "prefix": "dag_",
  "seed": [42]
}
```

## Запуск приложения

Общий вид:

```bash
./build/application \
  --command <schedule|run|stability> \
  --algo <algorithm-or-json> \
  --input <graphs_dir_or_generator_json> \
  --output <output_dir> \
  --processors <P> \
  --memory <M> \
  --threads <N>
```

Основные опции:

- `--command schedule` - построить расписание и сохранить JSON расписания.
- `--command run` - провести серию запусков и сохранить таблицы, динамику и лучшие расписания.
- `--command stability` - проверить устойчивость по разным значениям числа повторов.
- `--algo` - короткое имя алгоритма (`greedy`, `sao`, `csao`, `aco`, `sp`, `sp0`) или путь к JSON-конфигу.
- `--samples` - сколько графов взять из входного набора; `0` означает все найденные графы.
- `--batch` - размер батча.
- `--dups` - число повторов для `run`; список чисел для `stability`.
- `--processors` - число процессоров `P`.
- `--memory` - жесткое ограничение памяти `M`.
- `--threads` - число рабочих потоков приложения.

Примеры:

```bash
./build/application --command schedule \
  --algo data/sao.json \
  --input Graphs \
  --output Answer \
  --processors 4 \
  --memory 10000 \
  --samples 1 \
  --batch 1
```

```bash
./build/application --command run \
  --algo greedy \
  --algo data/aco_best.json \
  --algo data/sao.json \
  --input Graphs \
  --output Answer \
  --processors 4 \
  --memory 10000 \
  --threads 4 \
  --dups 10 \
  --samples 10 \
  --batch 5
```

```bash
./build/application --command stability \
  --algo data/sao.json \
  --input Graphs \
  --output Stability \
  --processors 4 \
  --memory 10000 \
  --threads 4 \
  --dups 1 3 5 10 \
  --samples 10 \
  --batch 5
```

## Конфиги алгоритмов

Готовые конфиги лежат в `data/`:

- `data/sao.json` - параметры имитации отжига.
- `data/csao.json` - параметры параллельной версии имитации отжига.
- `data/aco.json` - базовый муравьиный алгоритм.
- `data/aco_best.json` - настроенный муравьиный алгоритм.
- `data/aco_heavy.json`, `data/aco_tpe_hard.json` - дополнительные варианты ACO.

Каждый JSON содержит верхний ключ алгоритма (`sao`, `csao`, `aco`) и набор
параметров. Поле `label` определяет имя подкаталога в результатах.

## Выходные данные

Для `run` приложение создает структуру вида:

```text
<output_dir>/
  <algorithm_label>/
    table_data/                 # CSV-таблицы по батчам
    schedules/best/             # лучшие найденные расписания в JSON
    dynamics/best/              # динамика целевой функции для итерационных алгоритмов
    temp_dynamics/best/         # динамика температуры для SAO
    prob_dynamics/best/         # динамика вероятностей для SAO
    lambda_dynamics/best/       # дополнительная динамика SAO
    fedorenko_diagnostics/best/ # диагностика правила Федоренко
    conveyor/best/              # данные конвейера для CSAO
```

В JSON расписания сохраняются значения целевой функции, длительность расписания,
время выполнения и назначение работ на процессоры.

## Скрипты экспериментов

Основные скрипты в `tools/`:

- `tools/tune_aco.py` - подбор параметров ACO.
- `tools/plot_aco_tuning.py` - графики по результатам подбора ACO.
- `tools/run_coursework_comparison.py` - запуск фиксированной сетки сравнений для курсовой.
- `tools/run_scaling_grid_comparison.py` - запуск масштабируемой сетки по графам, `P` и `M`.
- `tools/plot_coursework_comparison.py` - SVG-графики по `results_long.csv`.

Пример запуска масштабируемой сетки:

```powershell
py -3 tools\run_scaling_grid_comparison.py `
  --build-dir build `
  --graphs-dir build\Graphs `
  --workers 4 `
  --app-threads 1 `
  --processors 2,4,10 `
  --memory-divisors n,20,10,5,2,1 `
  --resume
```

Построение графиков по CSV:

```powershell
py -3 tools\plot_coursework_comparison.py `
  --results build\tmp_coursework_comparison\results_long.csv `
  --out-dir build\tmp_coursework_comparison\plots
```

Скрипты в `visualisation/` строят диаграммы Ганта и графики использования памяти
по JSON-расписаниям, полученным из `schedules/best/`.

## Рекомендуемый рабочий процесс

1. Собрать `application`.
2. Подготовить каталог с графами или JSON генератора.
3. Проверить один короткий запуск через `--command schedule`.
4. Запустить серию через `--command run` или один из скриптов `tools/`.
5. Проверить CSV/JSON в выходном каталоге.
6. Построить графики и диаграммы вспомогательными Python-скриптами.

## Замечания по чистоте репозитория

- Не хранить результаты экспериментов в исходных каталогах `algorithms/`, `src/`, `include/`.
- Временные результаты лучше складывать в `build/`, `Answer/`, `Graphs/` или в каталог с префиксом `tmp_`.
- Перед фиксацией изменений полезно проверить `git status --short`: в проекте могут быть локальные экспериментальные правки алгоритмов и визуализаций.
