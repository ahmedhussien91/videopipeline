#!/usr/bin/env bash
# Build the pipeline on a Raspberry Pi (or any host with Pi sysroot/libcamera installed).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build/rpi-host"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Unix Makefiles}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "Configuring (type=${BUILD_TYPE})..."
CMAKE_ARGS=("-G" "${GENERATOR}" "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
if [[ -n "${TOOLCHAIN_FILE}" ]]; then
    CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
fi
cmake "${CMAKE_ARGS[@]}" "${ROOT_DIR}"

CORES="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "Building with ${CORES} threads..."
cmake --build . --parallel "${CORES}"

echo "Build complete: ${BUILD_DIR}/pipeline_cli"
