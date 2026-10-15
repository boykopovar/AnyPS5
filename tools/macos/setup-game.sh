#!/bin/bash
set -euo pipefail
if [ $# -lt 2 ]; then
    echo "usage: $0 <dump directory> <game directory> [module directory ...]" >&2
    echo "example: $0 ~/Downloads/PPSA28997 ~/Games/PPSA28997 Media/Modules Media/Plugins" >&2
    exit 2
fi
tools=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$tools/../.." && pwd)
build=${ANYPS5_BUILD:-$(cd "$repo/.." && pwd)/build-macos}
relinker="$build/core/relinker/relinker"
libs="$build/core/libs/libs"
[ -x "$relinker" ] || { echo "no relinker at $relinker: build first or set ANYPS5_BUILD" >&2; exit 1; }
[ -d "$libs" ] || { echo "no system libraries at $libs: build first or set ANYPS5_BUILD" >&2; exit 1; }
dump=$(cd "$1" && pwd)
mkdir -p "$2"
game=$(cd "$2" && pwd)
shift 2
directories=("$@")
input="$game/.relink-input"
mkdir -p "$input"

python3 -I "$tools/unself.py" "$dump/eboot.bin" "$input/eboot.elf"
for directory in sce_module "${directories[@]+"${directories[@]}"}"; do
    [ -d "$dump/$directory" ] || continue
    mkdir -p "$input/$directory"
    for module in "$dump/$directory"/*.prx "$dump/$directory"/*.sprx; do
        [ -f "$module" ] || continue
        python3 -I "$tools/unself.py" "$module" "$input/$directory/$(basename "$module")"
    done
done

arguments=(--macos --to-rosetta --registry)
for directory in "${directories[@]+"${directories[@]}"}"; do arguments+=(--module-dir "$directory"); done
(cd "$input" && "$relinker" "${arguments[@]}" eboot.elf "$game/eboot") > "$game/relink.log" 2>&1 || { tail -20 "$game/relink.log" >&2; exit 1; }

mkdir -p "$game/app0"
python3 -I "$tools/link_app0.py" "$dump" "$game/app0"
ln -sfn "$libs" "$game/libs"
for script in run.sh run-profile.sh; do
    [ -e "$game/$script" ] || cp "$tools/$script" "$game/$script"
done
echo "ready: $game/run.sh (profiled: $game/run-profile.sh)"
