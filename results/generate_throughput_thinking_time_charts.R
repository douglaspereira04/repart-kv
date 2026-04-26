#!/usr/bin/env Rscript
#
# Generate throughput vs thinking-time charts from aggregated throughput CSV files.
#
# Usage:
#   Rscript generate_throughput_thinking_time_charts.R [input_path] [output_path]
#
# Arguments:
#   input_path:  Directory containing aggregated throughput CSV files
#                (default: ./aggregated_results/throughput)
#   output_path: Directory to save charts (default: ./charts/throughput_thinking_time)
#
# Creates one chart per (workload, storage_engine, workers) combination:
#   Throughput (ops/s) vs Thinking time (ns)

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

# Parse command line arguments
args <- commandArgs(trailingOnly = TRUE)
input_path <- if (length(args) >= 1) args[1] else "./aggregated_results/throughput"
output_path <- if (length(args) >= 2) args[2] else "./charts/throughput_thinking_time"

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

  # Create storage type labels (p=partitions, d=paths, i=interval)
  data$storage_type_label <- paste0(
    data$storage_type,
    " (p=", data$partitions,
    ", d=", data$paths,
    ", i=", data$interval / 1000, "s)"
  )

  # Get unique workers for this workload+engine
  workers_list <- sort(unique(data$workers))

  for (num_workers in workers_list) {
    subset_data <- data %>% filter(workers == num_workers)
    if (nrow(subset_data) == 0) next

    # Factor for thinking_time to ensure proper ordering on X-axis
    subset_data$thinking_time_factor <- factor(
      subset_data$thinking_time,
      levels = sort(unique(subset_data$thinking_time))
    )

    safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
    safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")

    cat(paste("Generating throughput vs thinking-time chart for", workload, "-", storage_engine, "(Workers:", num_workers, ", Sync:", sync_mode, ") ...\n"))

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
        subtitle = paste("Workers:", num_workers, "| Sync:", sync_mode),
        color = "Storage Type",
        linetype = "Storage Type",
        shape = "Storage Type"
      ) +
      theme_minimal() +
      theme(
        plot.title = element_text(size = 20, face = "bold", hjust = 0.5),
        plot.subtitle = element_text(size = 14, hjust = 0.5),
        axis.title = element_text(size = 16),
        axis.text = element_text(size = 14),
        legend.title = element_text(size = 14, face = "bold"),
        legend.text = element_text(size = 12),
        legend.position = "right",
        panel.grid.major = element_line(color = "gray90", linewidth = 0.5),
        panel.grid.minor = element_line(color = "gray95", linewidth = 0.25)
      ) +
      scale_color_brewer(palette = "Set1") +
      scale_linetype_manual(values = c("solid", "dashed", "dotted", "dotdash", "longdash", "twodash", "11", "22", "44")) +
      scale_shape_manual(values = c(16, 17, 18, 15, 3, 4, 8, 1, 2)) +
      scale_y_continuous(
        expand = expansion(mult = c(0, 0.15)),
        limits = c(0, NA),
        breaks = pretty_breaks(n = 10)
      )

    output_file <- file.path(output_path, paste(safe_workload, safe_engine, num_workers, sync_mode, "throughput_thinking_time.png", sep = "."))
    ggsave(output_file, plot = p, width = 12, height = 6, dpi = 300, bg = "white")
    cat(paste("Generated chart:", output_file, "\n"))
  }
}

cat(paste("\nAll throughput vs thinking-time charts saved to:", output_path, "\n"))
