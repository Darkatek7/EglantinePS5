# Compute shaders to RADV as NIR, without SPIR-V (phase 3 of 3)

The PS5 port links RADV, so a shader can reach the driver in the driver's own intermediate form
(NIR) instead of SPIR-V that RADV would parse back into NIR (`spirv_to_nir`). Phase 3 does this for
compute programs: shorter shader compiles (less stutter), the same GPU code.

## How

- `headless/nir_backend.cpp` translates the emulator IR of a compute program to NIR, mirroring
  what `spirv_to_nir` makes of the SPIR-V backend's output: same descriptor bindings (the unified
  binding order of `EmitContext`), same memory accesses (including the explicit workgroup layout's
  16-bit and 8-byte shared accesses), same float controls and per-instruction NaN/Inf/exactness
  flags, same loop shape and loop safety counters, the guest-warp-sized shuffle semantics.
- The NIR is attached to a `VkShaderModule` made from a SPIR-V header only (`vk_shader_module.nir`,
  as RADV's built-in shaders do); RADV hashes the serialized NIR for its caches. It is freed once
  the pipeline is created (`nir_handoff.h`, core patch 0015).
- A program using anything not translated (an opcode, an indirect buffer, a cube or arrayed
  texture...) keeps the SPIR-V backend. With `EDEN_NX2_COMPILE_TIMES=1` each such program logs
  `NX2_NIR fallback reason=... opcode=...`, which lists what to translate next.
- The backend is built against the pinned RADV's own Mesa headers and its Mesa references are
  renamed like the isolated RADV archive (`headless/nir_backend.cmake`, `tools/isolate-radv.py`).
- On by default; `EDEN_NX2_NIR=0` keeps SPIR-V for every program. Requires subgroup size control
  (the compute pipeline requires guest-sized subgroups).

## Measured

Donkey Kong Bananza, fresh shader cache, same build, `EDEN_NX2_COMPILE_TIMES=1` and
`EDEN_NX2_PIPELINE_STATS=1` (`NX2_COMPILE_TIME` / `NX2_PIPELINE_STATS` in `eden_log.txt`). The two
HMMA (tensor-core) compute shaders, the costliest of the game's compute shaders:

| Shader | SPIR-V emit + RADV | NIR emit + RADV | Change |
| --- | --- | --- | --- |
| 1 (945,633 SPIR-V words) | 8 + 2,098 ms | 82 + 1,391 ms | **-30%** |
| 2 (710,809 SPIR-V words) | 7 + 1,500 ms | 78 + 976 ms | **-30%** |

GPU code (driver statistics), SPIR-V / NIR: instructions 26,106 / 26,136 and 19,418 / 19,426;
VGPRs 104 / 104 and 100 / 100; LDS 12 / 12 KiB and 10 / 10 KiB; latency estimate +1.0% and +2.4%.
Gameplay renders as with SPIR-V (console screenshots), no crash.

The other 19 compute programs of the run still take SPIR-V: `GetCbufU32x2` (14), `FPRecip32` (4)
and `LoadStorage64` (1) are not translated yet.

## Reproduce

    # env.txt on the console, fresh shader cache
    EDEN_NX2_COMPILE_TIMES=1
    EDEN_NX2_PIPELINE_STATS=1
    EDEN_NX2_NIR=0              # SPIR-V for comparison (default: NIR)
