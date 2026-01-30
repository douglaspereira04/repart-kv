#!/bin/bash

# Aggregate results into throughput and makespan CSVs
python3 aggregate_results.py . aggregated_results

# Generate throughput charts
Rscript generate_charts.R aggregated_results/throughput charts/throughput

# Generate makespan charts
Rscript generate_makespan_charts.R aggregated_results/makespan charts/makespan
