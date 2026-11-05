#!/bin/bash
# Clean build artifacts and caches after PR work
# Run this after each PR merge/close

set -euo pipefail

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT"

echo "=== Cleaning build cache ==="

# 1. Remove CMake object files, keep configure
find build -type f \( -name "*.o" -o -name "*.obj" \) -delete 2>/dev/null
echo "Removed object files"

# 2. Remove CMake cache except CMakeCache.txt (reconfigure preserves it)
find build -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null
echo "Removed CMakeFiles dirs"

# 3. Remove Python caches
find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null
find . -name ".pytest_cache" -type d -exec rm -rf {} + 2>/dev/null
echo "Removed Python caches"

# 4. Remove temporary test artifacts
rm -rf /tmp/opencode/anyps5-* 2>/dev/null || true
echo "Removed temp watcher files"

# 5. Report remaining size
echo ""
echo "=== Remaining sizes ==="
du -sh build/ 2>/dev/null || echo "build: not found"
du -sh 3rdparty/ 2>/dev/null || echo "3rdparty: not found"
df -h . | tail -1