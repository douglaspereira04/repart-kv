# Shared helpers for experiment_set*.sh. Source this file; do not execute it.
# Defines: run_and_move_metrics, _hard_experiment_type_enabled,
#          run_hard_experiments, run_experiment_set.
#
# Dry-run: pass --dry-run (or -n), or set EXPERIMENT_DRY_RUN=1. Prints the
# repart-kv-runner commands without touching disks, success.log, or the runner.

experiment_is_dry_run() {
    [[ "${EXPERIMENT_DRY_RUN:-}" == "1" || "${EXPERIMENT_DRY_RUN:-}" == "true" ]]
}

experiment_enable_dry_run_from_args() {
    local a
    for a in "$@"; do
        case "$a" in
            --dry-run|-n)
                EXPERIMENT_DRY_RUN=1
                echo "Dry-run: printing runner commands, not executing."
                ;;
        esac
    done
}

# No-op in dry-run so grids can call this instead of rm -rf on experiment disks.
experiment_clear_storage_paths() {
    if experiment_is_dry_run; then
        return 0
    fi
    local p
    for p in "$@"; do
        rm -rf "$p/repart_kv_storage"
        rm -rf "$p/repart_kv_keystorage"
    done
}

# When EXPERIMENT_PRELOAD_SNAPSHOT_DIR is set, each engine/type/partition/interval
# is preloaded once into that directory and later runs copy the tree and skip
# the insert loop. Sync does not change the preloaded bytes, so one snapshot
# covers sync off and sync on. The directory id inside the tree is fixed
# (REPART_KV_STORAGE_ID=snap) so the next process reopens the same files.
EXPERIMENT_SNAPSHOT_STORAGE_ID=snap
EXPERIMENT_SNAPSHOT_PRELOAD_WORKERS="${EXPERIMENT_SNAPSHOT_PRELOAD_WORKERS:-16}"

