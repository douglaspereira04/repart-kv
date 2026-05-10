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
# 11+. Paths
# Example:
# run_and_move_metrics workload.txt 1 tkrzw_tree 1 1000 0 60 false /tmp/path1
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
    local SYNC=${10}
    shift 10
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

    # create repart_kv_storage directory in each path
    # stop if directory already exists
    for p in "${PATHS[@]}"; do
        if [ -d "${p}/repart_kv_storage" ]; then
            echo "Error: directory ${p}/repart_kv_storage already exists"
            exit 1
        fi
        mkdir -p "${p}/repart_kv_storage" || true
    done

    local EXECUTE="./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL $MAX_DURATION $SYNC"
    echo $EXECUTE


    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}__${THINKING_TIME}__${SYNC_TAG}
    
    # Create results folder if it doesn't exist
    mkdir -p results

    #try run the experiment
    ./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL $MAX_DURATION $SYNC > "results/${BASENAME}(${TEST_NUMBER}).log"
    
    if [ $? -ne 0 ]; then
        echo "Error: experiment failed"
        exit 1
    fi

    #deletes directories repart_kv_storage in each path
    for p in "${PATHS[@]}"; do
        rm -rf "$p/repart_kv_storage"
        rm -rf "$p/repart_kv_keystorage"
    done
    mv "${BASENAME}.csv" "results/${BASENAME}(${TEST_NUMBER}).csv"
    mv "${BASENAME}__latency.csv" "results/${BASENAME}__latency(${TEST_NUMBER}).csv"

    #echo $BASENAME
    #echo ""
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
# 8. Non-zero hard repartition interval (ms); paired with 0 in REPARTITIONING_INTERVALS
# 9. Storage engine sync (false/true)
# 10 to n. Paths
# Example:
# run_hard_experiments 10 workload.txt tkrzw_tree 1,2,4 1,2,4 0 60 5000 false /tmp/path1
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

    # Convert comma separated partitions to array
    local PARTITIONS=($(echo $COMMA_SEPARATED_PARTITIONS | tr ',' '\n'))

    # Convert comma separated test workers to array
    local TEST_WORKERS=($(echo $COMMA_SEPARATED_TEST_WORKERS | tr ',' '\n'))

    shift 9

    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local REPARTITIONING_INTERVALS=(0 "$HARD_REPART_INTERVAL_MS")

    # for each worker count run experiment with pure engine
    for W in ${TEST_WORKERS[@]}; do
        #test repart-kv varying number of test workers, ...

        # test engine
        run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC ${PATHS[0]}

        for P in ${PARTITIONS[@]}; do
            # test lock stripping
            run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC ${PATHS[@]}

            # test hard repartitioning
            # ... repartition interval, ...
            for INTERVAL in ${REPARTITIONING_INTERVALS[@]}; do
                # ... number of partitions, ...
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $INTERVAL $THINKING_TIME $MAX_DURATION $SYNC ${PATHS[@]}
            done 
        done
    done

}

# Outer experiment grid over sync modes, repetitions, engines, workloads, and
# thinking times. Array parameters are bash array *names* (nameref); pass
# literals like SYNCON or STORAGE_ENGINES without $.
# Arguments:
#   $1  syncon             name of array (e.g. SYNCON)
#   $2  repetitions        integer (e.g. REPETITIONS)
#   $3  storage_engines    name of array (e.g. STORAGE_ENGINES)
#   $4  workloads          name of array (e.g. WORKLOADS)
#   $5  thinking_times     name of array (e.g. THINKING_TIMES)
#   $6  max_duration_sec        integer seconds for repart-kv-runner MAX_DURATION
#   $7  hard_repart_interval_ms non-zero hard repartition interval (ms); also paired with 0
# Uses global TEST_WORKERS, PARTITIONS, TMP for run_hard_experiments.
function run_experiment_set {
    declare -n syncon=$1
    local repetitions=$2
    declare -n storage_engines=$3
    declare -n workloads=$4
    declare -n thinking_times=$5
    local max_duration_sec=$6
    local hard_repart_interval_ms=$7

    for SYNC in "${syncon[@]}"; do
        for TEST_NUMBER in $(seq 1 "$repetitions"); do
            for STORAGE_ENGINE in "${storage_engines[@]}"; do
                for WORKLOAD in "${workloads[@]}"; do
                    for THINKING_TIME in "${thinking_times[@]}"; do
                        echo "run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME $max_duration_sec $hard_repart_interval_ms $SYNC ${TMP[@]}"
                        run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME $max_duration_sec $hard_repart_interval_ms $SYNC ${TMP[@]}
                    done
                done
            done
        done
    done
}

TMP=("055a7abb-4de7-4973-96ca-2c96fc9cf83d" "ee8c0c0d-fdf9-4ae8-a9f0-4015c99554cb" "fdedd994-5a78-4a52-9109-4a8fe3d2bad7")

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
    rm -rf "$p/repart_kv_keystorage"
done

REPETITIONS=1

STORAGE_ENGINES=(leveldb)
WORKLOADS=(ycsb_a.toml)
TEST_WORKERS="1,4,8,12,16"
THINKING_TIMES=(50000)
PARTITIONS="16"
SYNCON=(true)

MAX_DURATION_SEC=60
HARD_REPART_INTERVAL_MS=5000

run_experiment_set SYNCON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS"

WORKLOADS=(ycsb_d.toml)

run_experiment_set SYNCON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS"

WORKLOADS=(ycsb_e.toml)
THINKING_TIMES=(5000)

run_experiment_set SYNCON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES "$MAX_DURATION_SEC" "$HARD_REPART_INTERVAL_MS"