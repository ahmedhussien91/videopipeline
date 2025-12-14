#!/usr/bin/env bash
# Deploy and run the pipeline on a remote Raspberry Pi.
set -euo pipefail

TARGET_IP="${TARGET_IP:-192.168.1.101}"
TARGET_USER="${TARGET_USER:-root}"
TARGET_DIR="${TARGET_DIR:-/home/${TARGET_USER}/cameraCapture}"
PIPELINE_BIN="${PIPELINE_BIN:-build/rpi-host/pipeline_cli}"
CONFIG_TEMPLATE="${CONFIG_TEMPLATE:-examples/libcamera_to_tcp.conf}"

HOST_CANDIDATE=""
if command -v hostname >/dev/null 2>&1; then
    HOST_CANDIDATE="$({ hostname -I 2>/dev/null || true; } | awk 'NF{print $1; exit}')"
fi
SINK_HOST="${SINK_HOST:-${HOST_CANDIDATE:-127.0.0.1}}"
PORT="${PORT:-5000}"
TIME_LIMIT="${TIME_LIMIT:-5}"
STATS="${STATS:-true}"
DRY_RUN="${DRY_RUN:-0}"

run_cmd() {
    if [[ "${DRY_RUN}" == "1" ]]; then
        printf '[DRY] %s\n' "$*"
    else
        eval "$@"
    fi
}

if [[ ! -x "${PIPELINE_BIN}" ]]; then
    echo "Pipeline binary not found or not executable: ${PIPELINE_BIN}"
    exit 1
fi

if [[ ! -f "${CONFIG_TEMPLATE}" ]]; then
    echo "Config template not found: ${CONFIG_TEMPLATE}"
    exit 1
fi

TMP_CONFIG="$(mktemp /tmp/libcamera_to_tcp.XXXX.conf)"
trap 'rm -f "${TMP_CONFIG}"' EXIT
cp "${CONFIG_TEMPLATE}" "${TMP_CONFIG}"
sed -E -i "s/^(host=).*/\1${SINK_HOST}/" "${TMP_CONFIG}"
sed -E -i "s/^(port=).*/\1${PORT}/" "${TMP_CONFIG}"

echo "Using sink host ${SINK_HOST}, port ${PORT}"
echo "Deploying to ${TARGET_USER}@${TARGET_IP}:${TARGET_DIR}"

run_cmd "ssh ${TARGET_USER}@${TARGET_IP} 'mkdir -p ${TARGET_DIR}'"
run_cmd "scp ${PIPELINE_BIN} ${TARGET_USER}@${TARGET_IP}:${TARGET_DIR}/pipeline_cli"
run_cmd "scp ${TMP_CONFIG} ${TARGET_USER}@${TARGET_IP}:${TARGET_DIR}/libcamera_to_tcp.conf"

CMD="cd ${TARGET_DIR} && chmod +x pipeline_cli && ./pipeline_cli --config libcamera_to_tcp.conf --time ${TIME_LIMIT}"
if [[ "${STATS}" == "true" ]]; then
    CMD+=" --stats"
fi

echo "Running remote pipeline..."
run_cmd "ssh ${TARGET_USER}@${TARGET_IP} '${CMD}'"
