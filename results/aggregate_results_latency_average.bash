#!/bin/bash
# Aggregate results: throughput, makespan, and latency (from per-rep latency_median_ns/latency_p95_ns CSVs)
mkdir -p aggregated_throughput_time
mkdir -p aggregated_results/throughput
mkdir -p aggregated_results/makespan
mkdir -p aggregated_latency_latency_average

# Aggregate results into throughput and makespan
python3 aggregate_results.py . aggregated_results
python3 aggregate_throughput_time.py . aggregated_throughput_time

# Aggregate latency from new-format CSVs (latency_median_ns, latency_p95_ns per file)
python3 aggregate_latency_latency_average.py . aggregated_latency_latency_average aggregated_results/throughput
