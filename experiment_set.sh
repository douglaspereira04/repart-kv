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
    shift 7
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

    local EXECUTE="./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL"
    echo $EXECUTE


    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}
    
    # Create results folder if it doesn't exist
    mkdir -p results

    #try run the experiment
    ./build/repart-kv-runner $WORKLOAD_FILE $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL > "results/${BASENAME}(${TEST_NUMBER}).log"
    
    if [ $? -ne 0 ]; then
        echo "Error: experiment failed"
        exit 1
    fi

    #deletes directories repart_kv_storage in each path
    for p in "${PATHS[@]}"; do
        rm -rf "$p/repart_kv_storage"
    done
    mv "${BASENAME}.csv" "results/${BASENAME}(${TEST_NUMBER}).csv"

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

    # Convert comma separated partitions to array
    local PARTITIONS=($(echo $COMMA_SEPARATED_PARTITIONS | tr ',' '\n'))

    # Convert comma separated test workers to array
    local TEST_WORKERS=($(echo $COMMA_SEPARATED_TEST_WORKERS | tr ',' '\n'))

    shift 5

    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local REPARTITIONING_INTERVALS=(0 5000)

    # for each worker count run experiment with pure engine
    for W in ${TEST_WORKERS[@]}; do
        #test repart-kv varying number of test workers, ...

        # test engine
        run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE 0 ${PATHS[0]}

        for P in ${PARTITIONS[@]}; do
            # test lock stripping
            run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 ${PATHS[0]}
        
            # if number of partitions is greater than 1 and number of paths is greater than 1
            if [ $P -gt 1 ] && [ ${#PATHS[@]} -gt 1 ]; then
                # ... and usage of multiple storage drives
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 ${PATHS[@]}
            fi

            # test hard repartitioning
            # ... repartition interval, ...
            for INTERVAL in ${REPARTITIONING_INTERVALS[@]}; do
                # ... number of partitions, ...
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $INTERVAL ${PATHS[0]}

                # if number of partitions is greater than 1 and number of paths is greater than 1
                if [ $P -gt 1 ] && [ ${#PATHS[@]} -gt 1 ]; then
                    # ... and usage of multiple storage drives
                    run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $INTERVAL ${PATHS[@]}
                fi
            done 
        done
    done

}

TMP=("/tmp")


REPETITIONS=5

for TEST_NUMBER in $(seq 1 $REPETITIONS); do

    #invoke run_hard_experiments function with arguments
    run_hard_experiments $TEST_NUMBER ycsb_a.toml lmdb 1,2,4 1,8 $TMP
    run_hard_experiments $TEST_NUMBER ycsb_a.toml tkrzw_tree 1,2,4 1,8 $TMP

    #invoke run_hard_experiments function with arguments
    run_hard_experiments $TEST_NUMBER ycsb_d.toml lmdb 1,2,4 1,8 $TMP
    run_hard_experiments $TEST_NUMBER ycsb_d.toml tkrzw_tree 1,2,4 1,8 $TMP

    #invoke run_hard_experiments function with arguments
    run_hard_experiments $TEST_NUMBER ycsb_e.toml lmdb 1,2,4 1,8 $TMP
    run_hard_experiments $TEST_NUMBER ycsb_e.toml tkrzw_tree 1,2,4 1,8 $TMP

    #invoke run_hard_experiments function with arguments
    run_hard_experiments $TEST_NUMBER ycsb_b.toml lmdb 1,2,4 1,8 $TMP
    run_hard_experiments $TEST_NUMBER ycsb_b.toml tkrzw_tree 1,2,4 1,8 $TMP

    #invoke run_hard_experiments function with arguments
    run_hard_experiments $TEST_NUMBER ycsb_c.toml lmdb 1,2,4 1,8 $TMP
    run_hard_experiments $TEST_NUMBER ycsb_c.toml tkrzw_tree 1,2,4 1,8 $TMP
done