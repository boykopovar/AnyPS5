#!/bin/bash
set -u
dir=${ANYPS5_COMPILE_SLOT_DIR:?}
slots=${ANYPS5_COMPILE_SLOTS:?}
if [[ ! $slots =~ ^[1-9][0-9]*$ ]] || [[ $((slots)) != "$slots" ]]; then
  echo "compile_slot.sh: ANYPS5_COMPILE_SLOTS must be a positive decimal integer" >&2
  exit 2
fi
if (($# == 0)); then
  echo "compile_slot.sh: a compiler command is required" >&2
  exit 2
fi
command -v flock > /dev/null || { echo "compile_slot.sh: flock is required" >&2; exit 2; }
while :; do
  for ((i = 0; i < slots; i++)); do
    [ -e "$dir/slot-$i" ] || : >> "$dir/slot-$i" || { echo "compile_slot.sh: cannot create $dir/slot-$i" >&2; exit 2; }
    exec {fd}<"$dir/slot-$i" || { echo "compile_slot.sh: cannot open $dir/slot-$i" >&2; exit 2; }
    if flock -n "$fd"; then
      exec "$@"
    else
      status=$?
      if ((status != 1)); then
        echo "compile_slot.sh: flock failed on $dir/slot-$i (status $status)" >&2
        exit "$status"
      fi
    fi
    exec {fd}>&-
  done
  sleep 0.1
done
