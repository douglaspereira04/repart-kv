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
# 8. Paths
# Example:
# run_and_move_metrics workload.txt 1 tkrzw_tree 1 1000 /tmp/path1
function run_and_move_metrics {
    local TEST_NUMBER=$1
    local WORKLOAD_FILE=$2
    local PARTITIONS=$3
    local W=$4
    local KV_STORAGE_TYPE=$5
    local STORAGE_ENGINE=$6
    local REPARTITIONING_INTERVAL=$7
    local THINKING_TIME=$8
    shift 8
    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local WORKLOAD_BASENAME=$(basename "$WORKLOAD_FILE")
    # Remove file extension (everything after the last dot)
    local WORKLOAD="${WORKLOAD_BASENAME%.*}"
    local NUMBER_OF_PATHS=${#PATHS[@]}
    
    local COMMA_SEPARATED_PATHS=$(IFS=,; echo "${PATHS[*]}")

    # create repart_kv_storage directory in each path
    # stop if directory already exists
    for p in "${PATHS[@]}"; do
        if [ -d "${p}/repart_kv_storage" ]; then
            echo "Error: directory ${p}/repart_kv_storage already exists"
            exit 1
        fi
        mkdir -p "${p}/repart_kv_storage" || true
    done

    local EXECUTE="./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL"
    echo $EXECUTE


    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}__${THINKING_TIME}
    
    # Create results folder if it doesn't exist
    mkdir -p results

    #try run the experiment
    ./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $THINKING_TIME $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL 60 > "results/${BASENAME}(${TEST_NUMBER}).log"
    
    if [ $? -ne 0 ]; then
        echo "Error: experiment failed"
        exit 1
    fi

    #deletes directories repart_kv_storage in each path
    for p in "${PATHS[@]}"; do
        rm -rf "$p/repart_kv_storage"
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
# 6 to n. Paths
# Example:
# run_hard_experiments 10 workload.txt 1000 tkrzw_tree 1,2,4 1,2,4 /tmp/path1 /tmp/path2
function run_hard_experiments {
    local TEST_NUMBER=$1
    local WORKLOAD_FILE=$2
    local STORAGE_ENGINE=$3
    local COMMA_SEPARATED_TEST_WORKERS=$4
    local COMMA_SEPARATED_PARTITIONS=$5
    local THINKING_TIME=$6

    # Convert comma separated partitions to array
    local PARTITIONS=($(echo $COMMA_SEPARATED_PARTITIONS | tr ',' '\n'))

    # Convert comma separated test workers to array
    local TEST_WORKERS=($(echo $COMMA_SEPARATED_TEST_WORKERS | tr ',' '\n'))

    shift 6

    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local REPARTITIONING_INTERVALS=(0 3000)

    # for each worker count run experiment with pure engine
    for W in ${TEST_WORKERS[@]}; do
        #test repart-kv varying number of test workers, ...

        # test engine
        run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE 0 $THINKING_TIME ${PATHS[0]}

        for P in ${PARTITIONS[@]}; do
            # test lock stripping
            run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME ${PATHS[0]}
        
            # if number of partitions is greater than 1 and number of paths is greater than 1
            if [ $P -gt 1 ] && [ ${#PATHS[@]} -gt 1 ]; then
                # ... and usage of multiple storage drives
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME ${PATHS[@]}
            fi

            # test hard repartitioning
            # ... repartition interval, ...
            for INTERVAL in ${REPARTITIONING_INTERVALS[@]}; do
                # ... number of partitions, ...
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $INTERVAL $THINKING_TIME ${PATHS[0]}

                # if number of partitions is greater than 1 and number of paths is greater than 1
                if [ $P -gt 1 ] && [ ${#PATHS[@]} -gt 1 ]; then
                    # ... and usage of multiple storage drives
                    run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $INTERVAL $THINKING_TIME ${PATHS[@]}
                fi
            done 
        done
    done

}

TMP=("/tmp")

REPETITIONS=5

WORKLOADS=(ycsb_a.toml ycsb_d.toml ycsb_e.toml ycsb_b.toml ycsb_c.toml)
STORAGE_ENGINES=(leveldb lmdb tkrzw_tree)
TEST_WORKERS="1,2,4,6,8,10,12,14,16"
PARTITIONS="16"
THINKING_TIMES=(100000)

for TEST_NUMBER in $(seq 1 $REPETITIONS); do
    for WORKLOAD in ${WORKLOADS[@]}; do
        for STORAGE_ENGINE in ${STORAGE_ENGINES[@]}; do
            for THINKING_TIME in ${THINKING_TIMES[@]}; do
                echo "run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME ${TMP[@]}"
                run_hard_experiments $TEST_NUMBER $WORKLOAD $STORAGE_ENGINE $TEST_WORKERS $PARTITIONS $THINKING_TIME ${TMP[@]}
            done
        done
    done
done