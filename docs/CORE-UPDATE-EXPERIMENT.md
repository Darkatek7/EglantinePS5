# EglantinePS5: core update experiment (Eglantine upstream babdb6bb75)

2026-11-11. Worktree `~/Code/EglantinePS5-wt-core`, branch `exp/core-update` (local, not pushed). The submodule is on branch `exp/core-update-patches`. Nothing is installed: every console run went through the runner, which put main's release back afterwards.

## Summary

- **It builds and runs Zelda.** One run reached gameplay on Link's porch at **~29.5 FPS** (HUD "F30"), against **~20.5 FPS on main** (cec2d7e, same benchmark). That is the game's 30 FPS cap.
- **It is not stable.** 3 of the 4 runs on the final settings froze at the first scene load (~50 s in). Memory is ~1 GB tighter than on main, and the GPU threads end up blocked: a deadlock on the CPU side, not a GPU hang.
- Getting to this point took 5 fixes for upstream changes that break the PS5 (listed below). The most important: two upstream texture paths keep textures as RGBA8 at 4x the memory.

## What the branch contains

- **Core:** upstream Eglantine nx2 **babdb6bb75** (1,175 commits after our pin 3140648a66).
- **Ported patches:**
  - applied as-is: 0016, 0021, 0030, 0034, 0055, 0057;
  - adjusted: 0031, 0015 (NIR interface only), 0024 (texture cache ceiling).
- **Not ported:** the other ~65 patches. Upstream already covers most of what they fixed, but differently. Missing in particular:
  - the Zelda series 0054/0068/0070–0078;
  - the MKW memory work 0004/0010–0012/0023/0058/0059.
- **Dynarmic** stays at the old pin; upstream's JIT needs a new PS5 port.
- **PS5 layer:**
  - built in "lenient" mode;
  - 55 Python and 11 CMake port edits no longer match upstream and were skipped;
  - the multiplayer sockets use upstream's code.
  - Lists: `.local/core/skipped.txt*`.

## Fixes needed on top of upstream (all on the experiment branches)

1. **Fiber stacks.** Upstream rewrote `fiber.cpp`, so our PS5 stack edit silently missed. Each guest thread held a 2 MiB heap block: **330 MiB** for Zelda's 165 threads. Fixed in `headless/CMakeLists.txt`, which now also fails loudly if this edit misses.
2. **Turing macro JIT (new upstream, on by default).** The console can't give it executable memory: crash "Xbyak can't alloc" 29 s in. Now off by default, as on main (`EDEN_NX2_MME_JIT=1` turns it on).
3. **Uneven-mip ASTC kept as RGBA8 on AMD** (upstream #679, "not run on AMD hardware"). On the PS5's RADV almost every texture qualifies, so 4.7 GB of images and OOM at 55–90 s. Now off by default (`EDEN_NX2_UNEVEN_MIP_RGBA8=1` turns it on).
4. **ASTC→BC transcode scratch.** Upstream keeps one RGBA8 image per texture shape for reuse, with no limit (up to 192 MiB each, 3 GB during the load). Ported main's cap: **64 MiB** in flight, flush every 32 MiB.
5. **Texture cache ceiling (0024).** 3584/4096 MiB instead of upstream's 4352/5120.

Also kept: the 8 MiB cap on read-only native compute storage reads (upstream #1231 allows up to 1 GiB). It turned out not to be the OOM cause, but it matches main.

## Console runs (Zelda, save loads on Link's porch, 1x, 1080p)

| Run | Build | Result |
|---|---|---|
| zcore1, zc5, zc7–zc10 | u4–u10 | Out of device memory at 55–90 s (`BufferCache::CreateBuffer`) |
| zc6 | u6 | Macro JIT "can't alloc" at 29 s |
| zc11 | u11 | Heap exhausted at 57 s |
| zc12, zc12t | u12 (256 MiB cap) | Survived, but thrashing: 0.3–1.5 FPS |
| **zc12p** | u12 + 64 MiB cap + 0024 limits | **Gameplay, stable for 80 s: 28–30 FPS** |
| zr1, zr2, zr3 | u13 (same settings built in) | Froze at the scene load (~50 s) |

**Measured, zc12p vs main** (main = runs znl_1 and zl949_1 of cec2d7e's series):

| | Experiment | Main |
|---|---|---|
| FPS (fps.py, last 39 s) | **29.1** | 20.7 / 20.1 |
| Presented FPS (5 s windows) | steady 28.2–30.0 | 19.7–21.1 |
| HUD | F30 | F21 |
| GPU driver memory | 3.75 GB | ~3.9 GB |
| Game memory | 4.5 GB | ~5.0 GB |
| Eden heap | 1.28 GB | 1.45 GB |
| Pool free | **~0.7 GB** | ~1.2 GB |

The image looks right (lit scene, foliage). One visible difference: a ~64 px strip on the left edge with stale content.

## The freeze (unresolved)

- After the load spike, GPU driver memory briefly peaks at 4.3–4.8 GB, then the GPU threads stop.
- CPU sampling 70 s in shows no GPU thread running (blocked), idle guest cores, and only the memory compressor busy.
- The kernel log shows the GPU's queue empty, so the GPU itself is fine.
- The likely cause is a CPU-side wait under memory pressure. Suspects: the transcode cap's `scheduler.Wait`, or skipped PS5 edits for GPU completion wait/signal/shutdown, `HandleRasterizerWrite`, `DmaPusher`.
- Next step would be dumping the blocked threads' stacks (ps5debug).

## Takeaways

- Upstream's CPU-side work alone gives about **+45% on Zelda** (it reaches the 30 FPS cap). That is more than all our recent CPU-side patches together.
- A real move to upstream needs:
  - a new PS5 port of dynarmic;
  - the 66 skipped port edits redone;
  - the Zelda and MKW memory work redone on upstream's structures;
  - upstream's AMD and texture defaults checked against the PS5.
- Estimated at several days, not hours.

Builds: `.local/core/candidate-u12` (zc12p) and `candidate-u13`. Rebuild with `.local/core/build.sh NAME`. Trail: `.local/core/STATUS.md`.
