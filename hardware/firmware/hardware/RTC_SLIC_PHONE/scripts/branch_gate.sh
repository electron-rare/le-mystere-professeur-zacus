#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

DEFAULT_BUILD_ENVS_FULL=(
    "esp32dev"
    "esp32-s3-devkitc-1"
    "esp32-s3-usb-host"
    "esp32-s3-usb-msc"
)
DEFAULT_BUILD_ENVS_A252=(
    "esp32dev"
)
REPORT_JSON="${ARTIFACT_REPORT_PATH:-artifacts/route_parity_report.json}"

BUILD_ENVS=()
SKIP_BUILDS=0
PROFILE="a252"

log() {
    echo "[branch-gate] $*"
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[branch-gate] erreur: commande manquante: $1" >&2
        exit 1
    fi
}

usage() {
    cat <<'EOF'
Usage: scripts/branch_gate.sh [options]

Exécute la chaîne de validation de branche dans un ordre déterministe.

Options:
  --profile <a252|full>     Profil de build par défaut (a252: esp32dev seulement).
  --skip-builds             Ignore la phase de build PlatformIO.
  --build-env <env>         Ajouter explicitement un env PlatformIO (peut se répéter).
  --build-envs <env1,env2>  Ajouter plusieurs envs en une fois (séparés par des virgules).
  --report-json <path>      Emplacement du rapport parity JSON (défaut: artifacts/route_parity_report.json).
  --help                    Affiche cette aide.

Sans --build-env ni --build-envs, la séquence build cible :
  - profile a252: esp32dev
  - profile full: esp32dev, esp32-s3-devkitc-1, esp32-s3-usb-host, esp32-s3-usb-msc
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --profile)
            if [[ $# -lt 2 ]]; then
                usage
                exit 1
            fi
            case "$2" in
                a252|full)
                    PROFILE="$2"
                    ;;
                *)
                    echo "[branch-gate] profile invalide: $2 (attendu: a252|full)" >&2
                    exit 1
                    ;;
            esac
            shift 2
            ;;
        --skip-builds)
            SKIP_BUILDS=1
            shift
            ;;
        --build-env)
            if [[ $# -lt 2 ]]; then
                usage
                exit 1
            fi
            BUILD_ENVS+=("$2")
            shift 2
            ;;
        --build-envs)
            if [[ $# -lt 2 ]]; then
                usage
                exit 1
            fi
            IFS=',' read -r -a extra_envs <<< "$2"
            for env_name in "${extra_envs[@]}"; do
                if [[ -n "${env_name}" ]]; then
                    BUILD_ENVS+=("${env_name}")
                fi
            done
            shift 2
            ;;
        --report-json)
            if [[ $# -lt 2 ]]; then
                usage
                exit 1
            fi
            REPORT_JSON="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[branch-gate] option inconnue: $1" >&2
            usage
            exit 1
            ;;
    esac
done

run_checks() {
    log "vérification syntaxe scripts (python)"
    python3 -m py_compile scripts/hw_validation.py

    log "tests unitaires PlatformIO (esp32dev, mode host)"
    platformio test --without-uploading --without-testing -e esp32dev

    log "tests host DTMF"
    mkdir -p .pio/host
    c++ -std=c++17 -Wall -Wextra -pedantic -Isrc test/host/test_dtmf_host.cpp src/telephony/DtmfDecoder.cpp -o .pio/host/test_dtmf_host
    .pio/host/test_dtmf_host

    log "tests contrat parity/runtime/hw_validation"
    python3 -m unittest \
        scripts/test_check_web_route_parity.py \
        scripts/test_runtime_contracts.py \
        scripts/test_hw_validation_contracts.py

    log "contrôle route/command parity avec rapport JSON"
    mkdir -p "$(dirname "${REPORT_JSON}")"
    python3 scripts/check_web_route_parity.py --report-json "${REPORT_JSON}"
}

run_builds() {
    if (( SKIP_BUILDS )); then
        log "phase build ignorée (--skip-builds)"
        return
    fi

    if (( ${#BUILD_ENVS[@]} == 0 )); then
        if [[ "${PROFILE}" == "full" ]]; then
            BUILD_ENVS=("${DEFAULT_BUILD_ENVS_FULL[@]}")
        else
            BUILD_ENVS=("${DEFAULT_BUILD_ENVS_A252[@]}")
        fi
    fi

    log "profil build actif: ${PROFILE}"
    for env_name in "${BUILD_ENVS[@]}"; do
        log "build PlatformIO: ${env_name}"
        platformio run -e "${env_name}"
    done
}

require_cmd python3
require_cmd platformio
require_cmd c++

run_checks
run_builds

log "validation de branche terminée avec succès"
