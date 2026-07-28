#!/usr/bin/env bash
# ============================================================
# XPU Linux Installer - Downloads a REAL Linux distribution
# ============================================================
#
# This script downloads a real Linux rootfs (Ubuntu, Alpine, or
# Debian) and bundles it with XPU. The rootfs is entered via
# `proot` (no root required) - this is the same approach used
# by UserLAnd, Andronix, and Anlinux.
#
# This is NOT a fake environment. It's a real Linux distribution
# running inside XPU using proot's syscall translation.
#
# What you get:
#   - Real Ubuntu 22.04 / Alpine 3.19 / Debian 12 rootfs
#   - Real apt / apk package manager
#   - Real bash/dash shell
#   - Real coreutils, findutils, grep, etc.
#   - Real Python/Node/GCC if you install them via apt
#
# Requirements:
#   - proot (installed automatically on Termux via pkg)
#   - ~200MB free disk space for minimal rootfs
#   - Network access for download
#
# Usage:
#   bash scripts/install_linux.sh              # interactive
#   bash scripts/install_linux.sh ubuntu       # Ubuntu 22.04
#   bash scripts/install_linux.sh alpine       # Alpine 3.19
#   bash scripts/install_linux.sh debian       # Debian 12
#   bash scripts/install_linux.sh remove       # remove rootfs
# ============================================================

set -e

# Colors
RED='\033[1;31m'
BLUE='\033[1;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ROOTFS_DIR="${HOME}/.xpu/rootfs-linux"
PROOT_BIN=""

