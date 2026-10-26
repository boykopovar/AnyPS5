#!/bin/bash
set -u
dir=${ANYPS5_COMPILE_SLOT_DIR:?}
slots=${ANYPS5_COMPILE_SLOTS:?}
while :; do
  for ((i = 0; i < slots; i++)); do
    [ -e "$dir/slot-$i" ] || : >> "$dir/slot-$i"
    exec {fd}<"$dir/slot-$i"
    if flock -n "$fd"; then
      exec "$@"
    fi
    exec {fd}>&-
  done
  sleep 0.1
done
