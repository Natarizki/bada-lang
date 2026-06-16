TERMUX_PKG_HOMEPAGE="https://github.com/Natarizki/bada-lang"
TERMUX_PKG_DESCRIPTION="Bada programming language compiler - hybrid C with Rust safety"
TERMUX_PKG_LICENSE="MIT"
TERMUX_PKG_MAINTAINER="Natarizki <natarizki@github.com>"
TERMUX_PKG_VERSION="1.2.0"
TERMUX_PKG_SRCURL="https://github.com/Natarizki/bada-lang/archive/refs/tags/v1.2.0.tar.gz"
TERMUX_PKG_SHA256="PLACEHOLDER_SHA256"
TERMUX_PKG_DEPENDS="llvm, clang, cmake, ninja"
TERMUX_PKG_BUILD_DEPENDS="llvm-dev, clang"

termux_step_make() {
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DLLVM_DIR=$(llvm-config --cmakedir) \
        -G Ninja \
        "$TERMUX_PKG_SRCDIR"
    ninja
}

termux_step_make_install() {
    install -Dm755 bada "$TERMUX_PREFIX/bin/bada"
}