# Real download URLs (verified working as of 2024)
# These are the same URLs used by Anlinux project
declare -A DISTRO_URLS=(
    ["ubuntu"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Ubuntu/arm64/ubuntu-rootfs-arm64.tar.xz"
    ["ubuntu-x86"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Ubuntu/amd64/ubuntu-rootfs-amd64.tar.xz"
    ["alpine"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Alpine/arm64/alpine-rootfs-arm64.tar.xz"
    ["alpine-x86"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Alpine/amd64/alpine-rootfs-amd64.tar.xz"
    ["debian"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Debian/arm64/debian-rootfs-arm64.tar.xz"
    ["debian-x86"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Debian/amd64/debian-rootfs-amd64.tar.xz"
)

declare -A DISTRO_SIZES=(
    ["ubuntu"]="~120MB download, ~350MB extracted"
    ["alpine"]="~3MB download, ~15MB extracted"
    ["debian"]="~90MB download, ~280MB extracted"
)

detect_arch() {
    local arch=$(uname -m)
    case "$arch" in
        aarch64|arm64) echo "arm64" ;;
        x86_64|amd64)  echo "amd64" ;;
        armv7l|armv8l) echo "armhf" ;;
        *) echo "unknown" ;;
    esac
}

find_proot() {
    # Try common proot locations
    for path in \
        "/data/data/com.termux/files/usr/bin/proot" \
        "/usr/local/bin/proot" \
        "/usr/bin/proot"; do
        if [ -x "$path" ]; then
            PROOT_BIN="$path"
            return 0
        fi
    done
    # Try PATH
    if command -v proot >/dev/null 2>&1; then
        PROOT_BIN=$(command -v proot)
        return 0
    fi
    return 1
}

install_proot() {
    echo -e "${YELLOW}[setup] Installing proot...${NC}"
    if [ -d "/data/data/com.termux" ]; then
        pkg install -y proot tar xz-utils
    elif command -v apt >/dev/null 2>&1; then
        sudo apt install -y proot || apt install -y proot
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y proot
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --noconfirm proot
    else
        echo -e "${RED}[error] Cannot install proot automatically${NC}"
        echo "Please install proot manually: https://proot.gitlab.io/"
        return 1
    fi
}

download_rootfs() {
    local distro="$1"
    local arch=$(detect_arch)
    local url_key

    if [ "$arch" = "amd64" ]; then
        url_key="${distro}-x86"
    elif [ "$arch" = "arm64" ] || [ "$arch" = "armhf" ]; then
        url_key="${distro}"
    else
        echo -e "${RED}[error] Unsupported architecture: $arch${NC}"
        return 1
    fi

    local url="${DISTRO_URLS[$url_key]}"
    if [ -z "$url" ]; then
        echo -e "${RED}[error] Unknown distro: $distro${NC}"
        return 1
    fi

    echo -e "${YELLOW}[download] Distro: $distro ($arch)${NC}"
    echo -e "${YELLOW}[download] Size:  ${DISTRO_SIZES[$distro]}${NC}"
    echo -e "${YELLOW}[download] URL:   $url${NC}"
    echo -e "${YELLOW}[download] Dest:  $ROOTFS_DIR${NC}"
    echo ""

    mkdir -p "$ROOTFS_DIR"
    local tmp_tar="/tmp/xpu-rootfs-$(date +%s).tar.xz"

    echo -e "${YELLOW}[download] Downloading... (this may take a while)${NC}"
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "$tmp_tar" "$url" || {
            echo -e "${RED}[error] Download failed${NC}"
            return 1
        }
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$tmp_tar" "$url" || {
            echo -e "${RED}[error] Download failed${NC}"
            return 1
        }
    else
        echo -e "${RED}[error] Need curl or wget${NC}"
        return 1
    fi

    local size=$(stat -c%s "$tmp_tar" 2>/dev/null || stat -f%z "$tmp_tar")
    echo -e "${GREEN}[download] Downloaded $((size / 1024 / 1024)) MB${NC}"

    echo -e "${YELLOW}[extract] Extracting rootfs...${NC}"
    if ! command -v tar >/dev/null 2>&1; then
        echo -e "${RED}[error] tar not found${NC}"
        return 1
    fi
    tar xf "$tmp_tar" -C "$ROOTFS_DIR" 2>&1 | tail -3
    rm -f "$tmp_tar"

    if [ ! -d "$ROOTFS_DIR/bin" ]; then
        echo -e "${RED}[error] Extraction failed - no /bin in rootfs${NC}"
        return 1
    fi

    echo -e "${GREEN}[extract] ✓ Rootfs ready at $ROOTFS_DIR${NC}"

    # Create XPU integration files inside the rootfs
    setup_rootfs_integration

    return 0
}

setup_rootfs_integration() {
    echo -e "${YELLOW}[setup] Integrating XPU into rootfs...${NC}"

    # Create /etc/os-release customization
    cat > "$ROOTFS_DIR/etc/xpu-release" << EOF
XPU_LINUX=1
XPU_VERSION=1.5.0
XPU_INTEGRATED=1
XPU_HOST_KERNEL=$(uname -r)
XPU_HOST_ARCH=$(uname -m)
EOF

    # Create /etc/motd
    cat > "$ROOTFS_DIR/etc/motd" << 'EOF'

    ╔══════════════════════════════════════════════╗
    ║   XPU-Linux (REAL Linux inside XPU)          ║
    ╚══════════════════════════════════════════════╝

  This is a REAL Linux distribution running inside XPU
  via proot. The host kernel is shared (Android/Linux),
  but the userspace is 100% real.

  Type 'apt update' or 'apk update' to get started.
  Type 'xpu-info' to see XPU integration info.

EOF

    # Create xpu-info script inside rootfs
    mkdir -p "$ROOTFS_DIR/usr/local/bin"
    cat > "$ROOTFS_DIR/usr/local/bin/xpu-info" << 'EOF'
#!/bin/bash
echo "XPU-Linux Integration Info"
echo "=========================="
echo "XPU version    : 1.5.0"
echo "Rootfs path    : (this environment)"
echo "Host kernel    : $(uname -r)"
echo "Architecture   : $(uname -m)"
echo "Distribution   : $(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d'"' -f2)"
echo "Shell          : $SHELL"
echo "User           : $(whoami)"
echo ""
echo "This is a REAL Linux environment. Not a simulation."
echo "Package manager: $(command -v apt 2>/dev/null && echo apt || (command -v apk 2>/dev/null && echo apk || echo unknown))"
EOF
    chmod +x "$ROOTFS_DIR/usr/local/bin/xpu-info"

    # Create xpu-shell wrapper that drops into bash
    cat > "$ROOTFS_DIR/usr/local/bin/xpu-shell" << 'EOF'
#!/bin/bash
# XPU shell - just launches bash with the XPU prompt
export PS1='\[\033[1;31m\]Kernel\[\033[0m\]@\[\033[1;34m\]xpu\[\033[0m\]:\[\033[1;32m\]\w\[\033[0m\]\$ '
exec bash "$@"
EOF
    chmod +x "$ROOTFS_DIR/usr/local/bin/xpu-shell"

    echo -e "${GREEN}[setup] ✓ XPU integration complete${NC}"
}

remove_rootfs() {
    echo -e "${YELLOW}[remove] Removing rootfs at $ROOTFS_DIR...${NC}"
    if [ -d "$ROOTFS_DIR" ]; then
        rm -rf "$ROOTFS_DIR"
        echo -e "${GREEN}[remove] ✓ Removed${NC}"
    else
        echo -e "${YELLOW}[remove] No rootfs found${NC}"
    fi
}

show_status() {
    echo -e "${BLUE}=== XPU Linux Status ===${NC}"
    echo ""
    if [ -d "$ROOTFS_DIR/bin" ]; then
        echo -e "  Rootfs: ${GREEN}installed${NC} at $ROOTFS_DIR"
        local size=$(du -sh "$ROOTFS_DIR" 2>/dev/null | cut -f1)
        echo -e "  Size:   $size"
        if [ -f "$ROOTFS_DIR/etc/os-release" ]; then
            local distro=$(grep PRETTY_NAME "$ROOTFS_DIR/etc/os-release" | cut -d'"' -f2)
            echo -e "  Distro: $distro"
        fi
    else
        echo -e "  Rootfs: ${RED}not installed${NC}"
        echo ""
        echo "  Install with: bash scripts/install_linux.sh ubuntu"
    fi
    echo ""
    if [ -n "$PROOT_BIN" ]; then
        echo -e "  proot:  ${GREEN}found${NC} at $PROOT_BIN"
    else
        echo -e "  proot:  ${RED}not found${NC}"
    fi
}

main() {
    echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║   XPU Linux Installer - REAL Linux for XPU        ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
    echo ""

    # Find or install proot
    if ! find_proot; then
        install_proot || exit 1
        find_proot || {
            echo -e "${RED}[error] proot still not found after install attempt${NC}"
            exit 1
        }
    fi

    local cmd="${1:-}"

    case "$cmd" in
        ubuntu|alpine|debian)
            if [ -d "$ROOTFS_DIR/bin" ]; then
                echo -e "${YELLOW}[warn] Rootfs already exists at $ROOTFS_DIR${NC}"
                read -p "Overwrite? (y/N) " -n 1 -r
                echo
                if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                    echo "Aborted."
                    exit 0
                fi
                rm -rf "$ROOTFS_DIR"
            fi
            download_rootfs "$cmd"
            ;;
        remove)
            remove_rootfs
            ;;
        status|"")
            show_status
            ;;
        *)
            echo "Usage: $0 [ubuntu|alpine|debian|remove|status]"
            echo ""
            echo "This downloads a REAL Linux distribution rootfs and"
            echo "integrates it with XPU. The rootfs is entered via"
            echo "proot (no root required)."
            echo ""
            echo "Available distros:"
            echo "  ubuntu   - Ubuntu 22.04 (recommended, ~350MB)"
            echo "  alpine   - Alpine 3.19 (minimal, ~15MB)"
            echo "  debian   - Debian 12 (~280MB)"
            echo ""
            echo "Commands:"
            echo "  status   - show current installation"
            echo "  remove   - remove rootfs"
            exit 1
            ;;
    esac
}

main "$@"
