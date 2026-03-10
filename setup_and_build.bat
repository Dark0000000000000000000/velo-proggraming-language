#!/bin/bash
# ─── Velo IDE - MSYS2 Setup & Build Script ───────────────────────────────────
# Run from the velo/ project root in MSYS2 MinGW64 shell

set -e
echo "=== ⚡ Velo IDE - Setup & Build ==="

# ── 1. Install dependencies via pacman ──────────────────────────────────────
echo ""
echo ">>> Installing dependencies..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-glfw \
    mingw-w64-x86_64-mesa \
    git

# ── 2. Clone Dear ImGui (vendored) ──────────────────────────────────────────
echo ""
echo ">>> Setting up Dear ImGui..."
mkdir -p vendor
if [ ! -d "vendor/imgui" ]; then
    git clone --depth 1 https://github.com/ocornut/imgui.git vendor/imgui
else
    echo "    imgui already present, skipping."
fi

# ── 3. Configure & Build ────────────────────────────────────────────────────
echo ""
echo ">>> Configuring CMake (Release)..."
mkdir -p build
cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++

echo ""
echo ">>> Building..."
ninja -j$(nproc)

cd ..
echo ""
echo "✅  Build complete!"
echo "    Run:  ./build/velo.exe"
