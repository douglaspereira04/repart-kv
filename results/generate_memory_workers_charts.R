#!/usr/bin/env Rscript
#
# Generate memory-usage vs workers charts from aggregated throughput-time CSV files.
# For each engine, shows mean memory (MiB) per worker count with ±1 standard deviation
# (computed over the experiment time series).
#
# Usage:
#   Rscript generate_memory_workers_charts.R [input_path] [output_path] [max_y] [max_x] [width] [height] [show_titles]

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
  default_input = "./aggregated_throughput_time",
  default_output = "./charts/memory_workers"
)
input_path <- chart_opts$input_path
output_path <- chart_opts$output_path

if (!dir.exists(input_path)) {
  stop(paste("Error: Input path '", input_path, "' is not a directory", sep = ""))
}

dir.create(output_path, showWarnings = FALSE, recursive = TRUE)

jobs <- summarize_resource_by_workers_jobs(input_path, "memory_kb_mean")
if (length(jobs) == 0) {
  warning(paste("Warning: No memory data found in '", input_path, "'", sep = ""))
  quit(status = 0)
}

cat(paste("Found", length(jobs), "memory-by-workers chart job(s)\n"))

for (job in jobs) {
  partition_setting <- job$setting
  subset_data <- job$data
  workload <- job$workload
  storage_engine <- job$storage_engine
  thinking_time <- job$thinking_time
  sync_mode <- job$sync_mode

  p_parts <- partition_chart_filename_parts(partition_setting)
  subset_data$storage_type_label <- make_storage_type_label(subset_data)
  subset_data$workers_factor <- factor(subset_data$workers, levels = sort(unique(subset_data$workers)))

  safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
  safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")

  cat(paste("Generating memory-by-workers chart for", workload, "-", storage_engine,
            "(Thinking time:", thinking_time, "ns, Sync:", sync_mode,
            if (length(p_parts)) paste0(", ", p_parts) else "", ") ...\n"))

  dodge <- position_dodge(width = 0.75, preserve = "single")

  p <- ggplot(subset_data, aes(
    x = workers_factor,
    y = usage_mean_mib,
    color = storage_type_label,
    group = storage_type_label
  )) +
    geom_errorbar(
      aes(ymin = usage_mean_mib - usage_sd_mib, ymax = usage_mean_mib + usage_sd_mib),
      position = dodge,
      width = 0.18,
      linewidth = 0.9,
      alpha = 0.9
    ) +
    geom_point(size = 3.5, alpha = 0.9, position = dodge) +
    labs(
      x = "Number of Workers",
      y = "Memory (MiB)",
      title = paste("Memory vs Workers:", workload, "-", storage_engine),
      subtitle = partition_chart_subtitle(
        partition_setting, sync_mode,
        paste("Thinking time:", thinking_time, "ns | Error bars = temporal std dev"),
        storage_engine
      ),
      color = "Storage Type"
    ) +
    chart_theme() +
    scale_color_brewer(palette = "Set1") +
    scale_y_continuous(
      expand = expansion(mult = c(0, 0.15)),
      limits = y_axis_limits(chart_opts$max_y),
      breaks = pretty_breaks(n = 10)
    )

  p <- add_coord_axis_limits(p, chart_opts)
  p <- apply_chart_titles(p, chart_opts)

  output_file <- file.path(
    output_path,
    paste(c(safe_workload, safe_engine, thinking_time, sync_mode, p_parts, "memory_workers.png"), collapse = ".")
  )

  ggsave_chart(output_file, p, chart_opts)
  cat(paste("Generated chart:", output_file, "\n"))
}

cat(paste("\nAll memory-by-workers charts saved to:", output_path, "\n"))
