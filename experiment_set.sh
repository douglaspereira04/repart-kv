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
# 9. Storage engine sync (false/true/0/1, forwarded to repart-kv-runner).
#    Result CSV basenames use sync_off/sync_on to match repart-kv-runner output.
# 10+. Paths
# Example:
# run_and_move_metrics workload.txt 1 tkrzw_tree 1 1000 0 false /tmp/path1
function run_and_move_metrics {
    local TEST_NUMBER=$1
    local WORKLOAD_FILE=$2
    local PARTITIONS=$3
    local W=$4
    local KV_STORAGE_TYPE=$5
    local STORAGE_ENGINE=$6
    local REPARTITIONING_INTERVAL=$7
    local THINKING_TIME=$8
    local SYNC=$9
    shift 9
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

    local EXECUTE="./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL 60 $SYNC"
    echo $EXECUTE


    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}__${THINKING_TIME}__${SYNC_TAG}
    
    # Create results folder if it doesn't exist
    mkdir -p results

    #try run the experiment
    ./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL 60 $SYNC > "results/${BASENAME}(${TEST_NUMBER}).log"
    
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
# 7. Storage engine sync (false/true)
# 8 to n. Paths
# Example:
# run_hard_experiments 10 workload.txt tkrzw_tree 1,2,4 1,2,4 0 false /tmp/path1
function run_hard_experiments {
    local TEST_NUMBER=$1
    local WORKLOAD_FILE=$2
    local STORAGE_ENGINE=$3
    local COMMA_SEPARATED_TEST_WORKERS=$4
    local COMMA_SEPARATED_PARTITIONS=$5
    local THINKING_TIME=$6
    local SYNC=$7

    # Convert comma separated partitions to array
    local PARTITIONS=($(echo $COMMA_SEPARATED_PARTITIONS | tr ',' '\n'))

    # Convert comma separated test workers to array
    local TEST_WORKERS=($(echo $COMMA_SEPARATED_TEST_WORKERS | tr ',' '\n'))

    shift 7

    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local REPARTITIONING_INTERVALS=(0 3000)

    # for each worker count run experiment with pure engine
    for W in ${TEST_WORKERS[@]}; do
        #test repart-kv varying number of test workers, ...

        # test engine
        run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE 0 $THINKING_TIME $SYNC ${PATHS[0]}

        for P in ${PARTITIONS[@]}; do
            # test lock stripping
            run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME $SYNC ${PATHS[@]}

            # test hard repartitioning
            # ... repartition interval, ...
            for INTERVAL in ${REPARTITIONING_INTERVALS[@]}; do
                # ... number of partitions, ...
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $INTERVAL $THINKING_TIME $SYNC ${PATHS[@]}
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
# Uses global TEST_WORKERS, PARTITIONS, TMP for run_hard_experiments.
function run_experiment_set {
    declare -n syncon=$1
    local repetitions=$2
    declare -n storage_engines=$3
    declare -n workloads=$4
    declare -n thinking_times=$5

    for SYNC in "${syncon[@]}"; do
        for TEST_NUMBER in $(seq 1 "$repetitions"); do
            for STORAGE_ENGINE in "${storage_engines[@]}"; do
                for WORKLOAD in "${workloads[@]}"; do
                    for THINKING_TIME in "${thinking_times[@]}"; do
                        echo "run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME $SYNC ${TMP[@]}"
                        run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME $SYNC ${TMP[@]}
                    done
                done
            done
        done
    done
}

TMP=("/media/douglas/340ebcc9-1b07-4111-a768-fc8ac9cad904")

#deletes directories repart_kv_storage in each path
for p in "${TMP[@]}"; do
    rm -rf "$p/repart_kv_storage"
done

REPETITIONS=1

STORAGE_ENGINES=(leveldb)
WORKLOADS=(ycsb_a.toml)
TEST_WORKERS="1,4,8,16"
THINKING_TIMES=(50000)
PARTITIONS="16"
SYNCON=(false)


run_experiment_set SYNCON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES

WORKLOADS=(ycsb_d.toml)

run_experiment_set SYNCON "$REPETITIONS" STORAGE_ENGINES WORKLOADS THINKING_TIMES

WORKLOADS=(ycsb_e.toml)
THINKING_TIMES=(5000)

run_experiment_set SYNCON "$REPETITIONS"s STORAGE_ENGINES WORKLOADS THINKING_TIMES