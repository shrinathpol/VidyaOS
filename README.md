# 🖥️ VidyaOS — Embedded Desktop Operating System Simulator

> A fully featured Linux-inspired OS simulator built in **C++17**, with a hardware telemetry daemon in **Node.js**, support for interactive **Python**, **Java**, **JavaScript**, **C/C++** environments, real-time SDL2 rendering, and dual-target support for **Linux x86-64** and **Zephyr RTOS** — all from a single codebase.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Build & Run — Linux Standalone](#build--run--linux-standalone)
- [Build & Run — Zephyr RTOS](#build--run--zephyr-rtos)
- [Desktop Applications](#desktop-applications)
- [Shell Commands Reference](#shell-commands-reference)
- [GNU Coreutils](#gnu-coreutils)
- [Language Environments](#language-environments)
- [Hardware Telemetry Daemon](#hardware-telemetry-daemon)
- [Mouse & GUI Interaction](#mouse--gui-interaction)
- [Scripted / Automated Mode](#scripted--automated-mode)
- [Supported Board Targets](#supported-board-targets)

---

## Overview

**VidyaOS** is a hybrid embedded operating system simulator that combines:

- A **C++17 kernel core** with a software graphics engine rendering a full desktop directly at **native display resolution** (e.g. 1280×720 or 1920×1080) with anti-aliased TrueType fonts (TTF).
- A **GNU-compatible CLI shell** with virtual filesystem, package manager, and multi-language REPL support.
- A **Node.js orchestrator daemon** that bridges the C++ engine to cloud services and exposes hardware telemetry via a C++ N-API addon.
- An **8-window GUI desktop** with full mouse-driven interaction, a floating Start Menu, window docking / Aero-snapping, tiling layouts, and a centralized taskbar dock.
- A **virtual sensor driver** (`virtual_sensor.c`) compatible with the Zephyr sensor subsystem.
- Dual deployment: runs natively on **Linux x86-64** as a standalone binary *and* on **Zephyr RTOS** (`native_sim` / `nrf9161dk`) with an SDL2 display backend.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        VidyaOS Stack                         │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │          GUI Desktop (Native Framebuffer)            │   │
│  │  Start Menu · 8 Windows · HUD Widget · Taskbar Dock  │   │
│  └───────────────────────┬──────────────────────────────┘   │
│                          │ draw_desktop()                    │
│  ┌───────────────────────▼──────────────────────────────┐   │
│  │          Graphics Engine  (src/graphics.cpp)          │   │
│  │  Alpha Blending · Starfield · Glassmorphism · Font   │   │
│  └───────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐   │
│  │        State & Event Engine  (src/state.cpp)          │   │
│  │  Mouse Hitboxes · Window State · Virtual Filesystem  │   │
│  └───────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐   │
│  │          Shell / CLI Engine  (src/shell.cpp)          │   │
│  │ GNU Utils · APT · Python · Java · Node.js · GCC/G++ │   │
│  └───────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐   │
│  │     Platform Abstraction Layer  (include/platform.h)  │   │
│  │          Linux Native  ←→  Zephyr RTOS               │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │   Node.js Orchestrator Daemon  (vidya_node_daemon/)   │   │
│  │  C++ N-API Addon · Gemini AI · Google OAuth2 · IPC   │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │       Virtual Sensor Driver  (src/virtual_sensor.c)   │   │
│  │     Zephyr Sensor Subsystem · Ambient Temp Channel   │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

---

## Features

### 🖱️ Desktop GUI
- **8-Window Desktop** — Terminal, File Manager, System Monitor, Chrome Browser, Settings, Control Panel, Text Editor, App Store
- **Floating Start Menu** — 8-item launcher with hover highlighting and glassmorphic transparency
- **System HUD Widget** — Live CPU%, RAM (KB), and Temperature overlay rendered directly in the framebuffer
- **Centering Taskbar Dock** — Always-visible bottom dock showing app icons with active/inactive indicators and real-time clock
- **Aero-Snapping & Tiling** — Supports dragging windows to screen borders to snap to halves or quarters, or toggling automatic screen-tiling grid layout
- **Anti-Aliased Vector Fonts** — Implements standard vector TrueType Fonts (TTF) (`DejaVuSans` and `DejaVuSansMono`) for all UI labels, inputs, and terminals
- **Alpha Blending & Shadows** — Concentric drop shadows and glassmorphic translucency for windows
- **Unified Settings Engine** — Manages Wi-Fi, VPN, A/B slots system updates, AppArmor security hardening, accessibility screen reader (TTS) + locales, Bluetooth, and MQTT Smart Home devices
- **Real Web Browser Simulator**: Bookmarks navigation bar, simulated downloads dropdown queue, and a simulated Flathub store that performs flatpak installation packages routing
- **IDE Lite Text Editor**: Side-by-side navigator tree view, syntax coloring, code line-numbering gutter, and multiplexed bottom terminal panes

### 🐧 Linux-Compatible Shell
- Full **GNU coreutils** subset: `pwd`, `cd`, `ls`, `cat`, `echo`, `grep`, `cp`, `mv`, `rm`, `mkdir`, `touch`
- I/O redirection: `echo "text" > file.txt`, `echo "more" >> file.txt`
- **Virtual filesystem** with path resolution and directory hierarchy
- **File write command**: `file write /path content` — creates or overwrites virtual files
- **APT package manager**: `apt install`, `apt remove`, `apt list`
- `neofetch` — Linux-style system info display
- `cmatrix` — Green matrix rain animation in the terminal window (press any key to exit)
- **System lock**: `lock` with configurable passcode stored in state

### 💻 Multi-Language Environments

| Language       | Install Command        | Run / REPL                              |
|----------------|------------------------|-----------------------------------------|
| Python 3       | `apt install python`   | `python` (REPL) / `python script.py`   |
| Java           | `apt install java`     | `javac File.java` / `java ClassName`   |
| JavaScript     | `apt install nodejs`   | `node` (REPL) / `node script.js`       |
| C++            | `apt install g++`      | `g++ file.cpp -o out` / `./out`        |
| C              | `apt install gcc`      | `gcc file.c -o out` / `./out`          |

### 📡 Hardware Telemetry
- Real-time CPU%, RAM (KB), and temperature simulated in the render loop (~30 FPS, updated every second)
- Telemetry exposed inside VidyaOS at `/var/run/telemetry.json`
- `gemini` CLI command — AI-assisted hardware health analysis via the Node.js daemon
- `sensor status` / `sensor log start` / `sensor log stop` — raw sensor readings from the virtual sensor
- Temperature simulation: oscillates between 20–80°C with 3°C steps

### 🔧 Virtual Sensor Driver (Zephyr)
- Custom Zephyr sensor driver (`src/virtual_sensor.c`) that registers as a Zephyr device
- Compatible with `SENSOR_CHAN_AMBIENT_TEMP` channel
- Enabled via device tree overlay: `boards/native_sim.overlay`
- Falls back to a mock software counter when running outside Zephyr

---

## Project Structure

```
EmbeddedSystem/
├── src/
│   ├── main.cpp              # Main loop, SDL2 event handler, telemetry simulation
│   ├── shell.cpp             # CLI shell engine — all command handlers & language REPLs
│   ├── graphics.cpp          # Framebuffer renderer — draw_desktop(), primitives, font
│   ├── state.cpp             # Window state, virtual filesystem, mouse hitbox handler
│   └── virtual_sensor.c      # Zephyr-compatible custom ambient temperature sensor driver
│
├── include/
│   ├── platform.h            # Zephyr ↔ Linux abstraction layer (printk, k_msleep, etc.)
│   ├── state.h               # Shared global state declarations (windows, APT, telemetry…)
│   ├── graphics.h            # Graphics API declarations (draw_pixel, draw_desktop, etc.)
│   ├── shell.h               # Shell API declarations (execute_os_command, shell_thread_entry)
│   └── font.h                # 8×8 bitmap font table
│
├── vidya_node_daemon/         # Node.js orchestrator daemon
│   ├── index.js               # Daemon entry point — loads N-API addon, triggers Gemini check
│   ├── package.json           # npm manifest (node-addon-api, @google/generative-ai)
│   ├── binding.gyp            # node-gyp build config for C++ N-API addon
│   ├── ARCHITECTURE.md        # Daemon architecture blueprint (Mermaid diagrams)
│   ├── test.js                # Daemon integration test runner
│   ├── src/
│   │   └── resolution.cpp     # C++ N-API addon — exposes setResolution() to Node.js
│   └── services/
│       ├── gemini.js           # Google Gemini AI integration (telemetry analysis)
│       └── settings.js         # System settings service (resolution, Google OAuth2 sync)
│
├── boards/                   # Zephyr RTOS board-specific files
│   ├── native_sim.conf        # Extra Kconfig for native_sim (heap: 1 MiB)
│   ├── native_sim.overlay     # DTS overlay — enables virtual sensor & SDL display
│   └── nrf9161dk_nrf9161_ns.overlay  # DTS overlay — UART1 pin config for nRF9161 DK
│
├── dts/
│   └── bindings/              # Custom YAML device-tree bindings for virtual sensor
│
├── test_gui.in               # GUI mouse-interaction regression test script
├── test_telemetry.in         # Telemetry / sensor command test script
├── vidya_os_standalone       # Pre-built Linux x86-64 standalone binary
├── CMakeLists.txt            # Zephyr CMake build system (C++17, all 5 source files)
├── prj.conf                  # Zephyr Kconfig — GPIO, SERIAL, DISPLAY, SDL, C++17, SENSOR
└── README.md                 # This file
```

---

## Prerequisites

### Linux Standalone

```bash
# C++17 compiler + POSIX threads + SDL2 display library
sudo apt install g++ libsdl2-dev
```

### Zephyr RTOS Build

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) installed
- `west` tool: `pip install west`
- Nordic nRF Connect SDK v3.3.1+ (for nRF9161 DK target)

### Node.js Daemon (optional)

```bash
cd vidya_node_daemon
npm install          # Installs node-addon-api and @google/generative-ai
node-gyp rebuild     # Compiles the C++ resolution.node N-API addon
```

---

## Build & Run — Linux Standalone

### Build Standalone
To compile the standalone binary (which compiles local static `libvterm`, PTY integration, sandboxed filesystem layers, and graphics), run:

```bash
make -f Makefile.standalone
```

### Run Standalone
Launch the desktop compositor:

```bash
./vidyaos-desktop
```

You'll see the boot animation, then a `VidyaOS>` prompt:

```
Booting Vidya OS Kernel...
[  OK  ] Created slice User Application Slice.
[  OK  ] Started Dispatch Password Requests to Console Directory.
[  OK  ] Reached target Local File Systems.
[  OK  ] Started udev Coldplug all Devices.
[  OK  ] Started Set Up Additional Trust Store Certificates.
[  OK  ] Started System Logging Service.
[  OK  ] Started Accounts Service.
[  OK  ] Reached target Multi-User System.
[  OK  ] Reached target Graphical Interface.
Loading Desktop Environment Shell...

VidyaOS>
```

The SDL2 window opens simultaneously. Shell commands can be typed in **both** the SDL2 terminal window and the host terminal.

### Run in Script Mode

```bash
./vidyaos-desktop < test_gui.in
./vidyaos-desktop < test_telemetry.in
```

---

## Build & Run — Phase 6: Bootable Linux Desktop OS (Native Target)

VidyaOS can be built as a self-hosted bootable Linux operating system using the included Buildroot configurations. In this target mode, the Linux kernel boots directly to the KMS/DRM display framebuffer rendering the C++ desktop compositor without any X11 or Wayland middleware.

### 📦 Buildroot Configurations

The repository contains pre-configured Buildroot integration files:
- **Target defconfig**: `buildroot/configs/vidyaos_defconfig` (configures kernel, target packages, PipeWire, BlueZ, NetworkManager, RAUC, Flatpak, and GRUB bootloader).
- **Custom package**: `buildroot/package/vidyaos-native/` (`Config.in` and `vidyaos-native.mk` compile the local C++ codebase using the `NATIVE=1` standalone target configuration).
- **Overlay & Init scripts**: `buildroot/board/vidyaos/rootfs_overlay/etc/init.d/S99vidyaos` starts the desktop environment binary `/usr/bin/vidyaos-native` and routes its serial interface directly to `/dev/ttyS0`.

### 🛠️ Building the Bootable OS Image

To build the complete bootable disk image (`sdcard.img` or `rootfs.ext4`):
1. Download and extract [Buildroot 2024.02 LTS](https://buildroot.org/download.html) (or matching version).
2. Copy the `buildroot` subdirectory overlays from this repository into your Buildroot directory.
3. Load the defconfig and trigger the build:
   ```bash
   make vidyaos_defconfig
   make
   ```
4. Build outputs will be generated in `output/images/rootfs.ext4` and `output/images/bzImage`.

### 💾 Flashing to a USB Drive / Target Disk

Use the provided system deployment installer script to partition, format, and flash target block devices (e.g. `/dev/sdb`):
```bash
sudo ./scripts/install_usb.sh /dev/sdb
```
The installer script formats the drive with:
- **Partition 1 (`/boot`)**: Standard boot partition (50MB, ext4) containing `bzImage` and GRUB2 bootloader.
- **Partition 2 (`rootfs_a`)**: Slot A primary partition containing `rootfs.ext4`.
- **Partition 3 (`rootfs_b`)**: Slot B backup partition containing `rootfs.ext4`.
- **Partition 4 (`data`)**: Persistent configurations and app storage containing `settings.json`.

### 🔄 Dual-Slot A/B Update Testing with QEMU

To test the RAUC update system locally in a virtual machine:
```bash
qemu-system-x86_64 \
    -m 2G \
    -smp 2 \
    -drive file=/dev/sdb,format=raw \
    -serial stdio \
    -net nic -net user
```
When running natively, changing settings (Wi-Fi, Bluetooth, language locale, VPN, A/B Updates, AppArmor hardening) dynamically invokes the platform services on the guest OS (`nmcli`, `bluetoothctl`, `localectl`, `rauc`, and `espeak`).

---

## Build & Run — Zephyr RTOS

### native_sim target (recommended for development)

```bash
export ZEPHYR_BASE=~/ncs/v3.3.1/zephyr

# Clean build
west build -p always -b native_sim .

# Run
./build/EmbeddedSystem/zephyr/zephyr.exe
```

`boards/native_sim.conf` sets `CONFIG_HEAP_MEM_POOL_SIZE=1048576` (1 MiB) for the STL.  
`boards/native_sim.overlay` enables the SDL display driver and virtual sensor node.

### nRF9161 DK target

```bash
west build -p always -b nrf9161dk_nrf9161_ns .
west flash
```

`boards/nrf9161dk_nrf9161_ns.overlay` configures UART1 on pins TX=P0.14 / RX=P0.15 at 115200 baud.

### Zephyr Kconfig Summary (`prj.conf`)

| Config Key                    | Value            | Purpose                                      |
|-------------------------------|------------------|----------------------------------------------|
| `CONFIG_GPIO`                 | `y`              | Hardware GPIO subsystem                      |
| `CONFIG_SERIAL`               | `y`              | Hardware serial console                      |
| `CONFIG_CONSOLE`              | `y`              | Console peripheral                           |
| `CONFIG_DISPLAY`              | `y`              | Display subsystem                            |
| `CONFIG_SDL_DISPLAY`          | `y`              | SDL2 display driver (for native_sim)         |
| `CONFIG_CPP`                  | `y`              | C++ support                                  |
| `CONFIG_STD_CPP17`            | `y`              | C++17 standard library                       |
| `CONFIG_REQUIRES_FULL_LIBCPP` | `y`              | Full STL on host compiler (native_sim)       |
| `CONFIG_SENSOR`               | `y`              | Zephyr sensor subsystem                      |
| `CONFIG_HEAP_MEM_POOL_SIZE`   | `16384`          | System heap (overridden to 1 MiB in native_sim.conf) |
| `CONFIG_REBOOT`               | `y`              | Reboot / reset API                           |
| `CONFIG_BOOT_BANNER_STRING`   | `"Booting Vidya OS"` | Custom boot banner                       |
| `CONFIG_CONSOLE_SUBSYS`       | `y`              | Portable console subsystem                   |
| `CONFIG_CONSOLE_GETLINE`      | `y`              | Console `getline` utilities                  |
| `CONFIG_DEBUG`                | `y`              | GDB debugging hooks                          |
| `CONFIG_DEBUG_OPTIMIZATIONS`  | `y`              | Disable heavy compiler optimization          |

---

## Desktop Applications

| App            | Open via Shell Command                      | Open via Mouse                            |
|----------------|---------------------------------------------|-------------------------------------------|
| Terminal       | `desktop open terminal`                     | Double-click Terminal icon (top-left)     |
| File Manager   | `desktop open files`                        | Double-click Files icon                   |
| System Monitor | `desktop open monitor`                      | Double-click Monitor icon                 |
| Chrome Browser | `apt install chrome` → `desktop open chrome`| Double-click Chrome icon (after install)  |
| Settings       | `desktop open settings`                     | Start Menu → Settings                     |
| Control Panel  | `desktop open controlpanel`                 | Start Menu → Ctrl Panel                   |
| Text Editor    | `desktop open editor`                       | Start Menu → Text Editor                  |
| App Store      | `desktop open appstore`                     | Start Menu → App Store                    |

### Start Menu

Click the **Vidya Menu** button (bottom-left taskbar) or use:
```bash
mouse move 50 225
mouse click
```

Menu items (8 total):
1. Terminal
2. Files
3. Monitor
4. Browser *(only if Chrome installed)*
5. Settings
6. Ctrl Panel
7. Lock System
8. Shutdown

---

## Shell Commands Reference

```
System Controls:
  lock               Lock the console (requires passcode to unlock)
  sleep              Simulate system sleep
  reset              Reboot the OS simulator
  shutdown           Exit the simulator

Desktop & Mouse:
  desktop list                List status of all 8 desktop windows
  desktop open <app>          Open a window
  desktop close <app>         Close a window
    <app> = terminal | files | monitor | chrome | settings | controlpanel | editor | appstore
  mouse move <x> <y>          Move virtual cursor to (x, y) in high-resolution coordinates
  mouse click                 Click at current cursor position
  mouse status                Print current cursor coordinates

Hardware & Sensor:
  sensor status               Read current virtual sensor value
  sensor log start            Enable periodic sensor printing (~1 Hz)
  sensor log stop             Disable periodic sensor logging

Device Manager:
  device list                 List all registered virtual devices
  device status <name>        Check if a device is ready

Package Manager:
  apt list                    List all available packages and install status
  apt install <pkg>           Install a package
  apt remove <pkg>            Remove a package
    Available: chrome, neofetch, cmatrix, python, java, g++, gcc, nodejs

File Operations:
  file write <path> <content> Write content to a virtual file (creates if missing)

System Info:
  neofetch                    Display Linux-style system information
  gemini                      AI hardware analysis via Gemini (uses live telemetry)

Memory Footprint Change Tracker:
  footprint snapshot          Build a fresh footprint snapshot and store it in memory
  footprint diff              Compare live filesystem against stored snapshot
  footprint update [path]     Incrementally sync specified path (or all) to snapshot

Maintenance:
  upgrade                     Mock OS firmware upgrade (reboots)
  help                        Show full command reference
```

---

## GNU Coreutils

```bash
pwd                          # Print working directory
cd /path/to/dir              # Change directory
ls                           # List files in current directory
ls -l                        # Long format listing with sizes
ls -a                        # Include hidden files (dot-files)
cat file.txt                 # Print file contents
cat file1 file2              # Concatenate and print multiple files
echo "text"                  # Print text to terminal
echo "text" > file.txt       # Write to file (overwrite)
echo "more" >> file.txt      # Append to file
grep "pattern" file.txt      # Search for pattern in file
grep -i "pattern" file.txt   # Case-insensitive search
mkdir /newdir                # Create directory
mkdir -p /a/b/c              # Create nested directories
touch /newfile.txt           # Create empty file
cp /src /dest                # Copy file
cp -r /srcdir /destdir       # Copy directory recursively
mv /old /new                 # Move or rename
rm /file.txt                 # Delete file
rm -r /dir                   # Delete directory recursively
```

---

## Language Environments

### Python 3 REPL

```bash
apt install python
python
>>> x = 42
>>> print(x * 2)
84
>>> exit()
```

### Run a Python Script

```bash
file write /hello.py "print('Hello from VidyaOS!')"
python /hello.py
```

### JavaScript / Node.js REPL

```bash
apt install nodejs
node
> let x = 10
> console.log(x + 5)
15
> .exit
```

### Run a Node.js Script

```bash
file write /app.js "console.log('Running on VidyaOS!');"
node /app.js
```

### Java

```bash
apt install java
file write /Hello.java "class Hello { public static void main(String[] a) { System.out.println(\"Hi\"); } }"
javac /Hello.java
java Hello
```

### C++ Compilation

```bash
apt install g++
file write /test.cpp "#include<stdio.h>\nint main(){printf(\"Hi!\");return 0;}"
g++ /test.cpp -o /test
/test
```

### C Compilation

```bash
apt install gcc
file write /hello.c "#include<stdio.h>\nint main(){printf(\"Hello C!\");return 0;}"
gcc /hello.c -o /hello
/hello
```

---

## Hardware Telemetry Daemon

The Node.js daemon (`vidya_node_daemon/`) runs as a background service bridging the C++ engine to cloud services.

### Architecture

```
Tauri / HTML5 UI  ──IPC/WebSocket──►  Node.js Orchestrator (index.js)
                                            │
                              ┌─────────────┼──────────────────┐
                              ▼             ▼                  ▼
                       C++ N-API      Gemini AI Core     Google OAuth2
                      (resolution    (services/         (services/
                        .node)        gemini.js)         settings.js)
```

### Components

| File | Role |
|------|------|
| `index.js` | Orchestrator entry point; loads N-API addon and triggers Gemini analysis on startup |
| `src/resolution.cpp` | C++ N-API addon — exposes `setResolution(w, h)` to Node.js with zero IPC overhead |
| `services/gemini.js` | Uses `@google/generative-ai`; fetches live telemetry from C++ backend and calls Gemini LLM |
| `services/settings.js` | Reads/writes `settings.json`; manages Google OAuth2 flow (Express.js callback at `localhost:3000`) |
| `binding.gyp` | `node-gyp` build descriptor for the native C++ addon |
| `test.js` | Integration test that exercises the daemon's IPC and Gemini paths |
| `ARCHITECTURE.md` | Mermaid architecture diagram and component descriptions |

### Telemetry Data

Live telemetry is available inside VidyaOS at `/var/run/telemetry.json` (updated every ~1 second):

```bash
cat /var/run/telemetry.json
# → {"cpu": 34, "ram": 1024, "temp": 42}
```

Use the `gemini` command for AI-powered analysis:

```bash
gemini
# → Gemini CLI Assistant:
# → Analyzing telemetry data: {"cpu": 34, "ram": 1024, "temp": 42}
# → Insight: Your hardware looks healthy...
```

---

## Mouse & GUI Interaction

The system renders directly at **native display resolution** (e.g. 1280×720 or 1920×1080). Screen coordinates mapped by mouse commands are relative to the physical window dimensions.

```bash
mouse move <x> <y>    # Move virtual mouse cursor to (x, y)
mouse click           # Fires handle_desktop_click(x, y)
mouse status          # Shows current (x, y)
```

### Common Click Coordinates (assuming 1280x720 window resolution)

| Action                      | x   | y   |
|-----------------------------|-----|-----|
| Open Start Menu             | 50  | 700 |
| Start Menu → Terminal       | 50  | 330 |
| Start Menu → Files          | 50  | 360 |
| Start Menu → Monitor        | 50  | 390 |
| Start Menu → Browser        | 50  | 420 |
| Start Menu → Settings       | 50  | 450 |
| Start Menu → Control Panel  | 50  | 480 |
| Start Menu → Lock           | 50  | 510 |
| Start Menu → Shutdown       | 50  | 540 |

---

## Scripted / Automated Mode

Create a `.in` file and pipe it to the binary:

```bash
cat << 'EOF' > my_session.in
apt install chrome
desktop open settings
desktop open controlpanel
desktop list
mouse move 50 700
mouse click
mouse move 50 420
mouse click
desktop list
sensor status
gemini
shutdown
EOF

./vidyaos-desktop < my_session.in
```

Included test scripts:

| File | Purpose |
|------|---------|
| `test_gui.in` | Mouse-driven GUI regression test (opens windows, navigates menus, closes) |
| `test_telemetry.in` | Tests `sensor status`, `sensor log start/stop`, and `gemini` commands |

---

## Supported Board Targets

| Target | Board String | Notes |
|--------|-------------|-------|
| Linux x86-64 (standalone) | — | Direct `g++` compilation, SDL2 window, no Zephyr needed |
| Zephyr native_sim | `native_sim` | Recommended for development; SDL2 window on host desktop |
| Nordic nRF9161 DK | `nrf9161dk_nrf9161_ns` | UART1 on P0.14/P0.15; requires nRF Connect SDK |

---

## License

Built for educational and embedded systems research purposes.  
VidyaOS — *"Vidya" (विद्या) means knowledge in Sanskrit.*
