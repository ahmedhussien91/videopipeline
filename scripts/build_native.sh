#!/usr/bin/env bash
# Build the pipeline on the current Linux host.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build/native"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Unix Makefiles}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "Configuring (type=${BUILD_TYPE})..."
cmake -G "${GENERATOR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${ROOT_DIR}"

CORES="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "Building with ${CORES} threads..."
cmake --build . --parallel "${CORES}"

echo "Build complete: ${BUILD_DIR}/pipeline_cli"
