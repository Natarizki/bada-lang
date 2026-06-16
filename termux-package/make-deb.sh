#!/bin/bash
set -e

VERSION="1.2.0"
ARCH="aarch64"
PKG_NAME="bada"
PKG_DIR="${PKG_NAME}_${VERSION}_${ARCH}"

echo "Building Bada-Lang v${VERSION} .deb package..."

# ─── Build compiler dulu ────────────────────────────────
cd ~/bada-lang
rm -rf build
cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_DIR=$(llvm-config --cmakedir)
ninja -C build

# ─── Buat struktur .deb ─────────────────────────────────
mkdir -p $TMPDIR/${PKG_DIR}/DEBIAN
mkdir -p $TMPDIR/${PKG_DIR}/data/data/com.termux/files/usr/bin
mkdir -p $TMPDIR/${PKG_DIR}/data/data/com.termux/files/usr/share/doc/bada
mkdir -p $TMPDIR/${PKG_DIR}/data/data/com.termux/files/usr/share/bada/examples

# ─── Copy binary ────────────────────────────────────────
cp build/bada $TMPDIR/${PKG_DIR}/data/data/com.termux/files/usr/bin/bada
chmod 755 $TMPDIR/${PKG_DIR}/data/data/com.termux/files/usr/bin/bada

# ─── Copy examples ──────────────────────────────────────
cp examples/*.bada $TMPDIR/${PKG_DIR}/data/data/com.termux/files/usr/share/bada/examples/

# ─── DEBIAN/control ─────────────────────────────────────
cat > $TMPDIR/${PKG_DIR}/DEBIAN/control << EOF
Package: bada
Version: ${VERSION}
Architecture: aarch64
Maintainer: Natarizki <natarizki@github.com>
Depends: llvm, clang
Description: Bada programming language compiler
 Bada is a hybrid systems programming language combining
 C structure with Rust safety features. Compiles to native
 ARM64 binaries via LLVM IR or C++ transpiler.
 .
 Features:
  - Native ARM64 compilation via LLVM
  - Full C/C++ interop (downgrade system)
  - Inline ASM support (ARM64/x86_64/RISC-V)
  - Rust-style error messages
  - Standard library (bada::io, bada::math, etc)
Homepage: https://github.com/Natarizki/bada-lang
EOF

# ─── DEBIAN/postinst ────────────────────────────────────
cat > $TMPDIR/${PKG_DIR}/DEBIAN/postinst << EOF
#!/bin/bash
echo "Bada-Lang v${VERSION} installed successfully!"
echo "Usage: bada <file.bada> -o <output>"
echo "Docs:  https://github.com/Natarizki/bada-lang"
EOF
chmod 755 $TMPDIR/${PKG_DIR}/DEBIAN/postinst
# Fix DEBIAN directory permissions
chmod 755 $TMPDIR/${PKG_DIR}/DEBIAN
chmod 644 $TMPDIR/${PKG_DIR}/DEBIAN/control
chmod 755 $TMPDIR/${PKG_DIR}/DEBIAN/postinst
# ─── Build .deb ─────────────────────────────────────────
cd $TMPDIR
dpkg-deb --build ${PKG_DIR}
mv ${PKG_DIR}.deb ~/bada-lang/

echo ""
echo "✓ Package built: ~/bada-lang/${PKG_DIR}.deb"
echo ""
echo "Install with:"
echo "  dpkg -i ~/bada-lang/${PKG_DIR}.deb"
echo ""
echo "For Termux apt repo, submit to:"
echo "  https://github.com/termux/termux-packages"
