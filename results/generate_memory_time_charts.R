#!/usr/bin/env Rscript
#
# Generate memory-over-time charts with range bands from aggregated CSV files.
# Expects memory_kb_mean / memory_kb_min / memory_kb_max (KiB) from aggregate_throughput_time.py.
# Data points are sampled every 100ms (elapsed_s has 0.01s resolution).
#
# Usage:
#   Rscript generate_memory_time_charts.R [input_path] [output_path]
#

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

# Parse command line arguments
args <- commandArgs(trailingOnly = TRUE)
input_path <- if (length(args) >= 1) args[1] else "/home/douglas/Documentos/repart-kv/results/aggregated_throughput_time"
output_path <- if (length(args) >= 2) args[2] else "/home/douglas/Documentos/repart-kv/results/charts/memory_time"

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

  if (!all(c("memory_kb_mean", "memory_kb_min", "memory_kb_max") %in% names(data))) {
    warning(paste("Skipping (no memory columns):", csv_file))
    next
  }

  # Extract info from filename
  filename <- basename(csv_file)
  parts <- str_split(str_remove(filename, "\\.csv$"), "__")[[1]]
  workload <- parts[1]
  storage_engine <- parts[2]
  num_workers <- parts[3]
  thinking_time <- if (length(parts) >= 4) parts[4] else "0"
  sync_mode <- if (length(parts) >= 5) parts[5] else "sync_off"

  cat(paste("Generating chart for", workload, "-", storage_engine, "(Workers:", num_workers, ", Thinking:", thinking_time, "ns, Sync:", sync_mode, ") ...\n"))

  if (!"thinking_time" %in% names(data)) {
    data$thinking_time <- 0
  }

  # MiB for display (values are KiB from the benchmark)
  data$mem_mib_mean <- data$memory_kb_mean / 1024
  data$mem_mib_min <- data$memory_kb_min / 1024
  data$mem_mib_max <- data$memory_kb_max / 1024

  # Create nice labels for the legend
  data$label <- paste0(
    data$storage_type,
    " (p=", data$partitions,
    ", d=", data$paths,
    ", i=", data$interval / 1000, "s",
    ", t=", data$thinking_time, "ns)"
  )

  data <- data %>%
    arrange(label, elapsed_s) %>%
    group_by(label) %>%
    mutate(
      repartitioning_active = repartitioning_prob > 0,
      repartitioning_start = repartitioning_active & !lag(repartitioning_active, default = FALSE),
      repartitioning_end = !repartitioning_active & lag(repartitioning_active, default = FALSE)
    ) %>%
    ungroup()

  repartitioning_starts <- data %>% filter(repartitioning_start)
  repartitioning_ends <- data %>% filter(repartitioning_end)

  p <- ggplot(data, aes(x = elapsed_s, y = mem_mib_mean, color = label, fill = label)) +
    geom_ribbon(aes(ymin = mem_mib_min, ymax = mem_mib_max), alpha = 0.2, color = NA) +
    geom_line(linewidth = 1, alpha = 0.6) +
    geom_point(data = repartitioning_starts,
               aes(x = elapsed_s, y = mem_mib_mean, color = label),
               alpha = 0.4, size = 3, shape = 16, inherit.aes = FALSE) +
    geom_point(data = repartitioning_ends,
               aes(x = elapsed_s, y = mem_mib_mean, color = label),
               alpha = 1.0, size = 3, shape = 16, inherit.aes = FALSE) +
    labs(
      x = "Elapsed Time (s)",
      y = "Memory (MiB)",
      title = paste("Memory over Time:", workload, "-", storage_engine),
      subtitle = paste("Workers:", num_workers, "| Thinking time:", thinking_time, "ns | Sync:", sync_mode),
      color = "Configuration",
      fill = "Configuration"
    ) +
    theme_minimal() +
    theme(
      plot.title = element_text(size = 18, face = "bold", hjust = 0.5),
      plot.subtitle = element_text(size = 14, hjust = 0.5),
      axis.title = element_text(size = 14),
      axis.text = element_text(size = 12),
      legend.title = element_text(size = 12, face = "bold"),
      legend.text = element_text(size = 10),
      legend.position = "bottom",
      legend.direction = "vertical",
      panel.grid.major = element_line(color = "gray90", linewidth = 0.5),
      panel.grid.minor = element_line(color = "gray95", linewidth = 0.25)
    ) +
    scale_color_brewer(palette = "Set1") +
    scale_fill_brewer(palette = "Set1") +
    scale_x_continuous(
      breaks = pretty_breaks(n = 10),
      expand = expansion(mult = c(0.02, 0))
    ) +
    scale_y_continuous(
      expand = expansion(mult = c(0, 0.1)),
      limits = c(0, NA),
      breaks = pretty_breaks(n = 8)
    )

  output_file <- file.path(output_path, paste(workload, storage_engine, num_workers, thinking_time, sync_mode, "memory_time.png", sep = "."))

  ggsave(output_file, plot = p, width = 12, height = 8, dpi = 300, bg = "white")

  cat(paste("Generated chart:", output_file, "\n"))
}

cat(paste("\nAll charts saved to:", output_path, "\n"))
