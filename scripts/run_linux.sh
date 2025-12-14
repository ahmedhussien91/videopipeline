#!/usr/bin/env bash
# Run the pipeline locally on Linux with optional TCP host/port overrides.
set -euo pipefail

PIPELINE_BIN="${PIPELINE_BIN:-build/native/pipeline_cli}"
CONFIG="${CONFIG:-examples/test_pattern_to_file.yaml}"
SINK_HOST="${SINK_HOST:-}"
PORT="${PORT:-}"
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

if [[ ! -f "${CONFIG}" ]]; then
    echo "Config file not found: ${CONFIG}"
    exit 1
fi

CFG_TO_USE="${CONFIG}"
TMP_CONFIG=""

if [[ -n "${SINK_HOST}" || -n "${PORT}" ]]; then
    TMP_CONFIG="$(mktemp /tmp/pipeline_config.XXXX)"
    trap '[[ -n "${TMP_CONFIG}" ]] && rm -f "${TMP_CONFIG}"' EXIT
    cp "${CONFIG}" "${TMP_CONFIG}"
    CFG_TO_USE="${TMP_CONFIG}"

    if [[ -n "${SINK_HOST}" ]]; then
        # Handle INI/CONF and YAML host fields.
        sed -E -i "s/^(host=).*/\1${SINK_HOST}/" "${CFG_TO_USE}" || true
        sed -E -i "s/^( *host:).*/\1 \"${SINK_HOST}\"/" "${CFG_TO_USE}" || true
    fi
    if [[ -n "${PORT}" ]]; then
        sed -E -i "s/^(port=).*/\1${PORT}/" "${CFG_TO_USE}" || true
        sed -E -i "s/^( *port:).*/\1 \"${PORT}\"/" "${CFG_TO_USE}" || true
    fi
fi

CMD="${PIPELINE_BIN} --config ${CFG_TO_USE} --time ${TIME_LIMIT}"
if [[ "${STATS}" == "true" ]]; then
    CMD+=" --stats"
fi

echo "Running locally with config ${CFG_TO_USE}"
run_cmd "${CMD}"
