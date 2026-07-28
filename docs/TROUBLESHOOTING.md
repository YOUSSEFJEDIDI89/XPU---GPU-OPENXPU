# XPU Troubleshooting Guide

Common issues and their solutions.

## Issue: "Failed to open the file /tmp/xpu-rootfs-*.tar.xz"

**Cause**: On Termux (Android), `/tmp/` does not exist. Older versions of the install script hardcoded `/tmp/`.

**Fix**: Update to the latest version:
```bash
git pull
bash scripts/install_linux.sh ubuntu
```

The new version auto-detects a writable temp directory:
1. `$TMPDIR` (if set)
2. `/tmp` (Linux)
3. `$PREFIX/tmp` (Termux default: `/data/data/com.termux/files/usr/tmp`)
4. `~/.cache` (fallback)
5. Current directory (last resort)

---

## Issue: "Download failed" on Termux

**Possible causes and fixes**:

### 1. No internet / GitHub blocked
Test your connection:
```bash
curl -I https://github.com
```
If this fails, try a VPN or use a different network.

### 2. Disk space
Check available space:
```bash
df -h ~
```
You need at least 500MB free for Ubuntu, 50MB for Alpine.

### 3. Old curl/wget
Update them:
```bash
pkg upgrade curl wget
```

### 4. Manual download
If the script keeps failing, download manually:

1. Open this URL in your phone's browser:
   - Alpine: `https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Alpine/arm64/alpine-rootfs-arm64.tar.xz`
   - Ubuntu: `https://github.com/EXALAB/Anlinux-Resources/raw/master/Rootfs/Ubuntu/arm64/ubuntu-rootfs-arm64.tar.xz`

2. Move the downloaded file:
```bash
# The file is usually in /sdcard/Download/
mkdir -p ~/.xpu/rootfs-linux
tar xf /sdcard/Download/alpine-rootfs-arm64.tar.xz -C ~/.xpu/rootfs-linux
```

3. Run the integration setup:
```bash
bash scripts/install_linux.sh alpine
# (it will detect the existing rootfs and set up XPU integration)
```

---

## Issue: "Architecture: armhf" on a 64-bit phone

This is actually correct in some cases. Here's why:

- `uname -m` returns `armv8l` when your phone's **userspace** is 32-bit, even if the kernel is 64-bit
- Some older Android phones (Android 7-9 on certain SoCs) ship with a 32-bit userspace
- The script correctly downloads the `armhf` (32-bit ARM) rootfs in this case

To verify your phone is really 64-bit capable:
```bash
cat /proc/cpuinfo | grep -i "model name\|processor" | head -5
getprop ro.product.cpu.abi
```

If `ro.product.cpu.abi` shows `arm64-v8a`, your phone supports 64-bit. You can still use the `armhf` rootfs - it works fine on 64-bit kernels.

---

## Issue: "proot not found" or "proot: command not found"

Install proot:

**On Termux:**
```bash
pkg install proot
```

**On Debian/Ubuntu:**
```bash
sudo apt install proot
```

**On Fedora:**
```bash
sudo dnf install proot
```

**On Arch:**
```bash
sudo pacman -S proot
```

---

## Issue: "setlocale: LC_ALL: cannot change locale (ar_SA.UTF-8)"

This is a harmless warning, but you can fix it:

```bash
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
```

To make this permanent, add to `~/.bashrc`:
```bash
echo 'export LANG=en_US.UTF-8' >> ~/.bashrc
echo 'export LC_ALL=en_US.UTF-8' >> ~/.bashrc
source ~/.bashrc
```

---

## Issue: "make: Nothing to be done for 'all'"

This is **NOT an error**. It means the project is already built and up to date.

To force a rebuild:
```bash
make clean
make
```

---

## Issue: cmake errors like "does not appear to contain CMakeLists.txt"

You're running `cmake ..` from the wrong directory. CMake requires a separate build directory:

```bash
# CORRECT way:
mkdir build        # create build dir INSIDE the project
cd build           # enter it
cmake ..           # now .. points to the project root with CMakeLists.txt
cmake --build . -j4
```

Or just use Make (simpler):
```bash
make -j4
```

---

## Issue: "xpu: command not found" after install-system

Your shell hasn't re-read the PATH. Either:
- Open a **new terminal**, or
- Run: `source ~/.bashrc` (or `source ~/.zshrc`)

Verify installation:
```bash
which xpu
xpu-info
```

---

## Issue: Render daemon produces black BMP frames

The render daemon saves frames to `render_output/`. If they're all black:

1. Make sure you're using the latest version (older versions had a rasterizer bug)
2. Run with a smaller resolution first:
```bash
LD_LIBRARY_PATH=build ./build/xpu_render_daemon --width 320 --height 240 --max-frames 10 --save-every 1
```
3. Check the BMP file is valid:
```bash
ls -la render_output/
file render_output/frame_00000.bmp
```

---

## Issue: Neural network doesn't learn (XOR accuracy stays at 50%)

This is normal for the first 50-100 epochs. The network needs time to find good weights.

If after 1000 epochs it's still at 50%:
1. Make sure you have the latest code: `git pull && make clean && make`
2. Try increasing the learning rate: edit `samples/nn_train/main.cpp` and change `LR` from `0.05f` to `0.1f`
3. Try more epochs: change `EPOCHS` from `1000` to `2000`

---

## Still stuck?

Open an issue on GitHub:
https://github.com/YOUSSEFJEDIDI89/XPU---GPU-OPENXPU/issues

Include:
- Your phone model (e.g., Samsung Galaxy A10s)
- Android version
- Termux version (`pkg info termux-tools`)
- Output of `uname -a`
- The exact error message
