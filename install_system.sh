#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
# XPU - System installer for Termux/Linux
# ============================================================
# Installs XPU as a system component so commands work from
# anywhere. Mimics a "GPU driver" install in the sense that
# it puts libraries in /usr/local/lib and binaries in
# /usr/local/bin (or $PREFIX on Termux).
#
# After install, you can run from any directory:
#   xpu-info           - show library info
#   xpu-check          - verify library integrity
#   xpu-loader         - start loader daemon
#   xpu-render-daemon  - background renderer
#   xpu-benchmark      - performance test
#   xpu-nn-train       - train XOR neural network
#   xpu-mnist-train    - train MNIST-like CNN
#   xpu-update         - check + install updates
#   xpu-uninstall      - cleanly remove XPU
#
# Usage:
#   bash install_system.sh           # install
#   bash install_system.sh --force   # reinstall over existing
# ============================================================

set -e

PREFIX="${PREFIX:-/usr/local}"
ON_TERMUX=0
if [ -d "/data/data/com.termux" ]; then
    ON_TERMUX=1
    PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
fi

LIBDIR="$PREFIX/lib"
BINDIR="$PREFIX/bin"
INCDIR="$PREFIX/include"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   XPU System Installer                             ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════╝${NC}"
echo ""

if [ "$ON_TERMUX" = "1" ]; then
    echo -e "  Platform: ${YELLOW}Termux (Android)${NC}"
else
    echo -e "  Platform: ${YELLOW}Linux${NC}"
fi
echo -e "  Prefix  : $PREFIX"
echo ""

# 1. Build first if needed
if [ ! -f "build/libxpu.so" ]; then
    echo -e "${YELLOW}[1/5] Building XPU first...${NC}"
    if [ "$ON_TERMUX" = "1" ]; then
        make CXX=clang++ CC=clang -j$(nproc) 2>&1 | tail -5
    else
        make -j$(nproc) 2>&1 | tail -5
    fi
else
    echo -e "${YELLOW}[1/5] Using existing build/libxpu.so${NC}"
fi

# 2. Create directories
echo -e "${YELLOW}[2/5] Creating directories...${NC}"
mkdir -p "$LIBDIR" "$BINDIR" "$INCDIR/xpu"

# On Termux, create a /tmp symlink for compatibility with scripts that
# hardcode /tmp (we don't anymore, but this helps third-party tools)
if [ "$ON_TERMUX" = "1" ] && [ ! -d "/tmp" ]; then
    mkdir -p "$PREFIX/tmp" 2>/dev/null || true
    export TMPDIR="$PREFIX/tmp"
fi

