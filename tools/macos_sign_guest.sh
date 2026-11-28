#!/bin/bash
set -euo pipefail
if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: tools/macos_sign_guest.sh executable [library-directory]" >&2
    exit 2
fi
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
entitlements=$root/tools/macos_hardened_runtime.entitlements
identity=${CODESIGN_IDENTITY:--}
codesign --force --sign "$identity" --options runtime --timestamp=none --entitlements "$entitlements" "$1"
if [[ $# -eq 2 ]]; then
    find "$2" -type f \( -name '*.prx' -o -name '*.dylib' \) -print0 |
        while IFS= read -r -d '' library; do
            codesign --force --sign "$identity" --timestamp=none "$library"
        done
fi
codesign --display --verbose=2 --entitlements - "$1"
