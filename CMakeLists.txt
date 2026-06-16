cmake_minimum_required(VERSION 3.18)
project(bada VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── LLVM via llvm-config ───────────────────────────────
execute_process(
    COMMAND llvm-config --cxxflags
    OUTPUT_VARIABLE LLVM_CXX_FLAGS_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND llvm-config --ldflags
    OUTPUT_VARIABLE LLVM_LD_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND llvm-config --includedir
    OUTPUT_VARIABLE LLVM_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND llvm-config --libdir
    OUTPUT_VARIABLE LLVM_LIB_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ─── Strip -fno-exceptions ──────────────────────────────
string(REPLACE "-fno-exceptions" "" LLVM_CXX_FLAGS "${LLVM_CXX_FLAGS_RAW}")
string(REPLACE "-fno-rtti" "" LLVM_CXX_FLAGS "${LLVM_CXX_FLAGS}")

# ─── Apply flags ────────────────────────────────────────
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS} -fexceptions")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LD_FLAGS}")

# ─── Sources ────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/error.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/transpiler.cpp
)

# ─── Include ────────────────────────────────────────────
include_directories(include)
include_directories(${LLVM_INCLUDE_DIR})
link_directories(${LLVM_LIB_DIR})

# ─── Executable ─────────────────────────────────────────
add_executable(bada ${SOURCES})

# ─── Link LLVM dynamic ──────────────────────────────────
target_link_libraries(bada PRIVATE LLVM)

# ─── Install ────────────────────────────────────────────
install(TARGETS bada DESTINATION $ENV{PREFIX}/bin)
