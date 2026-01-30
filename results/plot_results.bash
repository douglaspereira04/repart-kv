#!/bin/bash

python3 aggregate_results.py . aggregated_results
Rscript generate_charts.R aggregated_results charts