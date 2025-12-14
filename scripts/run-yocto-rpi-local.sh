#!/usr/bin/env bash
# Cross-build for Raspberry Pi using a Yocto SDK (local machine).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Path to the Yocto environment-setup script for your Pi SDK.
SDK_ENV="${SDK_ENV:-/opt/yocto/poky/5.0.8/environment-setup-cortexa72-poky-linux}"

# CMake toolchain file in this repo.
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-${ROOT_DIR}/cmake/toolchains/rpi-yocto.cmake}"

# Build settings.
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/rpi-yocto}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Unix Makefiles}"

if [[ ! -f "${SDK_ENV}" ]]; then
    echo "Yocto SDK env file not found: ${SDK_ENV}"
    echo "Set SDK_ENV to your environment-setup-* script."
    exit 1
fi

if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
    echo "Toolchain file not found: ${TOOLCHAIN_FILE}"
    exit 1
fi

echo "Sourcing Yocto SDK: ${SDK_ENV}"
source "${SDK_ENV}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "Configuring with toolchain=${TOOLCHAIN_FILE} (type=${BUILD_TYPE})..."
cmake -G "${GENERATOR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
      "${ROOT_DIR}"

CORES="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "Building with ${CORES} threads..."
cmake --build . --parallel "${CORES}"

echo "Done. Binary: ${BUILD_DIR}/pipeline_cli"
