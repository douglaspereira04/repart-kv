#!/usr/bin/env Rscript
#
# Generate latency vs thinking-time charts from aggregated latency CSV files.
# Uses aggregated_latency_latency_average (from per-rep latency_median_ns/latency_p95_ns).
#
# Usage:
#   Rscript generate_latency_charts_latency_average.R [input_path] [output_path]
#
# Arguments:
#   input_path:  Directory containing aggregated latency CSV files (default: ./aggregated_latency_latency_average)
#   output_path: Directory to save charts (default: ./charts/latency_latency_average)
#
# Creates two charts per (workload, storage_engine, workers) combination:
# - latency_median: median latency vs thinking time (µs)
# - latency_p95: 95th percentile latency vs thinking time (µs)

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

# Parse command line arguments
args <- commandArgs(trailingOnly = TRUE)
input_path <- if (length(args) >= 1) args[1] else "./aggregated_latency_latency_average"
output_path <- if (length(args) >= 2) args[2] else "./charts/latency_latency_average"

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

# Create output directory
dir.create(output_path, showWarnings = FALSE, recursive = TRUE)

# Process each aggregated file
for (csv_file in csv_files) {
  data <- read.csv(csv_file)
  if (nrow(data) == 0) next

  if (!"thinking_time" %in% names(data)) {
    data$thinking_time <- 0
  }

  workload <- unique(data$workload)
  storage_engine <- unique(data$storage_engine)

  # Create storage type labels
  data$storage_type_label <- paste0(
    data$storage_type,
    " (p=", data$partitions,
    ", d=", data$paths,
    ", i=", data$interval / 1000, "s)"
  )

  workers_list <- sort(unique(data$workers))

  for (num_workers in workers_list) {
    subset_data <- data %>% filter(workers == num_workers)
    if (nrow(subset_data) == 0) next

    # Convert latency from ns to µs (CSV data is always in nanoseconds)
    subset_data$latency_median <- subset_data$latency_median / 1000
    subset_data$latency_95 <- subset_data$latency_95 / 1000

    subset_data$thinking_time_factor <- factor(subset_data$thinking_time, levels = sort(unique(subset_data$thinking_time)))

    safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
    safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")

    for (metric in c("median", "p95")) {
      y_col <- if (metric == "median") "latency_median" else "latency_95"
      y_label <- if (metric == "median") "Latency Median (µs)" else "Latency 95th Percentile (µs)"
      metric_title <- if (metric == "median") "Latency (Median)" else "Latency (95th Percentile)"

      cat(paste("Generating", metric, "latency chart for", workload, "-", storage_engine, "(Workers:", num_workers, ") ...\n"))

      p <- ggplot(subset_data, aes(x = thinking_time_factor, y = .data[[y_col]],
                                  color = storage_type_label,
                                  linetype = storage_type_label,
                                  shape = storage_type_label,
                                  group = storage_type_label)) +
        geom_line(linewidth = 1.2, alpha = 0.8) +
        geom_point(size = 4, alpha = 0.8) +
        labs(
          x = "Thinking Time (ns)",
          y = y_label,
          title = paste(metric_title, ":", workload, "-", storage_engine),
          subtitle = paste("Workers:", num_workers),
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

      output_file <- file.path(output_path, paste(safe_workload, safe_engine, num_workers, paste0("latency_", metric), "png", sep = "."))
      ggsave(output_file, plot = p, width = 12, height = 6, dpi = 300, bg = "white")
      cat(paste("Generated chart:", output_file, "\n"))
    }
  }
}

cat(paste("\nAll latency charts saved to:", output_path, "\n"))
