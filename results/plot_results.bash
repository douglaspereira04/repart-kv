#!/bin/bash

mkdir -p charts/throughput_time
mkdir -p charts/throughput
mkdir -p charts/makespan
mkdir -p charts/latency
mkdir -p charts/throughput_thinking_time
mkdir -p charts/throughput_latency
mkdir -p charts/throughput_latency_by_thinking_time

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

# Generate throughput vs latency charts (requires aggregate_latency with throughput from aggregate_results)
Rscript generate_throughput_latency_charts.R aggregated_latency charts/throughput_latency

# Generate throughput vs latency charts (one per thinking_time; points = workers)
Rscript generate_throughput_latency_by_thinking_time_charts.R aggregated_latency charts/throughput_latency_by_thinking_time
