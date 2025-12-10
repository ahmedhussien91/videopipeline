cmake_minimum_required(VERSION 3.16)

# Yocto cross toolchain for Raspberry Pi (cortexa72)
# Usage:
#   source /opt/yocto/poky/5.0.8/environment-setup-cortexa72-poky-linux
#   cmake -S . -B build/rpi -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/rpi-yocto.cmake

if(NOT DEFINED ENV{SDKTARGETSYSROOT} AND NOT DEFINED ENV{OECORE_TARGET_SYSROOT})
    message(FATAL_ERROR "Yocto environment is not sourced. Run 'source /opt/yocto/poky/5.0.8/environment-setup-cortexa72-poky-linux' first.")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(_YOCTO_SYSROOT $ENV{SDKTARGETSYSROOT})
if(NOT _YOCTO_SYSROOT)
    set(_YOCTO_SYSROOT $ENV{OECORE_TARGET_SYSROOT})
endif()
set(CMAKE_SYSROOT ${_YOCTO_SYSROOT})

# Prefer the Yocto cross compilers on PATH; avoid embedded flags from env vars.
find_program(_C_COMPILER aarch64-poky-linux-gcc PATHS ENV PATH NO_DEFAULT_PATH)
find_program(_CXX_COMPILER aarch64-poky-linux-g++ PATHS ENV PATH NO_DEFAULT_PATH)

if(_C_COMPILER)
    set(CMAKE_C_COMPILER ${_C_COMPILER})
endif()
if(_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER ${_CXX_COMPILER})
endif()

if(DEFINED ENV{AR})
    set(CMAKE_AR $ENV{AR})
endif()

# Respect Yocto sysroot during lookups
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_PREFIX_PATH ${CMAKE_SYSROOT}/usr)

# Propagate pkg-config settings from the SDK
if(DEFINED ENV{PKG_CONFIG_SYSROOT_DIR})
    set(ENV{PKG_CONFIG_SYSROOT_DIR} $ENV{PKG_CONFIG_SYSROOT_DIR})
endif()
if(DEFINED ENV{PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_PATH} $ENV{PKG_CONFIG_PATH})
endif()

# Default kernel headers location for V4L2 or platform code
set(_DEFAULT_KERNEL_HEADERS "/opt/yocto/ycoto-excersise/rpi-build-sysv/workspace/sources/linux-raspberrypi/include")
if(NOT DEFINED KERNEL_HEADERS_DIR AND EXISTS ${_DEFAULT_KERNEL_HEADERS})
    set(KERNEL_HEADERS_DIR ${_DEFAULT_KERNEL_HEADERS} CACHE PATH "Kernel headers path")
endif()
