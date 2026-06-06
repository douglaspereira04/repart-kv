#!/usr/bin/env Rscript
#
# Generate makespan charts from aggregated CSV metrics files.
#
# Usage:
#   Rscript generate_makespan_charts.R [input_path] [output_path] [max_y] [max_x] [width] [height] [show_titles]
#
# Arguments:
#   input_path:  Directory containing aggregated makespan CSV files (default: ./aggregated_results/makespan)
#   output_path: Directory to save charts (default: ./charts/makespan)
#   max_y:       Maximum Y-axis value; 0 = automatic (default: 0)
#   max_x:       Maximum X-axis value; 0 = automatic (default: 0)
#   width:       Chart width in pixels; 0 = 3600 (default: 0)
#   height:      Chart height in pixels; 0 = 1800 (default: 0)

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
  default_input = "./aggregated_results/makespan",
  default_output = "./charts/makespan"
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

cat(paste("Found", length(csv_files), "aggregated makespan CSV file(s)\n"))

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
  thinking_times <- sort(unique(data$thinking_time))

  safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
  safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")

  for (tt in thinking_times) {
    tt_data <- data %>% filter(thinking_time == tt)
    if (nrow(tt_data) == 0) next

    for (job in partition_chart_jobs(tt_data)) {
      partition_setting <- job$setting
      subset_data <- job$data
      if (nrow(subset_data) == 0) next

      p_parts <- partition_chart_filename_parts(partition_setting)
      subset_data$storage_type_label <- make_storage_type_label(subset_data)
      subset_data$workers_factor <- factor(subset_data$workers, levels = sort(unique(subset_data$workers)))

      cat(paste("Generating makespan chart for", workload, "-", storage_engine,
                "(Thinking time:", tt, "ns, Sync:", sync_mode,
                if (length(p_parts)) paste0(", ", p_parts) else "", ") ...\n"))

      p <- ggplot(subset_data, aes(x = workers_factor, y = makespan_s, fill = storage_type_label)) +
        geom_bar(stat = "identity", position = position_dodge(width = 0.8), width = 0.7, alpha = 0.8) +
        labs(
          x = "Number of Workers",
          y = "Makespan (seconds)",
          title = paste("Execution Time:", workload, "-", storage_engine),
          subtitle = partition_chart_subtitle(partition_setting, sync_mode, paste("Thinking time:", tt, "ns"), storage_engine),
          fill = "Storage Type"
        ) +
        chart_theme() +
        scale_fill_brewer(palette = "Set1") +
        scale_y_continuous(
          expand = expansion(mult = c(0, 0.15)),
          limits = y_axis_limits(chart_opts$max_y),
          breaks = pretty_breaks(n = 10)
        )

      p <- add_coord_axis_limits(p, chart_opts)
      p <- apply_chart_titles(p, chart_opts)

      output_file <- file.path(
        output_path,
        paste(c(safe_workload, safe_engine, tt, sync_mode, p_parts, "makespan.png"), collapse = ".")
      )

      ggsave_chart(output_file, p, chart_opts)
      cat(paste("Generated chart:", output_file, "\n"))
    }
  }
}

cat(paste("\nAll makespan charts saved to:", output_path, "\n"))
