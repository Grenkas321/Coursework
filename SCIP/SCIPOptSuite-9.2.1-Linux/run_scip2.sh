#!/bin/sh
set -x

SCIP_DIR="$HOME/Coursework_multiprocessing/Coursework/SCIP/SCIPOptSuite-9.2.1-Linux"
files=$(ls "$SCIP_DIR/translator_inputs"/*)

mkdir -p "$SCIP_DIR/outs/new_tr" "$SCIP_DIR/logs/new_tr"
mkdir -p "$SCIP_DIR/outs/new_no_tr" "$SCIP_DIR/logs/new_no_tr"

for file in $files; do
  job=$(basename "$file")
  job=${job%%.txt}
  
  # Используем именованный канал (FIFO)
  pipe=$(mktemp -u)
  mkfifo "$pipe"
  
  # Функция запуска
  run_scip() {
    variant="$1"
    type="$2"
    
    "$SCIP_DIR/bin/scip" -s "$SCIP_DIR/only_time.set" \
      -l "$SCIP_DIR/outs/${type}/${job}_${variant}_input.txt" \
      -f "$SCIP_DIR/inputs/${type}/order/${job}_${variant}_input.lp" \
      > "$SCIP_DIR/logs/${type}/${job}_${variant}_input.log" 2>&1
    
    # Отправляем сигнал о завершении в pipe
    echo "done" > "$pipe"
  }
  
  # Запускаем все в фоне
  run_scip "default" "new_tr" &
  run_scip "up_right" "new_tr" &
  run_scip "down_left" "new_tr" &
  run_scip "tiers" "new_tr" &
  run_scip "default" "new_no_tr" &
  run_scip "up_right" "new_no_tr" &
  run_scip "down_left" "new_no_tr" &
  run_scip "tiers" "new_no_tr" &
  
  # Ждем первого сообщения в pipe (блокирующее чтение)
  read signal < "$pipe"
  
  # Убиваем все SCIP процессы
  pkill -f "scip.*${job}" 2>/dev/null || true
  
  # Очистка
  rm -f "$pipe"
  
  # Короткая пауза для гарантии завершения
  sleep 0.01
  
  echo "Обработан файл: $job"
done
