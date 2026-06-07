#!/bin/bash



# Function to run and move metrics
# Arguments:
# 1. Test number
# 2. Workload file
# 3. Partitions
# 4. Test workers
# 5. KV Storage type (hard, soft, threaded, hard_threaded, engine)
# 6. Storage engine
# 7. Repartitioning interval
# 8. Thinking time (ns)
# 9. Max duration (seconds, repart-kv-runner argv / MAX_DURATION)
# 10. Storage engine sync (false/true/0/1, forwarded to repart-kv-runner).
#     Result CSV basenames use sync_off/sync_on to match repart-kv-runner output.
# 11. Tracking duration (ms, repart-kv-runner argv / TRACKING_DURATION)
# 12+. Paths
# Example:
# run_and_move_metrics workload.txt 1 tkrzw_tree 1 1000 0 60 false 10000 /tmp/path1
function run_and_move_metrics {
    local TEST_NUMBER=$1
    local WORKLOAD_FILE=$2
    local PARTITIONS=$3
    local W=$4
    local KV_STORAGE_TYPE=$5
    local STORAGE_ENGINE=$6
    local REPARTITIONING_INTERVAL=$7
    local THINKING_TIME=$8
    local MAX_DURATION=$9
    local SYNC="${10}"
    local TRACKING_DURATION_MS="${11}"
    shift 11
    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local WORKLOAD_BASENAME=$(basename "$WORKLOAD_FILE")
    # Remove file extension (everything after the last dot)
    local WORKLOAD="${WORKLOAD_BASENAME%.*}"
    local NUMBER_OF_PATHS=${#PATHS[@]}
    
    local COMMA_SEPARATED_PATHS=$(IFS=,; echo "${PATHS[*]}")

    # repart_kv names metrics like ...__sync_on.csv / ...__sync_off.csv (see repart_kv.cpp)
    local SYNC_LOWER
    SYNC_LOWER=$(echo "$SYNC" | tr '[:upper:]' '[:lower:]')
    local SYNC_TAG
    case "$SYNC_LOWER" in
        true|1|on|yes) SYNC_TAG="sync_on" ;;
        *) SYNC_TAG="sync_off" ;;
    esac

    local EXECUTE="./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL $MAX_DURATION $SYNC $TRACKING_DURATION_MS"
    echo $EXECUTE

    if { [ -f success.log ] && grep -Fxq "$EXECUTE" success.log; } || \
       { [ -f fail.log ] && grep -Fxq "$EXECUTE" fail.log; }; then
        echo "Skipping (already in success.log or fail.log): $EXECUTE"
        return 0
    fi

    # create repart_kv_storage directory in each path
    # stop if directory already exists
    for p in "${PATHS[@]}"; do
        if [ -d "${p}/repart_kv_storage" ]; then
            echo "Error: directory ${p}/repart_kv_storage already exists"
            exit 1
        fi
        mkdir -p "${p}/repart_kv_storage" || true
    done

    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}__${THINKING_TIME}__${SYNC_TAG}
    
    # Create results folder if it doesn't exist
    mkdir -p results

    # Run the experiment: 1 try + 3 retries (4 attempts total).
    local attempt=1
    local max_attempts=1
    local experiment_succeeded=0
    while [ "$attempt" -le "$max_attempts" ]; do
        if ./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL $MAX_DURATION $SYNC $TRACKING_DURATION_MS > "results/${BASENAME}(${TEST_NUMBER}).log"; then
            experiment_succeeded=1
            break
        fi
        if [ "$attempt" -eq "$max_attempts" ]; then
            echo "Warning: experiment failed after $max_attempts attempts"
            : > "results/${BASENAME}(${TEST_NUMBER}).log"
            rm -f "${BASENAME}.csv" "${BASENAME}__latency.csv"
            break
        fi
        echo "Warning: attempt $attempt failed, retrying ($((max_attempts - attempt)) attempts remaining)..."
        attempt=$((attempt + 1))
    done

    #deletes directories repart_kv_storage in each path
    for p in "${PATHS[@]}"; do
        rm -rf "$p/repart_kv_storage"
        rm -rf "$p/repart_kv_keystorage"
    done
    if [ "$experiment_succeeded" -eq 1 ]; then
        echo "$EXECUTE" >> success.log
        mv "${BASENAME}.csv" "results/${BASENAME}(${TEST_NUMBER}).csv"
        mv "${BASENAME}__latency.csv" "results/${BASENAME}__latency(${TEST_NUMBER}).csv"
    else
        echo "$EXECUTE" >> fail.log
        rm -f "${BASENAME}.csv" "${BASENAME}__latency.csv"
    fi

    #echo $BASENAME
    #echo ""
}

