#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

mkdir -p aggregated_throughput_time
mkdir -p aggregated_results/throughput
mkdir -p aggregated_results/makespan
mkdir -p aggregated_latency

pids=()

run_python() {
  echo "Starting: python3 $*"
  python3 "$@" &
  pids+=("$!")
}

# Independent aggregations (can run in parallel)
run_python aggregate_results.py . aggregated_results
run_python aggregate_throughput_time.py . aggregated_throughput_time

status=0
for pid in "${pids[@]}"; do
  if ! wait "$pid"; then
    echo "Error: python3 job (pid $pid) failed" >&2
    status=1
  fi
done
if [[ "$status" -ne 0 ]]; then
  exit "$status"
fi

# Depends on aggregated_results/throughput from aggregate_results.py
echo "Starting: python3 aggregate_latency.py . aggregated_latency"
python3 aggregate_latency.py . aggregated_latency
