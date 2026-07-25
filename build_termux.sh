#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
# XPU - Termux build script for Android phones
# ============================================================
# Tested on:
#   - Samsung Galaxy A10s  (MediaTek Helio P22, ARMv8-A, NEON)
#   - Samsung Galaxy A12   (MediaTek Helio P35)
#   - Xiaomi Redmi 9A      (MediaTek Helio G25)
#   - Any Android 7+ phone with Termux
#
# Usage:
#   bash build_termux.sh            # full build + tests
#   bash build_termux.sh build      # just build
#   bash build_termux.sh run        # build + run daemon
#   bash build_termux.sh clean      # clean and rebuild
# ============================================================

set -e  # exit on first error

# --- Colors for nice output ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   XPU build for Termux (Android phones)            ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"

# --- 1. Setup locale (fixes setlocale warnings) ---
echo -e "${YELLOW}[1/6] Setting up locale...${NC}"
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANGUAGE=en_US.UTF-8

# --- 2. Install dependencies ---
echo -e "${YELLOW}[2/6] Checking dependencies...${NC}"
PKG_LIST=""
command -v clang++   >/dev/null 2>&1 || PKG_LIST="$PKG_LIST clang"
command -v make      >/dev/null 2>&1 || PKG_LIST="$PKG_LIST make"
command -v git       >/dev/null 2>&1 || PKG_LIST="$PKG_LIST git"

if [ -n "$PKG_LIST" ]; then
    echo -e "${YELLOW}Installing:$PKG_LIST${NC}"
    pkg install -y $PKG_LIST
fi

# --- 3. Detect architecture ---
echo -e "${YELLOW}[3/6] Detecting architecture...${NC}"
ARCH=$(uname -m)
echo -e "  Architecture: ${GREEN}$ARCH${NC}"

case "$ARCH" in
    aarch64|armv8l)
        echo -e "  ${GREEN}✓${NC} AArch64 with NEON SIMD (recommended path)"
        ;;
    armv7l|arm)
        echo -e "  ${YELLOW}⚠${NC}  ARMv7 (32-bit). NEON will be used if available."
        ;;
    x86_64)
        echo -e "  ${GREEN}✓${NC} x86_64 with SSE2/AVX2"
        ;;
    *)
        echo -e "  ${YELLOW}⚠${NC}  Unknown architecture ($ARCH) - will use scalar fallback"
        ;;
esac

# --- 4. Configure compiler ---
# Termux's clang++ is the recommended compiler. Use it explicitly.
export CXX=${CXX:-clang++}
export CC=${CC:-clang}

# Show phone info if available
if [ -f /proc/cpuinfo ]; then
    CPU_MODEL=$(grep -m1 -i "Hardware\|model name" /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs)
    if [ -n "$CPU_MODEL" ]; then
        echo -e "  CPU: ${GREEN}$CPU_MODEL${NC}"
    fi
    CPU_CORES=$(nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo)
    echo -e "  Cores: ${GREEN}$CPU_CORES${NC}"
fi

# Memory info
if [ -f /proc/meminfo ]; then
    MEM_TOTAL=$(grep MemTotal /proc/meminfo | awk '{print int($2/1024)}')
    echo -e "  RAM: ${GREEN}${MEM_TOTAL} MB${NC}"
fi

# --- 5. Build ---
echo -e "${YELLOW}[5/6] Building XPU...${NC}"
ACTION=${1:-build}

if [ "$ACTION" = "clean" ]; then
    echo -e "  Cleaning previous build..."
    make clean
    ACTION=build
fi

if [ "$ACTION" = "build" ] || [ "$ACTION" = "run" ]; then
    # Use clang++ explicitly; pass -fPIC for shared lib
    make -j$(nproc) CXX="$CXX" CC="$CC" 2>&1 | tail -20
fi

# --- 6. Test ---
if [ "$ACTION" != "clean" ]; then
    echo -e "${YELLOW}[6/6] Running tests...${NC}"
    export LD_LIBRARY_PATH="$(pwd)/build:$LD_LIBRARY_PATH"
    if [ -x build/xpu_test_math ]; then
        echo -e "${BLUE}── Math tests ──${NC}"
        ./build/xpu_test_math 2>&1 | tail -8
    fi
    if [ -x build/xpu_test_rasterizer ]; then
        echo -e "${BLUE}── Rasterizer tests ──${NC}"
        ./build/xpu_test_rasterizer 2>&1 | tail -5
    fi
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   ✓ Build complete!                                ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Next steps:"
echo -e "  ${BLUE}Run the render daemon (rotating 3D cube):${NC}"
echo -e "    LD_LIBRARY_PATH=build ./build/xpu_render_daemon --width 640 --height 480"
echo ""
echo -e "  ${BLUE}Benchmark mode (no saving, just FPS):${NC}"
echo -e "    LD_LIBRARY_PATH=build ./build/xpu_render_daemon --no-save --max-frames 1000"
echo ""
echo -e "  ${BLUE}Run on background (like screen recorder):${NC}"
echo -e "    nohup ./build/xpu_render_daemon --width 320 --height 240 > render.log 2>&1 &"
echo ""

if [ "$ACTION" = "run" ]; then
    echo -e "${YELLOW}Starting render daemon now...${NC}"
    LD_LIBRARY_PATH=build ./build/xpu_render_daemon --width 640 --height 480 --max-frames 60
fi