# Experiment-mode names for nameref-backed arrays passed into run_hard_experiments /
# run_experiment_set. Allowed entries: engine, lock stripping, partitioned, repartitioning.

# Return 0 when $want is listed in the remaining $@ selection.
_hard_experiment_type_enabled() {
    local want="$1"
    shift
    local m
    for m in "$@"; do
        if [ "$m" = "$want" ]; then
            return 0
        fi
    done
    return 1
}

# Function to run hard experiments
# Arguments:
# 1. Test number
# 2. Workload file
# 3. Storage engine
# 4. Comma separated numbers of test workers
# 5. Comma separated partitions
# 6. Thinking time (ns)
# 7. Max duration (seconds, repart-kv-runner / MAX_DURATION)
# 8. Non-zero hard repartition interval (ms); paired with partitioned vs repartitioning
# 9. Storage engine sync (false/true)
# 10. Tracking duration (ms, repart-kv-runner argv / TRACKING_DURATION)
# 11. Name of a bash array of experiment modes to run: engine | lock stripping |
#     partitioned | repartitioning (required; pass e.g. ALL_HARD_EXPERIMENT_TYPES).
# 12 to n. Paths
# Example:
# HARD_TYPES=(engine "partitioned")
# run_hard_experiments 10 workload.txt tkrzw_tree 1,2,4 1,2,4 0 60 5000 false 10000 HARD_TYPES /tmp/path1
function run_hard_experiments {
    local TEST_NUMBER=$1
    local WORKLOAD_FILE=$2
    local STORAGE_ENGINE=$3
    local COMMA_SEPARATED_TEST_WORKERS=$4
    local COMMA_SEPARATED_PARTITIONS=$5
    local THINKING_TIME=$6
    local MAX_DURATION=$7
    local HARD_REPART_INTERVAL_MS=$8
    local SYNC=$9
    local TRACKING_DURATION_MS=$10
    declare -n _EXPERIMENT_TYPES_REF="${11}"
    shift 11

    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local -a _exp_types=("${_EXPERIMENT_TYPES_REF[@]}")

    # Convert comma separated partitions to array
    local PARTITIONS=($(echo $COMMA_SEPARATED_PARTITIONS | tr ',' '\n'))

    # Convert comma separated test workers to array
    local TEST_WORKERS=($(echo $COMMA_SEPARATED_TEST_WORKERS | tr ',' '\n'))

    # for each worker count run experiment with pure engine
    for W in ${TEST_WORKERS[@]}; do
        #test repart-kv varying number of test workers, ...

        # test engine
        if _hard_experiment_type_enabled engine "${_exp_types[@]}"; then
            run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[0]}
        fi

        for P in ${PARTITIONS[@]}; do
            # test lock stripping
            if _hard_experiment_type_enabled lock_stripping "${_exp_types[@]}"; then
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[@]}
            fi

            # hard repartitioning: interval 0 (partitioned layout only)
            if _hard_experiment_type_enabled partitioned "${_exp_types[@]}"; then
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[@]}
            fi

            # hard repartitioning: periodic repartitioning
            if _hard_experiment_type_enabled repartitioning "${_exp_types[@]}"; then
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE "$HARD_REPART_INTERVAL_MS" $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[@]}
            fi
        done
    done

}

