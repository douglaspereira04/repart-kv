#!/usr/bin/env Rscript
#
# Generate throughput vs latency charts from aggregated latency CSV files.
# Each chart is for a fixed thinking_time; points on each line = different workers.
# Uses aggregated_latency_latency_average (from per-rep latency_median_ns/latency_p95_ns).
#
# Usage:
#   Rscript generate_throughput_latency_by_thinking_time_charts_latency_average.R [input_path] [output_path]
#
# Arguments:
#   input_path:  Directory (default: ./aggregated_latency_latency_average)
#   output_path: Directory (default: ./charts/throughput_latency_by_thinking_time_latency_average)
#
# Creates two charts per (workload, storage_engine, thinking_time):
#   - throughput_latency_median: X=throughput, Y=latency median
#   - throughput_latency_p95:     X=throughput, Y=latency 95th percentile
#
# Each line = (storage_type, partitions, paths, interval).
# Points on each line = (throughput, latency) for each workers setting.
# Points are connected in order of increasing workers (geom_path).

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

args <- commandArgs(trailingOnly = TRUE)
input_path <- if (length(args) >= 1) args[1] else "./aggregated_latency_latency_average"
output_path <- if (length(args) >= 2) args[2] else "./charts/throughput_latency_by_thinking_time_latency_average"

if (!dir.exists(input_path)) {
  stop(paste("Error: Input path '", input_path, "' is not a directory", sep = ""))
}

csv_files <- list.files(path = input_path, pattern = "\\.csv$", full.names = TRUE)
if (length(csv_files) == 0) {
  warning(paste("Warning: No CSV files found in '", input_path, "'", sep = ""))
  quit(status = 0)
}

cat(paste("Found", length(csv_files), "aggregated latency CSV file(s)\n"))
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

  workload <- unique(data$workload)
  storage_engine <- unique(data$storage_engine)

  data$line_label <- paste0(
    data$storage_type,
    " (p=", data$partitions,
    ", d=", data$paths,
    ", i=", data$interval / 1000, "s)"
  )

  thinking_times <- sort(unique(data$thinking_time))

  for (think_time in thinking_times) {
    subset_data <- data %>% filter(thinking_time == think_time)
    if (nrow(subset_data) == 0) next

    subset_data <- subset_data %>% arrange(line_label, workers)

    # Convert latency from ns to µs
    subset_data$latency_median <- subset_data$latency_median / 1000
    subset_data$latency_95 <- subset_data$latency_95 / 1000

    safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
    safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")

    for (metric in c("median", "p95")) {
      y_col <- if (metric == "median") "latency_median" else "latency_95"
      y_label <- if (metric == "median") "Latency Median (µs)" else "Latency 95th Percentile (µs)"
      metric_suffix <- if (metric == "median") "latency_median" else "latency_p95"

      cat(paste("Generating throughput vs", metric, "latency chart for", workload, "-", storage_engine, "(Thinking time:", think_time, "ns) ...\n"))

      p <- ggplot(subset_data, aes(x = throughput_ops_per_sec / 1000, y = .data[[y_col]],
                                   color = line_label,
                                   linetype = line_label,
                                   shape = line_label,
                                   group = line_label)) +
        geom_path(linewidth = 1.2, alpha = 0.8) +
        geom_point(size = 4, alpha = 0.8) +
        geom_text(aes(label = workers), vjust = -0.5, size = 3.5, show.legend = FALSE) +
        labs(
          x = "Thousand Operations per Second",
          y = y_label,
          title = paste("Throughput vs Latency (", if (metric == "median") "Median" else "95th pctl", "):", workload, "-", storage_engine),
          subtitle = paste("Thinking time:", think_time, "ns | Points = worker counts"),
          color = "Configuration",
          linetype = "Configuration",
          shape = "Configuration"
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
        scale_x_continuous(
          expand = expansion(mult = c(0.05, 0.05)),
          breaks = pretty_breaks(n = 10)
        ) +
        scale_y_continuous(
          expand = expansion(mult = c(0, 0.15)),
          limits = c(0, NA),
          breaks = pretty_breaks(n = 10)
        )

      output_file <- file.path(output_path, paste(safe_workload, safe_engine, think_time, "throughput", metric_suffix, "png", sep = "."))
      ggsave(output_file, plot = p, width = 12, height = 6, dpi = 300, bg = "white")
      cat(paste("Generated chart:", output_file, "\n"))
    }
  }
}

cat(paste("\nAll throughput vs latency (by thinking time) charts saved to:", output_path, "\n"))
