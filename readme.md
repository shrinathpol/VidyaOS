# VidyaOS — Build & Run Guide

VidyaOS has two runnable components:

| Component | Description | Language |
|-----------|-------------|----------|
| `vidyaos-desktop` | Native CLI shell simulator | C++ |
| `web_vidyaos/` | Modern web desktop GUI (browser) | React + TypeScript |

---

## 1. Native C++ Shell (`vidyaos-desktop`)

### Prerequisites
```bash
sudo apt install g++ make libdrm-dev libgbm-dev
```

### Build
```bash
make -f Makefile.standalone
```

### Run
```bash
./vidyaos-desktop
```

> Default login: username `shri`, password `vidya123`

### Kill & Restart
```bash
pkill -f vidyaos-desktop ; ./vidyaos-desktop
```

---

## 2. Web Desktop GUI (`web_vidyaos/`)

### Prerequisites — Node.js v18+
```bash
# If using local Node.js install:
export PATH=/home/shrinathpol/programs/node-v22.11.0-linux-x64/bin:$PATH

# Or install via system package:
sudo apt install nodejs npm
```

### Install dependencies (first time only)
```bash
cd web_vidyaos
npm install
```

### Run development server (hot-reload)
```bash
cd web_vidyaos
npm run dev
```
Then open **http://localhost:5173** in your browser.

### Build for production
```bash
cd web_vidyaos
npm run build
npm run preview    # serves at http://localhost:4173
```

---

## 3. Zephyr / RTOS Target (Optional)

### Build
```bash
west build -p always -b native_sim
```

### Run
```bash
./build/EmbeddedSystem/zephyr/zephyr.exe
```

---

## Quick Reference

```bash
# Native shell
make -f Makefile.standalone && ./vidyaos-desktop

# Web GUI (dev mode)
export PATH=~/programs/node-v22.11.0-linux-x64/bin:$PATH
cd web_vidyaos && npm install && npm run dev
```