# 3. Install library + headers
echo -e "${YELLOW}[3/5] Installing library + headers...${NC}"
cp build/libxpu.so "$LIBDIR/libxpu.so"
chmod 755 "$LIBDIR/libxpu.so"
cp -r include/xpu/*.h "$INCDIR/xpu/"

# Write version file (used by updater)
echo "1.3.0" > "$LIBDIR/xpu_version.txt"

# 4. Install binaries with nice names
echo -e "${YELLOW}[4/5] Installing command-line tools...${NC}"
install -m 755 build/xpu_loader "$BINDIR/xpu-loader" 2>/dev/null || \
    gcc -O2 -o "$BINDIR/xpu-loader" src/loader/xpu_loader.c -ldl 2>/dev/null || true
install -m 755 build/xpu_updater "$BINDIR/xpu-update" 2>/dev/null || \
    gcc -O2 -o "$BINDIR/xpu-update" src/updater/xpu_updater.c 2>/dev/null || true

# Wrapper scripts so we can run from any directory
cat > "$BINDIR/xpu-info" << 'EOF'
#!/bin/sh
exec xpu-loader --info "$@"
EOF
chmod 755 "$BINDIR/xpu-info"

cat > "$BINDIR/xpu-check" << 'EOF'
#!/bin/sh
exec xpu-loader --check "$@"
EOF
chmod 755 "$BINDIR/xpu-check"

cat > "$BINDIR/xpu-render-daemon" << 'EOF'
#!/bin/sh
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/data/data/com.termux/files/usr/lib:/usr/local/lib" \
exec xpu-render-daemon-bin "$@"
EOF
chmod 755 "$BINDIR/xpu-render-daemon"
install -m 755 build/xpu_render_daemon "$BINDIR/xpu-render-daemon-bin" 2>/dev/null || true

cat > "$BINDIR/xpu-benchmark" << 'EOF'
#!/bin/sh
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/data/data/com.termux/files/usr/lib:/usr/local/lib" \
exec xpu-benchmark-bin "$@"
EOF
chmod 755 "$BINDIR/xpu-benchmark"
install -m 755 build/xpu_benchmark "$BINDIR/xpu-benchmark-bin" 2>/dev/null || true

cat > "$BINDIR/xpu-nn-train" << 'EOF'
#!/bin/sh
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/data/data/com.termux/files/usr/lib:/usr/local/lib" \
exec xpu-nn-train-bin "$@"
EOF
chmod 755 "$BINDIR/xpu-nn-train"
install -m 755 build/xpu_nn_train "$BINDIR/xpu-nn-train-bin" 2>/dev/null || true

cat > "$BINDIR/xpu-mnist-train" << 'EOF'
#!/bin/sh
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/data/data/com.termux/files/usr/lib:/usr/local/lib" \
exec xpu-mnist-train-bin "$@"
EOF
chmod 755 "$BINDIR/xpu-mnist-train"
install -m 755 build/xpu_mnist_train "$BINDIR/xpu-mnist-train-bin" 2>/dev/null || true

# Uninstaller
cat > "$BINDIR/xpu-uninstall" << 'UNINST_EOF'
#!/bin/sh
echo "Removing XPU system installation..."
PREFIX="${PREFIX:-/usr/local}"
[ -d "/data/data/com.termux" ] && PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
rm -f "$PREFIX/lib/libxpu.so"
rm -f "$PREFIX/lib/xpu_version.txt"
rm -f "$PREFIX/bin/xpu-loader"
rm -f "$PREFIX/bin/xpu-update"
rm -f "$PREFIX/bin/xpu-info"
rm -f "$PREFIX/bin/xpu-check"
rm -f "$PREFIX/bin/xpu-render-daemon"
rm -f "$PREFIX/bin/xpu-render-daemon-bin"
rm -f "$PREFIX/bin/xpu-benchmark"
rm -f "$PREFIX/bin/xpu-benchmark-bin"
rm -f "$PREFIX/bin/xpu-nn-train"
rm -f "$PREFIX/bin/xpu-nn-train-bin"
rm -f "$PREFIX/bin/xpu-mnist-train"
rm -f "$PREFIX/bin/xpu-mnist-train-bin"
rm -f "$PREFIX/bin/xpu-shell"
rm -f "$PREFIX/bin/xpi"
rm -f "$PREFIX/bin/xpu-uninstall"
rm -rf "$PREFIX/include/xpu"
echo "XPU removed. (~/.xpu/rootfs is kept - remove manually if needed)"
echo "  rm -rf ~/.xpu"
UNINST_EOF
chmod 755 "$BINDIR/xpu-uninstall"

# Install shell + xpi
install -m 755 build/xpu_shell "$BINDIR/xpu-shell" 2>/dev/null || true
install -m 755 build/xpi "$BINDIR/xpi" 2>/dev/null || true
install -m 755 build/xpu_system "$BINDIR/xpu-system" 2>/dev/null || true

# Create convenience alias: 'xpu' enters the real Linux environment
# (prefers xpu-system if a rootfs is installed, falls back to xpu-shell)
cat > "$BINDIR/xpu" << 'EOF'
#!/bin/sh
# If a real Linux rootfs is installed, enter it via proot
if [ -d "$HOME/.xpu/rootfs-linux/bin" ]; then
    exec xpu-system "$@"
else
    # Fall back to the lightweight XPU shell
    exec xpu-shell "$@"
fi
EOF
chmod 755 "$BINDIR/xpu"

# 5. Update shared library cache
echo -e "${YELLOW}[5/5] Updating library cache...${NC}"
if [ "$ON_TERMUX" = "1" ]; then
    ldconfig "$LIBDIR" 2>/dev/null || true
    termux-reload-settings 2>/dev/null || true
else
    if command -v ldconfig >/dev/null 2>&1; then
        sudo ldconfig 2>/dev/null || ldconfig 2>/dev/null || true
    fi
fi

# Verify
echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   ✓ Installation complete!                         ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Installed:"
echo "  Library : $LIBDIR/libxpu.so"
echo "  Headers : $INCDIR/xpu/"
echo "  Version : $LIBDIR/xpu_version.txt"
echo ""
echo "Commands available from anywhere:"
echo "  xpu               - enter REAL Linux (Ubuntu/Alpine) if installed, else XPU shell"
echo "  xpu-system        - enter the real Linux rootfs via proot"
echo "  xpu-shell         - enter the lightweight XPU shell"
echo "  xpu-system --status - check if real Linux rootfs is installed"
echo "  xpi install <pkg> - install packages via apt/apk"
echo "  xpu-info          - show library info"
echo "  xpu-check         - verify integrity"
echo "  xpu-loader        - start loader daemon"
echo "  xpu-update        - check + install updates"
echo "  xpu-render-daemon - background 3D renderer"
echo "  xpu-benchmark     - performance test"
echo "  xpu-nn-train      - train XOR neural network"
echo "  xpu-mnist-train   - train MNIST-like CNN"
echo "  xpu-uninstall     - remove XPU completely"
echo ""
echo -e "${YELLOW}To install REAL Linux inside XPU:${NC}"
echo "  make install-linux ubuntu    # ~350MB, full Ubuntu 22.04"
echo "  make install-linux alpine    # ~15MB, minimal Alpine 3.19"
echo "  make install-linux debian    # ~280MB, Debian 12"
echo ""
echo -e "${GREEN}XPU is now installed as a system component.${NC}"
