#!/bin/bash

# Smoke-test grid for the same runner paths as experiment_set.sh, with a tiny
# payload (ycsb_{a,e}_mini.toml: 10240 records). Uses /tmp so it can run without
# the experiment disks. CSV names use the _mini workload stem.

_EXPERIMENT_SET_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=experiment_set_lib.sh
source "${_EXPERIMENT_SET_DIR}/experiment_set_lib.sh"
experiment_enable_dry_run_from_args "$@"

TMP=("/tmp/repart-kv-mini")
mkdir -p "${TMP[@]}"

experiment_clear_storage_paths "${TMP[@]}"

REPETITIONS=1
MAX_DURATION_SEC=3
HARD_REPART_INTERVAL_MS=500
TRACKING_DURATION_MS=100

STORAGE_ENGINES=(leveldb lmdb lmdb_nolock rocksdb)
PARTITIONS="32"
TEST_WORKERS="4"
THINKING_TIMES=(0)
SYNC_ON=(false true)

ALL_HARD_EXPERIMENT_TYPES=(engine partitioned repartitioning lock_stripping range_lock_stripping)

WORKLOADS=(ycsb_a_mini.toml ycsb_e_mini.toml)

run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
