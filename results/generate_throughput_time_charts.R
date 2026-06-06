#!/usr/bin/env Rscript
#
# Generate throughput-over-time charts with range bands from aggregated CSV files.
# Data points are sampled every 100ms (elapsed_s has 0.01s resolution).
#
# Usage:
#   Rscript generate_throughput_time_charts.R [input_path] [output_path] [max_y] [max_x] [width] [height] [show_titles]
#
# Arguments:
#   max_y:   Maximum Y-axis value; 0 = automatic (default: 0)
#   max_x:   Maximum X-axis value; 0 = automatic (default: 0)
#   width:   Chart width in pixels; 0 = 3600 (default: 0)
#   height:  Chart height in pixels; 0 = 2400 (default: 0)
#

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
  default_input = "/home/douglas/Documentos/repart-kv/results/aggregated_throughput_time",
  default_output = "/home/douglas/Documentos/repart-kv/results/charts/throughput_time",
  default_height_px = 2400L
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

cat(paste("Found", length(csv_files), "aggregated CSV file(s)\n"))

# Create output directory
dir.create(output_path, showWarnings = FALSE, recursive = TRUE)

# Process each aggregated file
for (csv_file in csv_files) {
  data <- read.csv(csv_file)
  if (nrow(data) == 0) next
  
  # Extract info from filename
  # (e.g., ycsb_a__lmdb__1__10__sync_off.csv or backward-compatible variants)
  filename <- basename(csv_file)
  parts <- str_split(str_remove(filename, "\\.csv$"), "__")[[1]]
  workload <- parts[1]
  storage_engine <- parts[2]
  num_workers <- parts[3]
  thinking_time <- if (length(parts) >= 4) parts[4] else "0"
  sync_mode <- if (length(parts) >= 5) parts[5] else "sync_off"

  if (!"thinking_time" %in% names(data)) {
    data$thinking_time <- 0
  }

  for (job in partition_chart_jobs(data)) {
    partition_setting <- job$setting
    chart_data <- job$data
    if (nrow(chart_data) == 0) next

    p_parts <- partition_chart_filename_parts(partition_setting)

    cat(paste("Generating chart for", workload, "-", storage_engine,
              "(Workers:", num_workers, ", Thinking:", thinking_time, "ns, Sync:", sync_mode,
              if (length(p_parts)) paste0(", ", p_parts) else "", ") ...\n"))

    chart_data$label <- make_time_series_label(chart_data, storage_engine)

    chart_data <- chart_data %>%
      arrange(label, elapsed_s) %>%
      group_by(label) %>%
      mutate(
        repartitioning_active = repartitioning_prob > 0,
        repartitioning_start = repartitioning_active & !lag(repartitioning_active, default = FALSE),
        repartitioning_end = !repartitioning_active & lag(repartitioning_active, default = FALSE)
      ) %>%
      ungroup()

    repartitioning_starts <- chart_data %>% filter(repartitioning_start)
    repartitioning_ends <- chart_data %>% filter(repartitioning_end)

    p <- ggplot(chart_data, aes(x = elapsed_s, y = mean / 1000, color = label, fill = label)) +
      geom_ribbon(aes(ymin = min / 1000, ymax = max / 1000), alpha = 0.2, color = NA) +
      geom_line(linewidth = 1, alpha = 0.6) +
      geom_point(data = repartitioning_starts,
                 aes(x = elapsed_s, y = mean / 1000, color = label),
                 alpha = 0.4, size = 3, shape = 16, inherit.aes = FALSE) +
      geom_point(data = repartitioning_ends,
                 aes(x = elapsed_s, y = mean / 1000, color = label),
                 alpha = 1.0, size = 3, shape = 16, inherit.aes = FALSE) +
      labs(
        x = "Elapsed Time (s)",
        y = "Thousand Operations per Second",
        title = paste("Throughput over Time:", workload, "-", storage_engine),
        subtitle = partition_chart_subtitle(
          partition_setting, sync_mode,
          paste("Workers:", num_workers, "| Thinking time:", thinking_time, "ns"),
          storage_engine
        ),
        color = "Configuration",
        fill = "Configuration"
      ) +
      chart_theme(legend_position = "bottom", legend_direction = "vertical") +
      scale_color_brewer(palette = "Set1") +
      scale_fill_brewer(palette = "Set1") +
      scale_x_continuous(
        breaks = pretty_breaks(n = 10),
        expand = expansion(mult = c(0.02, 0)),
        limits = x_axis_limits(chart_opts$max_x)
      ) +
      scale_y_continuous(
        expand = expansion(mult = c(0, 0.1)),
        limits = y_axis_limits(chart_opts$max_y),
        breaks = pretty_breaks(n = 8)
      )

    p <- add_coord_axis_limits(p, chart_opts)
    p <- apply_chart_titles(p, chart_opts)

    output_file <- file.path(
      output_path,
      paste(c(workload, storage_engine, num_workers, thinking_time, sync_mode, p_parts, "throughput_time.png"), collapse = ".")
    )

    ggsave_chart(output_file, p, chart_opts)
    cat(paste("Generated chart:", output_file, "\n"))
  }
}

cat(paste("\nAll charts saved to:", output_path, "\n"))