# Outer experiment grid over sync modes, repetitions, engines, workloads, and
# thinking times. Array parameters are bash array *names* (nameref); pass
# literals like SYNCON or STORAGE_ENGINES without $.
# Arguments:
#   $1  sync_on                  name of array (e.g. SYNC_ON)
#   $2  repetitions              integer (e.g. REPETITIONS)
#   $3  storage_engines          name of array (e.g. STORAGE_ENGINES)
#   $4  workloads               name of array (e.g. WORKLOADS)
#   $5  thinking_times          name of array (e.g. THINKING_TIMES)
#   $6  max_duration_sec       integer seconds for repart-kv-runner MAX_DURATION
#   $7  hard_repart_interval_ms non-zero hard repartition interval (ms) for mode repartitioning
#   $8  hard_experiment_types   required name of array of modes to run:
#                              engine | lock stripping | partitioned | repartitioning
#   $9  tracking_duration_ms    tracking duration (ms) for repart-kv-runner TRACKING_DURATION
# Uses global TEST_WORKERS, PARTITIONS, TMP for run_hard_experiments.
function run_experiment_set {
    declare -n sync_on=$1
    local repetitions=$2
    declare -n storage_engines=$3
    declare -n workloads=$4
    declare -n thinking_times=$5
    local max_duration_sec=$6
    local hard_repart_interval_ms=$7
    local hard_types_ref="$8"
    local tracking_duration_ms=$9

    for SYNC in "${sync_on[@]}"; do
        for TEST_NUMBER in $(seq 1 "$repetitions"); do
            for STORAGE_ENGINE in "${storage_engines[@]}"; do
                for WORKLOAD in "${workloads[@]}"; do
                    for THINKING_TIME in "${thinking_times[@]}"; do
                        echo "run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME $max_duration_sec $hard_repart_interval_ms $SYNC $tracking_duration_ms $hard_types_ref ${TMP[@]}"
                        run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME $max_duration_sec $hard_repart_interval_ms $SYNC $tracking_duration_ms "$hard_types_ref" ${TMP[@]}
                    done
                done
            done
        done
    done
}

TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d" "/media/douglas/ee8c0c0d-fdf9-4ae8-a9f0-4015c99554cb" "/media/douglas/fdedd994-5a78-4a52-9109-4a8fe3d2bad7")

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
    rm -rf "$p/repart_kv_keystorage"
done

REPETITIONS=5
MAX_DURATION_SEC=30
HARD_REPART_INTERVAL_MS=10000
TRACKING_DURATION_MS=1000

STORAGE_ENGINES=(leveldb lmdb)
#STORAGE_ENGINES=(leveldb)
#TEST_WORKERS="1,4,8,12,16,24,32"
#THINKING_TIMES=(5000000) # 5ms (if operations are instant, then it would at most 200ops*threads/second)
PARTITIONS="32"

# Explicit list matching _hard_experiment_type_enabled / run_hard_experiments docs;
# pass this array name as arg 8 to run_experiment_set to run every variant;
# pass TRACKING_DURATION_MS as arg 9.
ALL_HARD_EXPERIMENT_TYPES=(engine partitioned repartitioning lock_stripping)
#ALL_HARD_EXPERIMENT_TYPES=(engine)


WORKLOADS=(ycsb_a.toml)
#WORKLOADS=(ycsb_e.toml)

TEST_WORKERS="4,8,12,16"
THINKING_TIMES=(100000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(10000000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
ALL_HARD_EXPERIMENT_TYPES=(partitioned repartitioning lock_stripping)

TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d")

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
    rm -rf "$p/repart_kv_keystorage"
done

THINKING_TIMES=(100000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(10000000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"


STORAGE_ENGINES=(rocksdb)
ALL_HARD_EXPERIMENT_TYPES=(engine)

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
    rm -rf "$p/repart_kv_keystorage"
done

THINKING_TIMES=(100000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(10000000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"



#READ WORKLOADS
TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d" "/media/douglas/ee8c0c0d-fdf9-4ae8-a9f0-4015c99554cb" "/media/douglas/fdedd994-5a78-4a52-9109-4a8fe3d2bad7")


STORAGE_ENGINES=(leveldb lmdb)
ALL_HARD_EXPERIMENT_TYPES=(engine partitioned repartitioning lock_stripping)


WORKLOADS=(ycsb_d.toml ycsb_e.toml)

TEST_WORKERS="4,8,12,16"
THINKING_TIMES=(20000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(200000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
ALL_HARD_EXPERIMENT_TYPES=(partitioned repartitioning lock_stripping)

TMP=("/media/douglas/055a7abb-4de7-4973-96ca-2c96fc9cf83d")

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
    rm -rf "$p/repart_kv_keystorage"
done

THINKING_TIMES=(20000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(200000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"


STORAGE_ENGINES=(rocksdb)
ALL_HARD_EXPERIMENT_TYPES=(engine)

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
    rm -rf "$p/repart_kv_keystorage"
done

THINKING_TIMES=(20000)
SYNC_ON=(false)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"

THINKING_TIMES=(200000)
SYNC_ON=(true)
run_experiment_set SYNC_ON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS" ALL_HARD_EXPERIMENT_TYPES "$TRACKING_DURATION_MS"
