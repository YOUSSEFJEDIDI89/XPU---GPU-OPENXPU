#!/usr/bin/env bash
# ============================================================
# XPU Linux Installer - Downloads a REAL Linux distribution
# ============================================================
# Fixed version: works on Termux (no /tmp), better arch detect,
# retry on failure, resume downloads.
#
# Usage:
#   bash scripts/install_linux.sh              # interactive
#   bash scripts/install_linux.sh ubuntu       # Ubuntu 22.04
#   bash scripts/install_linux.sh alpine       # Alpine 3.19
#   bash scripts/install_linux.sh debian       # Debian 12
#   bash scripts/install_linux.sh remove       # remove rootfs
#   bash scripts/install_linux.sh status       # show status
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

# ----------------------------------------------------------------
# Detect a writable temp directory.
# Termux doesn't have /tmp - use $TMPDIR or $PREFIX/tmp instead.
# ----------------------------------------------------------------
detect_tmp_dir() {
    # Try in order: TMPDIR env, /tmp, $PREFIX/tmp, $HOME/.cache, .
    if [ -n "$TMPDIR" ] && [ -d "$TMPDIR" ] && [ -w "$TMPDIR" ]; then
        echo "$TMPDIR"
        return 0
    fi
    if [ -d "/tmp" ] && [ -w "/tmp" ]; then
        echo "/tmp"
        return 0
    fi
    if [ -n "$PREFIX" ] && [ -d "$PREFIX/tmp" ] && [ -w "$PREFIX/tmp" ]; then
        echo "$PREFIX/tmp"
        return 0
    fi
    # Termux default
    if [ -d "/data/data/com.termux/files/usr/tmp" ] && [ -w "/data/data/com.termux/files/usr/tmp" ]; then
        echo "/data/data/com.termux/files/usr/tmp"
        return 0
    fi
    # Fallback: home/.cache
    local cache="${HOME}/.cache"
    mkdir -p "$cache" 2>/dev/null
    if [ -d "$cache" ] && [ -w "$cache" ]; then
        echo "$cache"
        return 0
    fi
    # Last resort: current dir
    echo "."
    return 0
}

