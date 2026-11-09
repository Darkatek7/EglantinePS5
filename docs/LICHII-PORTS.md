# Lichii ports (November 2026)

Changes taken from Lichii (zortos293/Lichii `nx2`) into the EglantinePS5 core as patches. Lichii is
about 1,100 commits past our core pin (Eglantine 3140648a), so these are ports, not cherry-picks.
Console numbers: PS5, same save and scene, warm caches, interleaved runs
(`tools/bananza-bench-console.py`, `tools/mkw-bench-console.py`).

## Upload skips (core 0061, 0062)

- **0061, join copy covers the image** (Lichii #817). When `JoinImages` makes a new image that one of
  its alias copies overwrites whole (same format, every level and layer, from the origin, at 1x, on a
  fully backed image), the guest upload and the older copies before it are skipped.
- **0062, stale guest memory** (Lichii #818). When the collector drops a GPU-written image without
  writing it back, its whole pages are marked stale and kept tracked. A new image made entirely on
  stale pages starts without an upload, unless the buffer cache wrote any of those pages on the GPU.
  CPU writes, unmaps and write-backs clear the marks. Switch 2 titles only.
  - PS5 difference: core 0050 also marks texture-cache writes in `g_nx2_gpu_written_pages` (for the
    console's unwritten-page reads), so Lichii's check would almost never skip here. 0062 adds a
    buffer-cache-only bitmap, `g_nx2_buffer_written_pages`, and `Nx2MayBeGpuWritten` reads that.
  - `StalePages::PAGE_SIZE` is `PAGE_BYTES` here: the PS5 SDK defines `PAGE_SIZE`.

Measured: Mario Kart World 1x title, 3 runs each: 23.2 / 27.6 / 26.9 FPS before, 25.6 / 28.2 /
28.4 after (about +5%). Out-of-memory survival is unchanged (both builds run out at about
200-215 s in most runs). Donkey Kong Bananza's Ingot Isle gate: unchanged (16.0 FPS).
