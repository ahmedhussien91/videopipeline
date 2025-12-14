#!/usr/bin/env bash
# Unified helper for building and running the video pipeline.
#
# Subcommands:
#   build native            Build on the current host.
#   build yocto             Cross-build for Raspberry Pi using a Yocto SDK.
#   run local               Run locally with a given config/binary.
#   run remote              Deploy and run on a remote Pi over SSH.
#
# Examples:
#   ./scripts/vp.sh build native --build-dir build/native
#   ./scripts/vp.sh build yocto --sdk-env /opt/yocto/poky/5.0.8/environment-setup-cortexa72-poky-linux
#   ./scripts/vp.sh run local --config examples/test_pattern_to_file.yaml
#   ./scripts/vp.sh run remote --ip 192.168.1.101 --user pi --sink-host 192.168.1.11

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_TOOLCHAIN="${ROOT_DIR}/cmake/toolchains/rpi-yocto.cmake"

err() { echo "Error: $*" >&2; exit 1; }

usage() {
    cat <<EOF
Usage: $0 <command> <subcommand> [options]

Commands:
  build native   [--build-dir DIR] [--build-type TYPE] [--generator GEN]
  build yocto    --sdk-env PATH [--toolchain-file FILE] [--build-dir DIR] [--build-type TYPE] [--generator GEN]
  run local      [--bin PATH] [--config FILE] [--sink-host HOST] [--port PORT] [--time SECS] [--no-stats]
  run remote     --ip IP --user USER [--bin PATH] [--config FILE] [--sink-host HOST] [--port PORT] [--time SECS] [--no-stats] [--target-dir DIR]

Common flags:
  --dry-run        Print commands without executing.

Defaults:
  build-type: Release
  generator:   Unix Makefiles
  native build dir:   build/native
  yocto build dir:    build/rpi-yocto
  local bin:          build/native/pipeline_cli
  remote bin:         build/rpi-yocto/pipeline_cli
  default config:     examples/libcamera_to_tcp.conf
EOF
}

dry_run=0
build_type="Release"
generator="Unix Makefiles"

command="${1:-}"
subcommand="${2:-}"
shift || true
shift || true

[[ -z "${command}" || -z "${subcommand}" ]] && { usage; exit 1; }

rewrite_config() {
    local cfg_in="$1" cfg_out="$2" host="$3" port="$4"
    cp "${cfg_in}" "${cfg_out}"
    if [[ -n "${host}" ]]; then
        sed -E -i "s/^(host=).*/\\1${host}/" "${cfg_out}" || true
        sed -E -i "s/^( *host:).*/\\1 \"${host}\"/" "${cfg_out}" || true
    fi
    if [[ -n "${port}" ]]; then
        sed -E -i "s/^(port=).*/\\1${port}/" "${cfg_out}" || true
        sed -E -i "s/^( *port:).*/\\1 \"${port}\"/" "${cfg_out}" || true
    fi
}

run_cmd() {
    if [[ "${dry_run}" == "1" ]]; then
        printf '[DRY] %s\n' "$*"
    else
        eval "$@"
    fi
}

