#!/bin/bash



# Function to run and move metrics
# Arguments:
# 1. Repetition
# 2. Workload file
# 3. Test workers
# 4. Storage engine
# 5. Warmup operations
# 6. KV Storage type (hard, soft, threaded, hard_threaded, engine)
# 7. Partitions
# 8. Repartitioning interval
# 9. Paths
# Example:
# run_and_move_metrics workload.txt 1 tkrzw_tree 1 1000 /tmp/path1
function run_and_move_metrics {
    local REP=$1
    local WORKLOAD_FILE=$2
    local PARTITIONS=$3
    local W=$4
    local KV_STORAGE_TYPE=$5
    local STORAGE_ENGINE=$6
    local WU_OPS=$7
    local REPARTITIONING_INTERVAL=$8
    shift 8
    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local WORKLOAD_BASENAME=$(basename "$WORKLOAD_FILE")
    # Remove file extension (everything after the last dot)
    local WORKLOAD="${WORKLOAD_BASENAME%.*}"
    local NUMBER_OF_PATHS=${#PATHS[@]}
    
    local COMMA_SEPARATED_PATHS=$(IFS=,; echo "${PATHS[*]}")

    # take workload file aand repeat w times separated by commas
    local COMMA_SEPARATED_WORKLOAD_FILES=$(for i in $(seq 1 $W); do echo $WORKLOAD_FILE; done | tr '\n' ',')
    #remove trailing comma if it exists
    COMMA_SEPARATED_WORKLOAD_FILES=$(echo $COMMA_SEPARATED_WORKLOAD_FILES | sed 's/,$//')

    # create repart_kv_storage directory in each path
    # stop if directory already exists
    for p in "${PATHS[@]}"; do
        if [ -d "${p}/repart_kv_storage" ]; then
            echo "Error: directory ${p}/repart_kv_storage already exists"
            exit 1
        fi
        mkdir -p "${p}/repart_kv_storage" || true
    done

    ./build/repart-kv-runner $COMMA_SEPARATED_WORKLOAD_FILES $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $WU_OPS $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL
    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}
    
    # Create results folder if it doesn't exist
    mkdir -p results
    mv "${BASENAME}.csv" "results/${BASENAME}(${REP}).csv"

    local EXECUTE="./build/repart-kv-runner $COMMA_SEPARATED_WORKLOAD_FILES $PARTITIONS $W $KV_STORAGE_TYPE $STORAGE_ENGINE $WU_OPS $COMMA_SEPARATED_PATHS $REPARTITIONING_INTERVAL"
    echo $EXECUTE
    #echo $BASENAME
    #echo ""
    
    #deletes directories repart_kv_storage in each path
    for p in "${PATHS[@]}"; do
        rm -rf "$p/repart_kv_storage"
    done
}


# Function to run hard experiments
# Arguments:
# 1. Repetition count
# 2. Workload file
# 3. Warmup operations
# 4. Storage engine
# 5. Comma separated numbers of test workers
# 6. Comma separated partitions
# 7 to n. Paths
# Example:
# run_hard_experiments 10 workload.txt 1000 tkrzw_tree 1,2,4 1,2,4 /tmp/path1 /tmp/path2
function run_hard_experiments {
    local REPETITIONS=$1
    local WORKLOAD_FILE=$2
    local WU_OPS=$3
    local STORAGE_ENGINE=$4
    local COMMA_SEPARATED_TEST_WORKERS=$5
    local COMMA_SEPARATED_PARTITIONS=$6

    # Convert comma separated partitions to array
    local PARTITIONS=($(echo $COMMA_SEPARATED_PARTITIONS | tr ',' '\n'))

    # Convert comma separated test workers to array
    local TEST_WORKERS=($(echo $COMMA_SEPARATED_TEST_WORKERS | tr ',' '\n'))

    shift 6

    # set PATHS as the subsequent arguments
    local PATHS=("$@")

    local REPARTITIONING_INTERVALS=(0 1000)

    # for each repetition run the experiment
    for REP in $(seq 1 $REPETITIONS); do
        # for each worker count run experiment with pure engine
        for W in ${TEST_WORKERS[@]}; do

            # test engine varying number of test workers
            run_and_move_metrics $REP $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE $WU_OPS 0 ${PATHS[0]}

            #test repart-kv varying number of test workers, ...
            for INTERVAL in ${REPARTITIONING_INTERVALS[@]}; do
                # ... repartition interval, ...
                for P in ${PARTITIONS[@]}; do
                    # ... number of partitions, ...
                    run_and_move_metrics $REP $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $WU_OPS $INTERVAL ${PATHS[0]}

            
                    # if number of partitions is greater than 1 and number of paths is greater than 1
                    if [ $P -gt 1 ] && [ ${#PATHS[@]} -gt 1 ]; then
                        # ... and usage of multiple storage drives
                        run_and_move_metrics $REP $WORKLOAD_FILE $P $W hard $STORAGE_ENGINE $WU_OPS $INTERVAL ${PATHS[@]}
                    fi
                done 
            done
        done
    done

}



#invoke run_hard_experiments function with arguments
run_hard_experiments 4 ycsb_a.txt 100000 lmdb 1,2 1,8 /tmp
run_hard_experiments 4 ycsb_a.txt 100000 tkrzw_tree 1,2 1,8 /tmp

run_hard_experiments 4 ycsb_d.txt 100000 lmdb 1,2 1,8 /tmp
run_hard_experiments 4 ycsb_d.txt 100000 tkrzw_tree 1,2 1,8 /tmp