experiment_workload_number() {
    local file="$1"
    local key="$2"
    awk -F= -v key="$key" '
        {
            name = $1
            gsub(/[[:space:]]/, "", name)
            if (name == key) {
                value = $2
                sub(/#.*/, "", value)
                gsub(/[[:space:]]/, "", value)
                print value
                exit
            }
        }
    ' "$file"
}

experiment_snapshot_key() {
    local workload_file="$1"
    local partitions="$2"
    local kv_type="$3"
    local engine="$4"
    local interval="$5"
    local n_records value_size
    n_records=$(experiment_workload_number "$workload_file" "workload.n_records")
    value_size=$(experiment_workload_number "$workload_file" "workload.value_max_size")
    if [[ -z "$n_records" || -z "$value_size" ]]; then
        echo "Error: could not read n_records/value_max_size from $workload_file" >&2
        return 1
    fi
    printf '%s__%s__p%s__i%s__n%s__v%s\n' \
        "$engine" "$kv_type" "$partitions" "$interval" "$n_records" "$value_size"
}

# Build $dest (with READY) if needed. Paths inside the snapshot are p0, p1, ...
# matching the runner path list for this combo.
experiment_ensure_preload_snapshot() {
    local dest="$1"
    local workload_file="$2"
    local partitions="$3"
    local kv_type="$4"
    local engine="$5"
    local interval="$6"
    local tracking_ms="$7"
    shift 7
    local paths=("$@")

    if [[ -f "$dest/READY" ]]; then
        return 0
    fi
    if [[ -f "$dest/FAILED" ]]; then
        echo "Preload snapshot previously failed: $dest" >&2
        return 1
    fi

    local root stage log_file joined i
    root=$(dirname "$dest")
    stage="${dest}.staging"
    log_file="${dest}.build.log"
    rm -rf "$stage"
    mkdir -p "$root"
    local -a stage_paths=()
    for i in "${!paths[@]}"; do
        mkdir -p "$stage/p${i}"
        stage_paths+=("$stage/p${i}")
    done
    joined=$(IFS=,; echo "${stage_paths[*]}")

    echo "Building preload snapshot $dest"
    echo "  workers=$EXPERIMENT_SNAPSHOT_PRELOAD_WORKERS paths=$joined"
    if ! REPART_KV_STORAGE_ID="$EXPERIMENT_SNAPSHOT_STORAGE_ID" \
        REPART_KV_KEEP_STORAGE=1 \
        REPART_KV_PRELOAD_ONLY=1 \
        ./build/repart-kv-runner \
            "$workload_file" "$partitions" "$EXPERIMENT_SNAPSHOT_PRELOAD_WORKERS" \
            "$kv_type" "$engine" 0 "$joined" "$interval" 1 false "$tracking_ms" \
            > "$log_file" 2>&1; then
        echo "Preload snapshot build failed. Log: $log_file" >&2
        tail -20 "$log_file" >&2 || true
        rm -rf "$stage"
        mkdir -p "$dest"
        echo "see ${log_file}" > "$dest/FAILED"
        return 1
    fi
    if ! grep -q "PRELOAD_ONLY done" "$log_file"; then
        echo "Preload snapshot build exited 0 without PRELOAD_ONLY done. Log: $log_file" >&2
        rm -rf "$stage"
        mkdir -p "$dest"
        echo "see ${log_file}" > "$dest/FAILED"
        return 1
    fi
    printf '%s\n' "$log_file" > "$stage/BUILD_LOG"
    : > "$stage/READY"
    rm -rf "$dest"
    mv "$stage" "$dest"
    echo "Preload snapshot ready: $dest"
}

experiment_restore_preload_snapshot() {
    local snap="$1"
    shift
    local paths=("$@")
    local i p src
    for i in "${!paths[@]}"; do
        p="${paths[$i]}"
        src="$snap/p${i}"
        if [[ ! -d "$src/repart_kv_storage" ]]; then
            echo "Error: snapshot is missing $src/repart_kv_storage" >&2
            return 1
        fi
        rm -rf "$p/repart_kv_storage" "$p/repart_kv_keystorage"
        mkdir -p "$p"
        cp -a --sparse=always "$src/repart_kv_storage" "$p/"
        if [[ -d "$src/repart_kv_keystorage" ]]; then
            cp -a --sparse=always "$src/repart_kv_keystorage" "$p/"
        fi
    done
}

# Function to run and move metrics
# Arguments:
# 1. Test number
# 2. Workload file
# 3. Partitions
# 4. Test workers
# 5. KV Storage type (hard, soft, threaded, hard_threaded, engine,
#    lock_stripping, range_lock_stripping)
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

    local SNAPSHOT_DEST=""
    if [[ -n "${EXPERIMENT_PRELOAD_SNAPSHOT_DIR:-}" ]]; then
        local SNAPSHOT_KEY
        if ! SNAPSHOT_KEY=$(experiment_snapshot_key "$WORKLOAD_FILE" "$PARTITIONS" \
            "$KV_STORAGE_TYPE" "$STORAGE_ENGINE" "$REPARTITIONING_INTERVAL"); then
            echo "$EXECUTE" >> fail.log
            return 0
        fi
        SNAPSHOT_DEST="${EXPERIMENT_PRELOAD_SNAPSHOT_DIR}/${SNAPSHOT_KEY}"
        echo "Preload snapshot: $SNAPSHOT_DEST"
    fi

    if experiment_is_dry_run; then
        return 0
    fi

    if { [ -f success.log ] && grep -Fxq "$EXECUTE" success.log; } || \
       { [ -f fail.log ] && grep -Fxq "$EXECUTE" fail.log; }; then
        echo "Skipping (already in success.log or fail.log): $EXECUTE"
        return 0
    fi

    if [[ -n "$SNAPSHOT_DEST" ]]; then
        if ! experiment_ensure_preload_snapshot "$SNAPSHOT_DEST" "$WORKLOAD_FILE" \
            "$PARTITIONS" "$KV_STORAGE_TYPE" "$STORAGE_ENGINE" \
            "$REPARTITIONING_INTERVAL" "$TRACKING_DURATION_MS" "${PATHS[@]}"; then
            echo "$EXECUTE" >> fail.log
            return 0
        fi
        if ! experiment_restore_preload_snapshot "$SNAPSHOT_DEST" "${PATHS[@]}"; then
            echo "$EXECUTE" >> fail.log
            return 0
        fi
    else
        # create repart_kv_storage directory in each path
        # stop if directory already exists
        for p in "${PATHS[@]}"; do
            if [ -d "${p}/repart_kv_storage" ]; then
                echo "Error: directory ${p}/repart_kv_storage already exists"
                exit 1
            fi
            mkdir -p "${p}/repart_kv_storage" || true
        done
    fi

    local BASENAME=${WORKLOAD}__${W}__${KV_STORAGE_TYPE}__${PARTITIONS}__${STORAGE_ENGINE}__${NUMBER_OF_PATHS}__${REPARTITIONING_INTERVAL}__${THINKING_TIME}__${SYNC_TAG}
    
    # Create results folder if it doesn't exist
    mkdir -p results

    # Run the experiment: 1 try + 3 retries (4 attempts total).
    local attempt=1
    local max_attempts=1
    local experiment_succeeded=0
    while [ "$attempt" -le "$max_attempts" ]; do
        local -a _runner_cmd=(./build/repart-kv-runner "$WORKLOAD_FILE" "$PARTITIONS" "$W" "$KV_STORAGE_TYPE" "$STORAGE_ENGINE" "$THINKING_TIME" "$COMMA_SEPARATED_PATHS" "$REPARTITIONING_INTERVAL" "$MAX_DURATION" "$SYNC" "$TRACKING_DURATION_MS")
        if [[ -n "$SNAPSHOT_DEST" ]]; then
            REPART_KV_STORAGE_ID="$EXPERIMENT_SNAPSHOT_STORAGE_ID" \
                REPART_KV_SKIP_PRELOAD=1 \
                "${_runner_cmd[@]}" > "results/${BASENAME}(${TEST_NUMBER}).log"
        else
            "${_runner_cmd[@]}" > "results/${BASENAME}(${TEST_NUMBER}).log"
        fi
        if [[ $? -eq 0 ]]; then
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
    local TRACKING_DURATION_MS=${10}
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
            if [ "$STORAGE_ENGINE" = "lmdb_nolock" ]; then
                echo "Skipping engine mode for lmdb_nolock (needs caller-owned lock)"
            else
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE 1 $W engine $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[0]}
            fi
        fi

        for P in ${PARTITIONS[@]}; do
            # test lock stripping (hash partitions)
            if _hard_experiment_type_enabled lock_stripping "${_exp_types[@]}"; then
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[@]}
            fi

            # test range lock stripping (uniform ranges from n_records)
            if _hard_experiment_type_enabled range_lock_stripping "${_exp_types[@]}"; then
                run_and_move_metrics $TEST_NUMBER $WORKLOAD_FILE $P $W range_lock_stripping $STORAGE_ENGINE 0 $THINKING_TIME $MAX_DURATION $SYNC $TRACKING_DURATION_MS ${PATHS[@]}
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
