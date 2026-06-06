#!/usr/bin/env Rscript
#
# Generate throughput vs thinking-time charts from aggregated throughput CSV files.
#
# Usage:
#   Rscript generate_throughput_thinking_time_charts.R [input_path] [output_path] [max_y] [max_x] [width] [height] [show_titles]
#
# Arguments:
#   input_path:  Directory containing aggregated throughput CSV files
#                (default: ./aggregated_results/throughput)
#   output_path: Directory to save charts (default: ./charts/throughput_thinking_time)
#   max_y:       Maximum Y-axis value; 0 = automatic (default: 0)
#   max_x:       Maximum X-axis value; 0 = automatic (default: 0)
#   width:       Chart width in pixels; 0 = 3600 (default: 0)
#   height:      Chart height in pixels; 0 = 1800 (default: 0)
#
# Creates one chart per (workload, storage_engine, workers) combination:
#   Throughput (ops/s) vs Thinking time (ns)

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

source("chart_partition_utils.R")

args <- commandArgs(trailingOnly = TRUE)
chart_opts <- parse_chart_cli_args(
  args,
  default_input = "./aggregated_results/throughput",
  default_output = "./charts/throughput_thinking_time"
)
input_path <- chart_opts$input_path
output_path <- chart_opts$output_path

# Validate input path
if (!dir.exists(input_path)) {
  stop(paste("Error: Input path '", input_path, "' is not a directory", sep = ""))
}

# Find all CSV files
csv_files <- list.files(path = input_path, pattern = "\\.csv$", full.names = TRUE)

if (length(csv_files) == 0) {
  warning(paste("Warning: No CSV files found in '", input_path, "'", sep = ""))
  quit(status = 0)
}

cat(paste("Found", length(csv_files), "aggregated throughput CSV file(s)\n"))

# Create output directory
dir.create(output_path, showWarnings = FALSE, recursive = TRUE)

# Process each aggregated file
for (csv_file in csv_files) {
  data <- read.csv(csv_file)
  if (nrow(data) == 0) next

  if (!"thinking_time" %in% names(data)) {
    data$thinking_time <- 0
  }
  if (!"sync_mode" %in% names(data)) {
    data$sync_mode <- "sync_off"
  }

  workload <- unique(data$workload)
  storage_engine <- unique(data$storage_engine)
  sync_mode <- unique(data$sync_mode)

  workers_list <- sort(unique(data$workers))
  safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
  safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")

  for (job in partition_chart_jobs(data)) {
    partition_setting <- job$setting
    chart_data <- job$data
    if (nrow(chart_data) == 0) next

    p_parts <- partition_chart_filename_parts(partition_setting)

    for (num_workers in workers_list) {
      subset_data <- chart_data %>% filter(workers == num_workers)
      if (nrow(subset_data) == 0) next

      subset_data$storage_type_label <- make_storage_type_label(subset_data)
      subset_data$thinking_time_factor <- factor(
        subset_data$thinking_time,
        levels = sort(unique(subset_data$thinking_time))
      )

      cat(paste("Generating throughput vs thinking-time chart for", workload, "-", storage_engine,
                "(Workers:", num_workers, ", Sync:", sync_mode,
                if (length(p_parts)) paste0(", ", p_parts) else "", ") ...\n"))

      p <- ggplot(subset_data, aes(x = thinking_time_factor, y = ops_per_second / 1000,
                                  color = storage_type_label,
                                  linetype = storage_type_label,
                                  shape = storage_type_label,
                                  group = storage_type_label)) +
        geom_line(linewidth = 1.2, alpha = 0.8) +
        geom_point(size = 4, alpha = 0.8) +
        labs(
          x = "Thinking Time (ns)",
          y = "Thousand Operations per Second",
          title = paste("Throughput vs Thinking Time:", workload, "-", storage_engine),
          subtitle = partition_chart_subtitle(partition_setting, sync_mode, paste("Workers:", num_workers), storage_engine),
          color = "Storage Type",
          linetype = "Storage Type",
          shape = "Storage Type"
        ) +
        chart_theme() +
        scale_color_brewer(palette = "Set1") +
        scale_linetype_manual(values = c("solid", "dashed", "dotted", "dotdash", "longdash", "twodash", "11", "22", "44")) +
        scale_shape_manual(values = c(16, 17, 18, 15, 3, 4, 8, 1, 2)) +
        scale_y_continuous(
          expand = expansion(mult = c(0, 0.15)),
          limits = y_axis_limits(chart_opts$max_y),
          breaks = pretty_breaks(n = 10)
        )

      p <- add_coord_axis_limits(p, chart_opts)
      p <- apply_chart_titles(p, chart_opts)

      output_file <- file.path(
        output_path,
        paste(c(safe_workload, safe_engine, num_workers, sync_mode, p_parts, "throughput_thinking_time.png"), collapse = ".")
      )
      ggsave_chart(output_file, p, chart_opts)
      cat(paste("Generated chart:", output_file, "\n"))
    }
  }
}

cat(paste("\nAll throughput vs thinking-time charts saved to:", output_path, "\n"))
