# Core update experiment patches

Applied in order on upstream Eglantine nx2 babdb6bb75 (the submodule pointer on this branch):

    git -C Eglantine checkout babdb6bb752af8a35e94b305613d6be7ba7e6217
    git -C Eglantine am ../tools/eglantine-patches-core-update/*.patch

Then build with the lenient port: `.local/core/build.sh NAME` (EDEN_PORT_LENIENT=1). Results and
open issues: docs/CORE-UPDATE-EXPERIMENT.md.
