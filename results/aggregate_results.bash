#!/bin/bash
mkdir -p aggregated_throughput_time
mkdir -p aggregated_results/throughput
mkdir -p aggregated_results/makespan
mkdir -p aggregated_latency

# Aggregate results into throughput, makespan, and latency CSVs
python3 aggregate_results.py . aggregated_results
python3 aggregate_throughput_time.py . aggregated_throughput_time
python3 aggregate_latency.py . aggregated_latency