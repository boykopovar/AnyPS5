#!/bin/bash
set -u
dir=${ANYPS5_COMPILE_SLOT_DIR:?}
slots=${ANYPS5_COMPILE_SLOTS:?}
command -v flock > /dev/null || { echo "compile_slot.sh: flock is required" >&2; exit 2; }
while :; do
  for ((i = 0; i < slots; i++)); do
    [ -e "$dir/slot-$i" ] || : >> "$dir/slot-$i" || { echo "compile_slot.sh: cannot create $dir/slot-$i" >&2; exit 2; }
    exec {fd}<"$dir/slot-$i" || { echo "compile_slot.sh: cannot open $dir/slot-$i" >&2; exit 2; }
    if flock -n "$fd"; then
      exec "$@"
    fi
    exec {fd}>&-
  done
  sleep 0.1
done
