#!/bin/bash

mkdir -p charts/throughput_time
mkdir -p charts/throughput
mkdir -p charts/makespan
mkdir -p charts/latency
mkdir -p charts/throughput_thinking_time

mkdir -p aggregated_throughput_time
mkdir -p aggregated_results/throughput
mkdir -p aggregated_results/makespan
mkdir -p aggregated_latency

# Aggregate results into throughput, makespan, and latency CSVs
python3 aggregate_results.py . aggregated_results
python3 aggregate_throughput_time.py . aggregated_throughput_time
python3 aggregate_latency.py . aggregated_latency

# Generate throughput-time charts
Rscript generate_throughput_time_charts.R aggregated_throughput_time charts/throughput_time

# Generate throughput charts
Rscript generate_charts.R aggregated_results/throughput charts/throughput

# Generate throughput vs thinking-time charts
Rscript generate_throughput_thinking_time_charts.R aggregated_results/throughput charts/throughput_thinking_time

# Generate makespan charts
Rscript generate_makespan_charts.R aggregated_results/makespan charts/makespan

# Generate latency charts
Rscript generate_latency_charts.R aggregated_latency charts/latency
