#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The browser build without Emscripten (the DRONE fork): clang's wasm32 target with wasi-libc, the same file
# (web/emu/felucca_web.c) and the same page as web/emu/build.sh. The module imports nothing (the worklet instantiates
# it with {}), its _initialize is wasi-libc's reactor entry.
#   apt install clang lld wasi-libc libclang-rt-18-dev-wasm32      (Ubuntu 24.04; or any clang with a WASI sysroot)
#   web/emu/build_clang.sh    ->  build/emu/{index.html, worklet.js, felucca.wasm, fonts/}
# Run after one ./build.sh, or tools/build.py's generate step alone (build/gen: the tables, the fonts, the samples).
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: build/gen missing (run ./build.sh once)"; exit 1; }
SYSROOT_INC="${WASI_INC:-/usr/include/wasm32-wasi}"
SYSROOT_LIB="${WASI_LIB:-/usr/lib/wasm32-wasi}"
VERSION=${EMU_VERSION:-$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)}
mkdir -p build/emu/fonts build/emu-shim/sys
cat > build/emu-shim/sys/wait.h <<'H'
/* wasi-libc has no processes: tests/hostsim.c's fork / wait (its test runners, never called in the browser) */
#include <sys/types.h>
static inline pid_t fork(void) { return -1; }
static inline pid_t waitpid(pid_t p, int *st, int o) { (void)p; (void)o; if (st) *st = 0; return -1; }
static inline int pipe(int fd[2]) { (void)fd; return -1; }
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) 0
H
clang --target=wasm32-wasi ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/emu-shim -Ibuild/gen -Ifirmware/src -I"$SYSROOT_INC" -L"$SYSROOT_LIB" \
    -mexec-model=reactor -Wl,--export-dynamic -Wl,-z,stack-size=1048576 -Wl,--initial-memory=33554432 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, $VERSION, clang)"
