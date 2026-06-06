#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

mkdir -p charts/throughput_time
mkdir -p charts/memory_time
mkdir -p charts/memory_workers
mkdir -p charts/disk_time
mkdir -p charts/disk_workers
mkdir -p charts/throughput
mkdir -p charts/makespan
mkdir -p charts/latency
mkdir -p charts/throughput_thinking_time
mkdir -p charts/throughput_latency
mkdir -p charts/throughput_latency_by_thinking_time

pids=()

run_rscript() {
  echo "Starting: Rscript $*"
  Rscript "$@" &
  pids+=("$!")
}

CHART_MAX_Y=0
CHART_MAX_X=0
CHART_WIDTH_PX=3000
CHART_HEIGHT_PX=1000
CHART_SHOW_TITLES=0
THROUGHPUT_LATENCY_CHART_MAX_Y=0


run_rscript generate_throughput_time_charts.R aggregated_throughput_time charts/throughput_time "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
run_rscript generate_memory_time_charts.R aggregated_throughput_time charts/memory_time "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
run_rscript generate_memory_workers_charts.R aggregated_throughput_time charts/memory_workers "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
run_rscript generate_disk_time_charts.R aggregated_throughput_time charts/disk_time "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
run_rscript generate_disk_workers_charts.R aggregated_throughput_time charts/disk_workers "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
#run_rscript generate_charts.R aggregated_results/throughput charts/throughput "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
#run_rscript generate_throughput_thinking_time_charts.R aggregated_results/throughput charts/throughput_thinking_time "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
#run_rscript generate_makespan_charts.R aggregated_results/makespan charts/makespan "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
#run_rscript generate_latency_charts.R aggregated_latency charts/latency "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
#run_rscript generate_throughput_latency_charts.R aggregated_latency charts/throughput_latency "$CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"
run_rscript generate_throughput_latency_by_thinking_time_charts.R aggregated_latency charts/throughput_latency_by_thinking_time "$THROUGHPUT_LATENCY_CHART_MAX_Y" "$CHART_MAX_X" "$CHART_WIDTH_PX" "$CHART_HEIGHT_PX" "$CHART_SHOW_TITLES"

status=0
for pid in "${pids[@]}"; do
  if ! wait "$pid"; then
    echo "Error: Rscript job (pid $pid) failed" >&2
    status=1
  fi
done

THROUGHPUT_LATENCY_BY_THINKING_TIME_DIR="charts/throughput_latency_by_thinking_time"
MEDIAN_DIR="${THROUGHPUT_LATENCY_BY_THINKING_TIME_DIR}/median"
mkdir -p "${MEDIAN_DIR}/sync_on" "${MEDIAN_DIR}/sync_off"

shopt -s nullglob
for chart in "${THROUGHPUT_LATENCY_BY_THINKING_TIME_DIR}"/*sync_off*.throughput.latency_median.png; do
  case "$(basename "$chart")" in
    *rocksdb*) continue ;;
  esac
  cp -f "$chart" "${MEDIAN_DIR}/sync_off/"
done

for chart in "${THROUGHPUT_LATENCY_BY_THINKING_TIME_DIR}"/*sync_on*.throughput.latency_median.png; do
  case "$(basename "$chart")" in
    *rocksdb*) continue ;;
  esac
  cp -f "$chart" "${MEDIAN_DIR}/sync_on/"
done
shopt -u nullglob

exit "$status"
