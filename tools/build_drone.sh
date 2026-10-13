#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# One-shot build of the DRONE fork on Linux x86-64 or WSL (Ubuntu): fetches what the build needs the first time
# (Python packages, the JieLi toolchain, the AC79 SDK), then builds build/felucca-<version>.fwsc.
#   tools/build_drone.sh [version]          (default 1.5.1-dr4)
set -e
cd "$(dirname "$0")/.."
V="${1:-1.5.1-dr4}"
python3 -c 'import PIL, fontTools' 2>/dev/null || pip3 install --user Pillow fonttools ||
    pip3 install --break-system-packages Pillow fonttools
[ -x "$HOME/.jieli/toolchain/pi32v2/bin/clang" ] || tools/get_toolchain.sh "$HOME/.jieli"
[ -f "$HOME/fw-AC79_AIoT_SDK/cpu/wl82/tools/uboot.boot" ] ||
    git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
        https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$HOME/fw-AC79_AIoT_SDK"
./build.sh --release "$V"
echo "package: build/felucca-$V.fwsc   install: python3 tools/fm1_install.py build/felucca-$V.fwsc"
