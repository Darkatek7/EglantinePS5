#!/usr/bin/env bash
# build.sh NAME: experimental core build (lenient port, scripted input ON) -> .local/core/candidate-NAME
set -euo pipefail
cd /home/heyde/Code/EglantinePS5-wt-core
export EDEN_BUILD_JOBS=${EDEN_BUILD_JOBS:-8} EDEN_PORT_LENIENT=1 EDEN_PORT_SKIPPED=$PWD/.local/core/skipped.txt EDEN_SKIP_SOURCE_CHECKS=1
bash tools/sync-eden-source.sh "$(cat .local/headless-cache)/source"
git -C Eglantine status --porcelain --untracked-files=all | awk '{print $2}' | while read -r f; do
    [[ -f "$(cat .local/headless-cache)/source/$f" ]] && touch "$(cat .local/headless-cache)/source/$f"
done
EDEN_SCRIPTED_INPUT=ON bash tools/build-package.sh release
out=.local/core/candidate-${1:-c1}; mkdir -p $out
cp build/release/PPSA98515/eboot.bin $out/eboot.bin
cp build/headless-native/eboot.elf $out/eboot.elf
cp build/headless-native/llvm-pie.elf $out/llvm-pie.elf 2>/dev/null || true
echo BUILD-DONE
