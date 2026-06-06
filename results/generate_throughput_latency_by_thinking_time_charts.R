#!/usr/bin/env Rscript
#
# Generate throughput vs latency charts from aggregated latency CSV files.
# Each chart is for a fixed thinking_time; points on each line = different workers.
#
# Usage:
#   Rscript generate_throughput_latency_by_thinking_time_charts.R [input_path] [output_path] [max_y] [max_x] [width] [height] [show_titles]
#
# Arguments:
#   input_path:  Directory containing aggregated latency CSV files
#                (default: ./aggregated_latency)
#   output_path: Directory to save charts (default: ./charts/throughput_latency_by_thinking_time)
#   max_y:       Maximum Y-axis value; 0 = automatic (default: 0)
#   max_x:       Maximum X-axis value; 0 = automatic (default: 0)
#   width:       Chart width in pixels; 0 = 3600 (default: 0)
#   height:      Chart height in pixels; 0 = 1800 (default: 0)
#
# Creates four charts per (workload, storage_engine, thinking_time) combination:
#   - throughput_latency_median:  X=throughput, Y=latency median
#   - throughput_latency_average: X=throughput, Y=latency average
#   - throughput_latency_p75:     X=throughput, Y=latency 75th percentile
#   - throughput_latency_p95:     X=throughput, Y=latency 95th percentile
#
# Each line = (storage_type, partitions, paths, interval).
# Points on each line = (throughput, latency) for each workers setting.
# When a matching workload__rocksdb__<sync>.csv exists, its engine (p=1) series is
# included on every chart and labeled "rocksdb".
# Points are connected in order of increasing workers (geom_path preserves data order;
# geom_line would reorder by x and hide the throughput "knee" when more threads = less throughput).

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
  default_input = "./aggregated_latency",
  default_output = "./charts/throughput_latency_by_thinking_time"
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

cat(paste("Found", length(csv_files), "aggregated latency CSV file(s)\n"))

clear_rocksdb_engine_cache()

# Create output directory
dir.create(output_path, showWarnings = FALSE, recursive = TRUE)

# Process each aggregated file
for (csv_file in csv_files) {
  data <- read.csv(csv_file)
  if (nrow(data) == 0) next

  # Require throughput for X-axis
  if (!"throughput_ops_per_sec" %in% names(data)) {
    warning(paste("Skipping", csv_file, ": missing throughput_ops_per_sec column"))
    next
  }

  # Filter out rows without valid throughput
  data <- data %>% filter(!is.na(throughput_ops_per_sec) & throughput_ops_per_sec != "" & throughput_ops_per_sec > 0)
  if (nrow(data) == 0) next

  # Ensure numeric
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
      subset_data$latency_average <- subset_data$latency_average / 1000
      subset_data$latency_75 <- subset_data$latency_75 / 1000
      subset_data$latency_95 <- subset_data$latency_95 / 1000

      metrics <- list(
        list(id = "median", col = "latency_median", label = "Latency Median (µs)", title = "Median", suffix = "latency_median"),
        list(id = "average", col = "latency_average", label = "Latency Average (µs)", title = "Average", suffix = "latency_average"),
        list(id = "p75", col = "latency_75", label = "Latency 75th Percentile (µs)", title = "75th pctl", suffix = "latency_p75"),
        list(id = "p95", col = "latency_95", label = "Latency 95th Percentile (µs)", title = "95th pctl", suffix = "latency_p95")
      )

      for (metric in metrics) {
        y_col <- metric$col
        y_label <- metric$label
        metric_suffix <- metric$suffix

        cat(paste("Generating throughput vs", metric$id, "latency chart for", workload, "-", storage_engine,
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
            title = paste("Throughput vs Latency (", metric$title, "):", workload, "-", storage_engine),
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
