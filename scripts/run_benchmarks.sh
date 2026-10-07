#!/usr/bin/env bash
# ============================================================================
# Phase 11 Benchmark Harness
# MDE-HRA Research Project
# ============================================================================
#
# Usage:
#   bash scripts/run_benchmarks.sh [REPETITIONS] [OUTPUT_DIR]
#
# Defaults:
#   REPETITIONS = 10
#   OUTPUT_DIR  = experiments/raw
#
# The harness:
#   1. Runs each benchmark × each configuration × REPETITIONS times
#   2. Writes one CSV file per run batch to OUTPUT_DIR
#   3. Performs per-benchmark correctness validation (B==C semantic check)
#   4. Logs errors to experiments/logs/
#
# Environment:
#   Requires PATH to include D:/msys64/ucrt64/bin for correct toolchain
#   Executable: build/src/instr-driver.exe
#
# Output file naming:
#   experiments/raw/YYYY-MM-DD_HHMMSS_runNN.csv
#   experiments/logs/YYYY-MM-DD_HHMMSS.log
#
# ============================================================================

set -euo pipefail

REPS="${1:-10}"
OUTPUT_DIR="${2:-experiments/raw}"
LOG_DIR="experiments/logs"
BM_DIR="experiments/benchmarks"
DRIVER="build/src/instr-driver.exe"
CONFIGS=("A" "B" "C")

TIMESTAMP=$(date '+%Y-%m-%d_%H%M%S')
RUN_CSV="${OUTPUT_DIR}/run_${TIMESTAMP}.csv"
LOG_FILE="${LOG_DIR}/run_${TIMESTAMP}.log"

mkdir -p "${OUTPUT_DIR}" "${LOG_DIR}"

log() {
    echo "[$(date '+%H:%M:%S')] $*" | tee -a "${LOG_FILE}"
}

# ---- Sanity check ----
if [[ ! -x "${DRIVER}" ]]; then
    echo "ERROR: Cannot find executable: ${DRIVER}" >&2
    exit 1
fi

# ---- Write CSV header ----
"${DRIVER}" HEADER > "${RUN_CSV}"

log "========================================================"
log "Phase 11 Benchmark Campaign"
log "Timestamp  : ${TIMESTAMP}"
log "Repetitions: ${REPS}"
log "Output CSV : ${RUN_CSV}"
log "Log file   : ${LOG_FILE}"
log "========================================================"

# ---- Benchmark list ----
BENCHMARKS=(
    "bm01_simple_alloc.c"
    "bm02_linked_list_build.c"
    "bm03_dead_pointers.c"
    "bm04_aliasing.c"
    "bm05_branch_alloc.c"
    "bm06_loop_list.c"
    "bm07_multi_field_graph.c"
    "bm08_genuine_loop.c"
)

PASS_COUNT=0
FAIL_COUNT=0

for BM_FILE in "${BENCHMARKS[@]}"; do
    BM_PATH="${BM_DIR}/${BM_FILE}"
    BM_NAME="${BM_FILE%.c}"

    if [[ ! -f "${BM_PATH}" ]]; then
        log "SKIP: ${BM_FILE} not found"
        continue
    fi

    log "----"
    log "Benchmark: ${BM_NAME}"

    # ---- Correctness check: all three configs must succeed on run 1 ----
    A_OK=1; B_OK=1; C_OK=1

    for CFG in "${CONFIGS[@]}"; do
        OUT=$("${DRIVER}" "${BM_PATH}" "${CFG}" "check" 2>&1) || true
        # success column is field 4 (1-indexed)
        SUCCESS=$(echo "${OUT}" | cut -d',' -f4)
        if [[ "${SUCCESS}" != "1" ]]; then
            log "  FAIL: ${BM_NAME} × ${CFG}: driver returned non-success"
            case "${CFG}" in
                A) A_OK=0;;
                B) B_OK=0;;
                C) C_OK=0;;
            esac
        fi
    done

    if [[ "${A_OK}" -eq 0 || "${B_OK}" -eq 0 || "${C_OK}" -eq 0 ]]; then
        log "  EXCLUDED: correctness gate failed for ${BM_NAME}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        continue
    fi

    log "  Correctness gate: A✓ B✓ C✓"

    # ---- Repetition runs ----
    for (( RUN=1; RUN<=REPS; RUN++ )); do
        for CFG in "${CONFIGS[@]}"; do
            LINE=$("${DRIVER}" "${BM_PATH}" "${CFG}" "${RUN}" 2>/dev/null)
            echo "${LINE}" >> "${RUN_CSV}"
        done
    done

    log "  Runs complete: ${REPS} repetitions × 3 configs = $((REPS * 3)) rows"
    PASS_COUNT=$((PASS_COUNT + 1))
done

log "========================================================"
log "Campaign complete"
log "Benchmarks accepted : ${PASS_COUNT}"
log "Benchmarks failed   : ${FAIL_COUNT}"
log "Raw data written to : ${RUN_CSV}"
log "========================================================"