case "${command}:${subcommand}" in
    build:native)
        build_dir="${build_dir:-${ROOT_DIR}/build/native}"
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --build-dir) build_dir="$2"; shift 2 ;;
                --build-type) build_type="$2"; shift 2 ;;
                --generator) generator="$2"; shift 2 ;;
                --dry-run) dry_run=1; shift ;;
                *) err "Unknown option: $1" ;;
            esac
        done
        run_cmd "mkdir -p \"${build_dir}\""
        run_cmd "cd \"${build_dir}\" && cmake -G \"${generator}\" -DCMAKE_BUILD_TYPE=\"${build_type}\" \"${ROOT_DIR}\""
        cores="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
        run_cmd "cd \"${build_dir}\" && cmake --build . --parallel \"${cores}\""
        echo "Native build ready at ${build_dir}/pipeline_cli"
        ;;

    build:yocto)
        sdk_env=""
        toolchain_file="${DEFAULT_TOOLCHAIN}"
        build_dir="${ROOT_DIR}/build/rpi-yocto"
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --sdk-env) sdk_env="$2"; shift 2 ;;
                --toolchain-file) toolchain_file="$2"; shift 2 ;;
                --build-dir) build_dir="$2"; shift 2 ;;
                --build-type) build_type="$2"; shift 2 ;;
                --generator) generator="$2"; shift 2 ;;
                --dry-run) dry_run=1; shift ;;
                *) err "Unknown option: $1" ;;
            esac
        done
        [[ -z "${sdk_env}" ]] && err "--sdk-env is required for Yocto build"
        [[ ! -f "${sdk_env}" ]] && err "SDK env file not found: ${sdk_env}"
        [[ ! -f "${toolchain_file}" ]] && err "Toolchain file not found: ${toolchain_file}"
        run_cmd "source \"${sdk_env}\""
        run_cmd "mkdir -p \"${build_dir}\""
        run_cmd "cd \"${build_dir}\" && cmake -G \"${generator}\" -DCMAKE_BUILD_TYPE=\"${build_type}\" -DCMAKE_TOOLCHAIN_FILE=\"${toolchain_file}\" \"${ROOT_DIR}\""
        cores="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
        run_cmd "cd \"${build_dir}\" && cmake --build . --parallel \"${cores}\""
        echo "Yocto build ready at ${build_dir}/pipeline_cli"
        ;;

    run:local)
        pipeline_bin="${ROOT_DIR}/build/native/pipeline_cli"
        config="${ROOT_DIR}/examples/libcamera_to_tcp.conf"
        sink_host=""
        port=""
        time_limit="5"
        stats=1
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --bin) pipeline_bin="$2"; shift 2 ;;
                --config) config="$2"; shift 2 ;;
                --sink-host) sink_host="$2"; shift 2 ;;
                --port) port="$2"; shift 2 ;;
                --time) time_limit="$2"; shift 2 ;;
                --no-stats) stats=0; shift ;;
                --dry-run) dry_run=1; shift ;;
                *) err "Unknown option: $1" ;;
            esac
        done
        [[ ! -x "${pipeline_bin}" ]] && err "Binary not executable: ${pipeline_bin}"
        [[ ! -f "${config}" ]] && err "Config not found: ${config}"
        cfg_use="${config}"
        tmp_cfg=""
        if [[ -n "${sink_host}" || -n "${port}" ]]; then
            tmp_cfg="$(mktemp /tmp/pipeline_cfg.XXXX.conf)"
            rewrite_config "${config}" "${tmp_cfg}" "${sink_host}" "${port}"
            cfg_use="${tmp_cfg}"
        fi
        cmd="\"${pipeline_bin}\" --config \"${cfg_use}\" --time ${time_limit}"
        [[ "${stats}" == "1" ]] && cmd+=" --stats"
        run_cmd "${cmd}"
        if [[ -n "${tmp_cfg}" ]]; then
            rm -f "${tmp_cfg}"
        fi
        ;;

    run:remote)
        target_ip=""
        target_user="pi"
        target_dir="/home/${target_user}/cameraCapture"
        pipeline_bin="${ROOT_DIR}/build/rpi-yocto/pipeline_cli"
        config_template="${ROOT_DIR}/examples/libcamera_to_tcp.conf"
        sink_host=""
        port="5000"
        time_limit="5"
        stats=1
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --ip) target_ip="$2"; shift 2 ;;
                --user) target_user="$2"; target_dir="/home/${target_user}/cameraCapture"; shift 2 ;;
                --target-dir) target_dir="$2"; shift 2 ;;
                --bin) pipeline_bin="$2"; shift 2 ;;
                --config) config_template="$2"; shift 2 ;;
                --sink-host) sink_host="$2"; shift 2 ;;
                --port) port="$2"; shift 2 ;;
                --time) time_limit="$2"; shift 2 ;;
                --no-stats) stats=0; shift ;;
                --dry-run) dry_run=1; shift ;;
                *) err "Unknown option: $1" ;;
            esac
        done
        [[ -z "${target_ip}" ]] && err "--ip is required for remote run"
        [[ ! -x "${pipeline_bin}" ]] && err "Binary not executable: ${pipeline_bin}"
        [[ ! -f "${config_template}" ]] && err "Config not found: ${config_template}"

        if [[ -z "${sink_host}" ]]; then
            host_candidate="$({ hostname -I 2>/dev/null || true; } | awk 'NF{print $1; exit}')"
            sink_host="${host_candidate:-127.0.0.1}"
        fi

        tmp_cfg="$(mktemp /tmp/libcamera_to_tcp.XXXX.conf)"
        rewrite_config "${config_template}" "${tmp_cfg}" "${sink_host}" "${port}"

        echo "Deploying to ${target_user}@${target_ip}:${target_dir} (sink host ${sink_host}, port ${port})"
        run_cmd "ssh ${target_user}@${target_ip} 'mkdir -p \"${target_dir}\"'"
        run_cmd "scp \"${pipeline_bin}\" ${target_user}@${target_ip}:\"${target_dir}/pipeline_cli\""
        run_cmd "scp \"${tmp_cfg}\" ${target_user}@${target_ip}:\"${target_dir}/libcamera_to_tcp.conf\""

        cmd="cd \"${target_dir}\" && chmod +x pipeline_cli && ./pipeline_cli --config libcamera_to_tcp.conf --time ${time_limit}"
        [[ "${stats}" == "1" ]] && cmd+=" --stats"
        run_cmd "ssh ${target_user}@${target_ip} '${cmd}'"
        rm -f "${tmp_cfg}"
        ;;

    *)
        usage
        exit 1
        ;;
esac
