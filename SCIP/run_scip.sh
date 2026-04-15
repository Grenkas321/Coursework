#!/bin/sh
set -x

SCIP_DIR="$HOME/SCIP/SCIPOptSuite-9.2.1-Linux"
files=$(ls "$SCIP_DIR/translator_inputs"/*)

# Создаем каталоги
mkdir -p "$SCIP_DIR/outs/new_tr" "$SCIP_DIR/logs/new_tr"
mkdir -p "$SCIP_DIR/outs/new_no_tr" "$SCIP_DIR/logs/new_no_tr"

for file in $files; do
  job=$(basename "$file")
  job=${job%%.txt}
  
  # Сохраняем PID всех запущенных процессов
  pids=""
  
  # Функция для запуска и сохранения PID
  launch_scip() {
    local variant="$1"
    local type="$2"  # "new_tr" или "new_no_tr"
    
    "$SCIP_DIR/bin/scip" -s "$SCIP_DIR/only_time.set" \
      -l "$SCIP_DIR/outs/${type}/${job}_${variant}_input.txt" \
      -f "$SCIP_DIR/inputs/${type}/order/${job}_${variant}_input.lp" \
      > "$SCIP_DIR/logs/${type}/${job}_${variant}_input.log" 2>&1 &
    
    pids="$pids $!"  # Сохраняем PID последнего запущенного процесса
  }
  
  # Запускаем все процессы
  launch_scip "default" "new_tr"
  launch_scip "up_right" "new_tr"
  launch_scip "down_left" "new_tr"
  launch_scip "tiers" "new_tr"
  launch_scip "reverse_tiers" "new_tr"
  launch_scip "default" "new_no_tr"
  launch_scip "up_right" "new_no_tr"
  launch_scip "down_left" "new_no_tr"
  launch_scip "tiers" "new_no_tr"
  launch_scip "reverse_tiers" "new_no_tr"
  
  # Ждем завершения всех процессов (альтернатива wait -n)
  # В sh нет wait -n, поэтому ждем все
  wait
  
  echo "Обработан файл: $job"
  
  # Небольшая пауза между файлами
  sleep 1
done
