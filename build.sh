#!/bin/bash
set -e

echo "╔══════════════════════════════════╗"
echo "║     Bada-Lang Compiler Build     ║"
echo "╚══════════════════════════════════╝"

# ─── Check Dependencies ─────────────────────────────────
echo "[1/4] Checking dependencies..."

for cmd in clang++ cmake ninja llvm-config; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: $cmd not found!"
        echo "Run: pkg install clang cmake ninja llvm"
        exit 1
    fi
done

echo "      All dependencies found!"

# ─── Configure ──────────────────────────────────────────
echo "[2/4] Configuring with CMake + Ninja..."

cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_DIR=$(llvm-config --cmakedir)

# ─── Build ──────────────────────────────────────────────
echo "[3/4] Building..."
ninja -C build

# ─── Install ────────────────────────────────────────────
echo "[4/4] Installing to \$PREFIX/bin..."
cp build/bada $PREFIX/bin/bada
chmod +x $PREFIX/bin/bada
# Add current dir to PATH jika belum ada
if ! grep -q 'PATH="$HOME/bada-lang:$PATH"' ~/.bashrc; then
    echo 'export PATH="$HOME/bada-lang:$PATH"' >> ~/.bashrc
    echo "Added ~/bada-lang to PATH"
fi

echo ""
echo "✓ Done! Bada compiler installed."
echo ""
echo "Usage:"
echo "  bada test.bada -o test"
echo "  test"
