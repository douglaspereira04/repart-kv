#!/usr/bin/env Rscript
#
# Generate throughput vs latency charts from aggregated latency CSV files.
# Each chart is for a fixed thinking_time; points on each line = different workers.
# Uses aggregated_latency_latency_average (from per-rep latency_median_ns/latency_p95_ns).
#
# Usage:
#   Rscript generate_throughput_latency_by_thinking_time_charts_latency_average.R [input_path] [output_path] [max_y] [max_x] [width] [height] [show_titles]
#
# Arguments:
#   input_path:  Directory (default: ./aggregated_latency_latency_average)
#   output_path: Directory (default: ./charts/throughput_latency_by_thinking_time_latency_average)
#   max_y:       Maximum Y-axis value; 0 = automatic (default: 0)
#   max_x:       Maximum X-axis value; 0 = automatic (default: 0)
#   width:       Chart width in pixels; 0 = 3600 (default: 0)
#   height:      Chart height in pixels; 0 = 1800 (default: 0)
#
# Creates two charts per (workload, storage_engine, thinking_time, partition setting):
#   - throughput_latency_median: X=throughput, Y=latency median
#   - throughput_latency_p95:     X=throughput, Y=latency 95th percentile
#
# Each line = (storage_type, paths, interval) with matched partition settings.
# Points on each line = (throughput, latency) for each workers setting.

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
  default_input = "./aggregated_latency_latency_average",
  default_output = "./charts/throughput_latency_by_thinking_time_latency_average"
)
input_path <- chart_opts$input_path
output_path <- chart_opts$output_path

if (!dir.exists(input_path)) {
  stop(paste("Error: Input path '", input_path, "' is not a directory", sep = ""))
}

csv_files <- list.files(path = input_path, pattern = "\\.csv$", full.names = TRUE)
if (length(csv_files) == 0) {
  warning(paste("Warning: No CSV files found in '", input_path, "'", sep = ""))
  quit(status = 0)
}

cat(paste("Found", length(csv_files), "aggregated latency CSV file(s)\n"))

clear_rocksdb_engine_cache()

dir.create(output_path, showWarnings = FALSE, recursive = TRUE)

for (csv_file in csv_files) {
  data <- read.csv(csv_file)
  if (nrow(data) == 0) next

  if (!"throughput_ops_per_sec" %in% names(data)) {
    warning(paste("Skipping", csv_file, ": missing throughput_ops_per_sec column"))
    next
  }

  data <- data %>% filter(!is.na(throughput_ops_per_sec) & throughput_ops_per_sec != "" & throughput_ops_per_sec > 0)
  if (nrow(data) == 0) next

  data$throughput_ops_per_sec <- as.numeric(data$throughput_ops_per_sec)

  if (!"sync_mode" %in% names(data)) {
    data$sync_mode <- "sync_off"
  }

  workload <- unique(data$workload)
  storage_engine <- unique(data$storage_engine)
  sync_mode <- unique(data$sync_mode)

  thinking_times <- sort(unique(data$thinking_time))
  safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
  safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")
  rocksdb_engine_rows <- get_rocksdb_engine_rows(input_path, workload, sync_mode)

  for (job in partition_chart_jobs(data)) {
    partition_setting <- job$setting
    chart_data <- job$data
    if (nrow(chart_data) == 0) next

    p_parts <- partition_chart_filename_parts(partition_setting)

    for (think_time in thinking_times) {
      subset_data <- chart_data %>% filter(thinking_time == think_time)
      subset_data <- append_rocksdb_baseline_rows(
        subset_data, rocksdb_engine_rows, storage_engine, think_time
      )
      if (nrow(subset_data) == 0) next

      subset_data$line_label <- make_storage_type_label(subset_data)
      subset_data <- subset_data %>% arrange(line_label, workers)
      subset_data$latency_median <- subset_data$latency_median / 1000
      subset_data$latency_95 <- subset_data$latency_95 / 1000

      for (metric in c("median", "p95")) {
        y_col <- if (metric == "median") "latency_median" else "latency_95"
        y_label <- if (metric == "median") "Latency Median (µs)" else "Latency 95th Percentile (µs)"
        metric_suffix <- if (metric == "median") "latency_median" else "latency_p95"

        cat(paste("Generating throughput vs", metric, "latency chart for", workload, "-", storage_engine,
                  "(Thinking time:", think_time, "ns, Sync:", sync_mode,
                  if (length(p_parts)) paste0(", ", p_parts) else "", ") ...\n"))

        p <- ggplot(subset_data, aes(x = throughput_ops_per_sec / 1000, y = .data[[y_col]],
                                     color = line_label,
                                     linetype = line_label,
                                     shape = line_label,
                                     group = line_label)) +
          geom_path(linewidth = 1.2, alpha = 0.8) +
          geom_point(size = 4, alpha = 0.8) +
          geom_text(aes(label = workers), vjust = -0.5, size = chart_dot_label_size(), show.legend = FALSE) +
          labs(
            x = "Thousand Operations per Second",
            y = y_label,
            title = paste("Throughput vs Latency (", if (metric == "median") "Median" else "95th pctl", "):", workload, "-", storage_engine),
            subtitle = partition_chart_subtitle(
              partition_setting, sync_mode,
              paste("Thinking time:", think_time, "ns | Points = worker counts"),
              storage_engine
            ),
            color = "Configuration",
            linetype = "Configuration",
            shape = "Configuration"
          ) +
          chart_theme() +
          scale_color_brewer(palette = "Set1") +
          scale_linetype_manual(values = c("solid", "dashed", "dotted", "dotdash", "longdash", "twodash", "11", "22", "44")) +
          scale_shape_manual(values = c(16, 17, 18, 15, 3, 4, 8, 1, 2)) +
          scale_x_continuous(
            expand = expansion(mult = c(0.05, 0.05)),
            limits = x_axis_limits(chart_opts$max_x),
            breaks = pretty_breaks(n = 10)
          ) +
          scale_y_continuous(
            expand = expansion(mult = c(0, 0.15)),
            limits = y_axis_limits(chart_opts$max_y),
            breaks = pretty_breaks(n = 10)
          )

        p <- add_coord_axis_limits(p, chart_opts)
        p <- apply_chart_titles(p, chart_opts)

        output_file <- file.path(
          output_path,
          paste(c(safe_workload, safe_engine, think_time, sync_mode, p_parts, "throughput", metric_suffix, "png"), collapse = ".")
        )
        ggsave_chart(output_file, p, chart_opts)
        cat(paste("Generated chart:", output_file, "\n"))
      }
    }
  }
}

cat(paste("\nAll throughput vs latency (by thinking time) charts saved to:", output_path, "\n"))
