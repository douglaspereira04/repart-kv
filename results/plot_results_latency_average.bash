#!/bin/bash
# Generate charts using aggregated_latency_latency_average (from per-rep latency_median_ns/latency_p95_ns)

mkdir -p charts/throughput_time
mkdir -p charts/throughput
mkdir -p charts/makespan
mkdir -p charts/latency_latency_average
mkdir -p charts/throughput_thinking_time
mkdir -p charts/throughput_latency_latency_average
mkdir -p charts/throughput_latency_by_thinking_time_latency_average

# Generate throughput-time charts
Rscript generate_throughput_time_charts.R aggregated_throughput_time charts/throughput_time

# Generate throughput charts
Rscript generate_charts.R aggregated_results/throughput charts/throughput

# Generate throughput vs thinking-time charts
Rscript generate_throughput_thinking_time_charts.R aggregated_results/throughput charts/throughput_thinking_time

# Generate makespan charts
Rscript generate_makespan_charts.R aggregated_results/makespan charts/makespan

# Generate latency charts (latency_average pipeline)
Rscript generate_latency_charts_latency_average.R aggregated_latency_latency_average charts/latency_latency_average

# Generate throughput vs latency charts
Rscript generate_throughput_latency_charts_latency_average.R aggregated_latency_latency_average charts/throughput_latency_latency_average

# Generate throughput vs latency by thinking time charts
Rscript generate_throughput_latency_by_thinking_time_charts_latency_average.R aggregated_latency_latency_average charts/throughput_latency_by_thinking_time_latency_average