# Real download URLs - using httpredir for better reliability
# These are the official Anlinux rootfs URLs
declare -A DISTRO_URLS_ARM64=(
    ["ubuntu"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Ubuntu/arm64/ubuntu-rootfs-arm64.tar.xz"
    ["alpine"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Alpine/arm64/alpine-rootfs-arm64.tar.xz"
    ["debian"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Debian/arm64/debian-rootfs-arm64.tar.xz"
)

declare -A DISTRO_URLS_AMD64=(
    ["ubuntu"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Ubuntu/amd64/ubuntu-rootfs-amd64.tar.xz"
    ["alpine"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Alpine/amd64/alpine-rootfs-amd64.tar.xz"
    ["debian"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Debian/amd64/debian-rootfs-amd64.tar.xz"
)

declare -A DISTRO_URLS_ARMHF=(
    ["ubuntu"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Ubuntu/armhf/ubuntu-rootfs-armhf.tar.xz"
    ["alpine"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Alpine/armhf/alpine-rootfs-armhf.tar.xz"
    ["debian"]="https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Debian/armhf/debian-rootfs-armhf.tar.xz"
)

declare -A DISTRO_SIZES=(
    ["ubuntu"]="~120MB download, ~350MB extracted"
    ["alpine"]="~3MB download, ~15MB extracted"
    ["debian"]="~90MB download, ~280MB extracted"
)

# ----------------------------------------------------------------
# Detect architecture correctly.
# Samsung A10s (Helio P22) is AArch64 - returns "arm64"
# 32-bit Android on ARMv7 returns "armhf"
# x86_64 returns "amd64"
# ----------------------------------------------------------------
detect_arch() {
    local arch=$(uname -m)
    case "$arch" in
        aarch64|arm64)
            echo "arm64"
            ;;
        x86_64|amd64)
            echo "amd64"
            ;;
        armv7l|armv8l)
            # armv8l is 32-bit userspace on 64-bit kernel
            # but we should still use armhf rootfs
            echo "armhf"
            ;;
        i686|i386)
            echo "i386"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

find_proot() {
    for path in \
        "/data/data/com.termux/files/usr/bin/proot" \
        "/usr/local/bin/proot" \
        "/usr/bin/proot" \
        "/sbin/proot"; do
        if [ -x "$path" ]; then
            PROOT_BIN="$path"
            return 0
        fi
    done
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
        sudo apt install -y proot tar xz-utils 2>/dev/null || apt install -y proot tar xz-utils
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y proot tar xz
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --noconfirm proot tar xz
    else
        echo -e "${RED}[error] Cannot install proot automatically${NC}"
        echo "Please install proot manually: https://proot.gitlab.io/"
        return 1
    fi
}

# ----------------------------------------------------------------
# Download with retry and resume support
# ----------------------------------------------------------------
download_with_retry() {
    local url="$1"
    local dest="$2"
    local max_retries=3
    local attempt=1

    while [ $attempt -le $max_retries ]; do
        echo -e "${YELLOW}[download] Attempt $attempt/$max_retries${NC}"

        # Try curl first (with -C - for resume support)
        if command -v curl >/dev/null 2>&1; then
            echo -e "${YELLOW}[download] Using curl (with resume)...${NC}"
            # -L: follow redirects
            # -C -: resume from where we left off
            # --retry: built-in retry
            # --connect-timeout: 30s
            if curl -L -C - --retry 3 --connect-timeout 30 \
                   -H "User-Agent: XPU-Updater/1.0" \
                   -o "$dest" "$url"; then
                # Verify file is not empty
                if [ -s "$dest" ]; then
                    echo -e "${GREEN}[download] ✓ Download complete${NC}"
                    return 0
                fi
            fi
        fi

        # Fall back to wget
        if command -v wget >/dev/null 2>&1; then
            echo -e "${YELLOW}[download] Using wget (with resume)...${NC}"
            # -c: continue (resume)
            # --tries: retry count
            # --timeout: per-read timeout
            if wget -c --tries=3 --timeout=30 \
                    --user-agent="XPU-Updater/1.0" \
                    -O "$dest" "$url"; then
                if [ -s "$dest" ]; then
                    echo -e "${GREEN}[download] ✓ Download complete${NC}"
                    return 0
                fi
            fi
        fi

        echo -e "${RED}[download] Attempt $attempt failed${NC}"
        attempt=$((attempt + 1))
        if [ $attempt -le $max_retries ]; then
            echo -e "${YELLOW}[download] Waiting 5 seconds before retry...${NC}"
            sleep 5
        fi
    done

    echo -e "${RED}[error] Download failed after $max_retries attempts${NC}"
    return 1
}

download_rootfs() {
    local distro="$1"
    local arch=$(detect_arch)
    local url

    echo -e "${YELLOW}[arch] Detected architecture: ${GREEN}$arch${NC}"

    case "$arch" in
        arm64)
            url="${DISTRO_URLS_ARM64[$distro]}"
            ;;
        amd64)
            url="${DISTRO_URLS_AMD64[$distro]}"
            ;;
        armhf)
            url="${DISTRO_URLS_ARMHF[$distro]}"
            ;;
        *)
            echo -e "${RED}[error] Unsupported architecture: $arch${NC}"
            echo "Supported: arm64 (AArch64), amd64 (x86_64), armhf (ARMv7)"
            return 1
            ;;
    esac

    if [ -z "$url" ]; then
        echo -e "${RED}[error] Unknown distro: $distro${NC}"
        echo "Available: ubuntu, alpine, debian"
        return 1
    fi

    echo -e "${YELLOW}[download] Distro: ${GREEN}$distro${NC} ${YELLOW}($arch)${NC}"
    echo -e "${YELLOW}[download] Size:  ${DISTRO_SIZES[$distro]}${NC}"
    echo -e "${YELLOW}[download] URL:   $url${NC}"
    echo -e "${YELLOW}[download] Dest:  $ROOTFS_DIR${NC}"
    echo ""

    mkdir -p "$ROOTFS_DIR"

    # Get a writable temp directory
    local tmp_dir=$(detect_tmp_dir)
    echo -e "${YELLOW}[tmp] Using temp directory: $tmp_dir${NC}"

    local tmp_tar="$tmp_dir/xpu-rootfs-$(date +%s).tar.xz"

    echo -e "${YELLOW}[download] Downloading... (this may take a while)${NC}"
    if ! download_with_retry "$url" "$tmp_tar"; then
        echo ""
        echo -e "${RED}[error] Download failed${NC}"
        echo ""
        echo "Possible causes:"
        echo "  1. No internet connection"
        echo "  2. GitHub is blocked in your region (try a VPN)"
        echo "  3. Disk full in temp dir ($tmp_dir)"
        echo "  4. Old curl/wget version"
        echo ""
        echo "Manual download alternative:"
        echo "  1. Open this URL in a browser:"
        echo "     $url"
        echo "  2. Move the downloaded file to:"
        echo "     $tmp_tar"
        echo "  3. Re-run: bash scripts/install_linux.sh $distro"
        rm -f "$tmp_tar"
        return 1
    fi

    local size=$(stat -c%s "$tmp_tar" 2>/dev/null || stat -f%z "$tmp_tar" 2>/dev/null || echo 0)
    echo -e "${GREEN}[download] Downloaded $((size / 1024 / 1024)) MB${NC}"

    # Verify the file is a valid xz/tar archive
    echo -e "${YELLOW}[verify] Checking file integrity...${NC}"
    if command -v file >/dev/null 2>&1; then
        local ftype=$(file "$tmp_tar")
        if echo "$ftype" | grep -qi "xz compressed\|tar archive"; then
            echo -e "${GREEN}[verify] ✓ Valid compressed archive${NC}"
        else
            echo -e "${RED}[verify] ✗ File doesn't look like a valid archive${NC}"
            echo "  File type: $ftype"
            echo "  The download may be corrupted or you got an HTML error page."
            rm -f "$tmp_tar"
            return 1
        fi
    fi

    echo -e "${YELLOW}[extract] Extracting rootfs to $ROOTFS_DIR...${NC}"
    if ! command -v tar >/dev/null 2>&1; then
        echo -e "${RED}[error] tar not found${NC}"
        echo "Install tar:"
        echo "  Termux: pkg install tar"
        echo "  Linux:  apt install tar"
        rm -f "$tmp_tar"
        return 1
    fi

    # Extract with verbose progress (show every 100th file to avoid spam)
    if ! tar xf "$tmp_tar" -C "$ROOTFS_DIR" 2>&1; then
        echo -e "${RED}[error] Extraction failed${NC}"
        echo "The downloaded file may be corrupted."
        echo "Try again or use a different distro."
        rm -f "$tmp_tar"
        rm -rf "$ROOTFS_DIR"/*
        return 1
    fi
    rm -f "$tmp_tar"

    if [ ! -d "$ROOTFS_DIR/bin" ]; then
        echo -e "${RED}[error] Extraction failed - no /bin in rootfs${NC}"
        echo "Contents of $ROOTFS_DIR:"
        ls -la "$ROOTFS_DIR" 2>&1 | head -20
        return 1
    fi

    echo -e "${GREEN}[extract] ✓ Rootfs ready at $ROOTFS_DIR${NC}"

    setup_rootfs_integration
    return 0
}

setup_rootfs_integration() {
    echo -e "${YELLOW}[setup] Integrating XPU into rootfs...${NC}"

    cat > "$ROOTFS_DIR/etc/xpu-release" << EOF
XPU_LINUX=1
XPU_VERSION=1.6.0
XPU_INTEGRATED=1
XPU_HOST_KERNEL=$(uname -r)
XPU_HOST_ARCH=$(uname -m)
EOF

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

    mkdir -p "$ROOTFS_DIR/usr/local/bin"

    cat > "$ROOTFS_DIR/usr/local/bin/xpu-info" << 'EOF'
#!/bin/bash
echo "XPU-Linux Integration Info"
echo "=========================="
echo "XPU version    : 1.6.0"
echo "Rootfs path    : (this environment)"
echo "Host kernel    : $(uname -r)"
echo "Architecture   : $(uname -m)"
if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo "Distribution   : ${PRETTY_NAME:-unknown}"
fi
echo "Shell          : $SHELL"
echo "User           : $(whoami)"
echo ""
echo "This is a REAL Linux environment. Not a simulation."
if command -v apt >/dev/null 2>&1; then
    echo "Package manager: apt"
elif command -v apk >/dev/null 2>&1; then
    echo "Package manager: apk"
fi
EOF
    chmod +x "$ROOTFS_DIR/usr/local/bin/xpu-info"

    cat > "$ROOTFS_DIR/usr/local/bin/xpu-shell" << 'EOF'
#!/bin/bash
export PS1='\[\033[1;31m\]Kernel\[\033[0m\]@\[\033[1;34m\]xpu\[\033[0m\]:\[\033[1;32m\]\w\[\033[0m\]\$ '
exec bash "$@"
EOF
    chmod +x "$ROOTFS_DIR/usr/local/bin/xpu-shell"

    # For Ubuntu/Debian: set up sources.list if missing
    if [ -f "$ROOTFS_DIR/etc/apt/sources.list" ] && [ ! -s "$ROOTFS_DIR/etc/apt/sources.list" ]; then
        local arch=$(detect_arch)
        local apt_arch="$arch"
        [ "$arch" = "arm64" ] && apt_arch="arm64"
        [ "$arch" = "armhf" ] && apt_arch="armhf"
        [ "$arch" = "amd64" ] && apt_arch="amd64"

        if [ -f "$ROOTFS_DIR/etc/debian_version" ]; then
            cat > "$ROOTFS_DIR/etc/apt/sources.list" << APT_EOF
deb http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse
APT_EOF
        fi
    fi

    # For Alpine: ensure repositories are set
    if [ -f "$ROOTFS_DIR/etc/apk/repositories" ] && [ ! -s "$ROOTFS_DIR/etc/apk/repositories" ]; then
        cat > "$ROOTFS_DIR/etc/apk/repositories" << ALPINE_EOF
https://dl-cdn.alpinelinux.org/alpine/v3.19/main
https://dl-cdn.alpinelinux.org/alpine/v3.19/community
ALPINE_EOF
    fi

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
    echo ""
    echo -e "  Architecture: $(detect_arch) ($(uname -m))"
    echo -e "  Temp dir:     $(detect_tmp_dir)"
}

main() {
    echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║   XPU Linux Installer - REAL Linux for XPU        ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
    echo ""

    # Show environment info for debugging
    echo -e "${YELLOW}[env] Home: $HOME${NC}"
    echo -e "${YELLOW}[env] Arch: $(uname -m) ($(detect_arch))${NC}"
    echo -e "${YELLOW}[env] Temp: $(detect_tmp_dir)${NC}"
    echo ""

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
            echo ""
            echo "Detected architecture: $(detect_arch) ($(uname -m))"
            echo "Temp directory:         $(detect_tmp_dir)"
            exit 1
            ;;
    esac
}

main "$@"
