cmake_minimum_required(VERSION 3.22)
project(fluttertap)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Capstone (disassembly engine) -- same engine Frida's Instruction.parse uses.
# We only need ARM64 + X86(x86_64) decoding, so every other architecture is
# disabled to keep the final .so small.
# ---------------------------------------------------------------------------
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARM64_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/capstone)

# ---------------------------------------------------------------------------
# Dobby (inline hooking engine) -- used for the two internal, non-exported
# BoringSSL/Flutter engine functions that cannot be reached through PLT hooks.
# ---------------------------------------------------------------------------
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
# We only ever link dobby_static; the shared "dobby" target fails to link on
# Android with this NDK/lld (duplicate C++17 inline-variable symbols) and is
# skipped entirely by a local patch in third_party/dobby/CMakeLists.txt
# (see docs/ARCHITECTURE.md).
add_subdirectory(third_party/dobby)

add_library(fluttertap SHARED
    main.cpp
    module_config.cpp
    companion.cpp
    json_min.cpp
    elf_utils.cpp
    mem_scan.cpp
    addr_resolver.cpp
    hooks.cpp
    log.cpp
)

target_include_directories(fluttertap PRIVATE
    include
    third_party/dobby/include
    third_party/capstone/include
)

target_link_libraries(fluttertap
    dobby_static
    capstone_static
    log
)

target_compile_options(fluttertap PRIVATE -Wall -Wextra -Wno-unused-parameter -fvisibility=hidden)

# Only the zygisk entry points need to be visible.
target_link_options(fluttertap PRIVATE -Wl,--exclude-libs,ALL)
