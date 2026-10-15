#!/bin/sh
cd "$(dirname "$0")" || exit 1
mkdir -p logs
log="logs/profile-$(date +%Y%m%d-%H%M%S).log"
APS5_PROFILE_DRAW=1 APS5_PROFILE_GPU=1 ./run.sh 2>&1 | while IFS= read -r line; do printf '%s %s\n' "$(date +%H:%M:%S)" "$line"; done | tee "$log"
