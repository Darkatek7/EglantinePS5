#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the production NIR integer flags using the pinned Mesa's host libraries.

MESA_HOST_BUILD may name a matching host Mesa build. Defaults to the RADV CLC host build.
No Mesa source or build is modified by this check.
"""
import ctypes.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get('MESA_HOST_BUILD', root.parent / 'mihawk-vulkan-review/.deps/work/radv-clc-build')).resolve()
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(c for c in commands if c['file'].endswith('/nir/nir.c'))
args = shlex.split(entry['command'])
includes = [a for a in args if a.startswith(('-I', '-D'))]
archives = [build / p for p in ('src/compiler/nir/libnir.a', 'src/compiler/libcompiler.a',
    'src/util/libmesa_util.a', 'src/util/libmesa_util_simd.a',
    'src/util/libmesa_util_clflush.a', 'src/util/libmesa_util_clflushopt.a',
    'src/util/blake3/libblake3.a', 'src/c11/impl/libmesa_util_c11.a')]
with tempfile.TemporaryDirectory(prefix='eden-nir-ops-') as tmp:
    for source in ('check-nir-integer-flags.cpp', 'check-nir-compute-ops.cpp'):
        exe = Path(tmp) / Path(source).stem
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1',
            '-Wno-macro-redefined', *includes, '-I' + str(root / 'headless'),
            str(root / 'tools' / source), '-o', str(exe),
            '-Wl,--start-group', *map(str, archives), '-Wl,--end-group',
            '-l:' + (ctypes.util.find_library('zstd') or 'libzstd.so'), '-lz', '-ldl', '-pthread', '-lm'],
            cwd=build, check=True)
        subprocess.run([str(exe)], check=True, timeout=60)
