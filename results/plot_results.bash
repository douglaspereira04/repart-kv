#!/bin/bash

# Aggregate results into throughput and makespan CSVs
python3 aggregate_results.py . aggregated_results
python3 aggregate_throughput_time.py . aggregated_throughput_time

# Generate throughput-time charts
Rscript generate_throughput_time_charts.R aggregated_throughput_time charts/throughput_time

# Generate throughput charts
Rscript generate_charts.R aggregated_results/throughput charts/throughput

# Generate makespan charts
Rscript generate_makespan_charts.R aggregated_results/makespan charts/makespan
