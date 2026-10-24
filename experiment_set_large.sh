#!/bin/bash

# Same experiment grid as experiment_set.sh, but with 32 GiB payloads
# (ycsb_{a,d,e}_large.toml: 8,388,608 records × 4096 B). CSV names use the
# _large workload stem, so they do not collide with results from experiment_set.sh.

_EXPERIMENT_SET_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=experiment_set_lib.sh
source "${_EXPERIMENT_SET_DIR}/experiment_set_lib.sh"
experiment_enable_dry_run_from_args "$@"

TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d" "/media/douglas/ee8c0c0d-fdf9-4ae8-a9f0-4015c99554cb" "/media/douglas/fdedd994-5a78-4a52-9109-4a8fe3d2bad7")

experiment_clear_storage_paths "${TMP[@]}"

REPETITIONS=3
MAX_DURATION_SEC=30
HARD_REPART_INTERVAL_MS=10000
TRACKING_DURATION_MS=1000

STORAGE_ENGINES=(lmdb_nolock)
#TEST_WORKERS="1,4,8,12,16,24,32"
#THINKING_TIMES=(5000000) # 5ms (if operations are instant, then it would at most 200ops*threads/second)
PARTITIONS="32"

# Explicit list matching _hard_experiment_type_enabled / run_hard_experiments docs;
# pass this array name as arg 8 to run_experiment_set to run every variant;
# pass TRACKING_DURATION_MS as arg 9.
ALL_HARD_EXPERIMENT_TYPES=(engine partitioned repartitioning lock_stripping range_lock_stripping)
#ALL_HARD_EXPERIMENT_TYPES=(engine)


WORKLOADS=(ycsb_a_large.toml)
#WORKLOADS=(ycsb_a.toml)

TEST_WORKERS="4,8,12,16"
THINKING_TIMES=(100000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(10000000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
ALL_HARD_EXPERIMENT_TYPES=(partitioned repartitioning lock_stripping range_lock_stripping)

TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d")

experiment_clear_storage_paths "${TMP[@]}"

THINKING_TIMES=(100000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(10000000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"


STORAGE_ENGINES=(rocksdb)
ALL_HARD_EXPERIMENT_TYPES=(engine)

experiment_clear_storage_paths "${TMP[@]}"

THINKING_TIMES=(100000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(10000000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"



#READ WORKLOADS
TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d" "/media/douglas/ee8c0c0d-fdf9-4ae8-a9f0-4015c99554cb" "/media/douglas/fdedd994-5a78-4a52-9109-4a8fe3d2bad7")


STORAGE_ENGINES=(lmdb_nolock)
ALL_HARD_EXPERIMENT_TYPES=(engine partitioned repartitioning lock_stripping range_lock_stripping)


WORKLOADS=(ycsb_d_large.toml ycsb_e_large.toml)

TEST_WORKERS="4,8,12,16"
THINKING_TIMES=(20000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(200000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
ALL_HARD_EXPERIMENT_TYPES=(partitioned repartitioning lock_stripping range_lock_stripping)

TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d")

experiment_clear_storage_paths "${TMP[@]}"

THINKING_TIMES=(20000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(200000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"


STORAGE_ENGINES=(rocksdb)
ALL_HARD_EXPERIMENT_TYPES=(engine)

experiment_clear_storage_paths "${TMP[@]}"

THINKING_TIMES=(20000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(200000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
