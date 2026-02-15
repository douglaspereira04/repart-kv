#!/usr/bin/env Rscript
#
# Generate throughput-over-time charts with range bands from aggregated CSV files.
#
# Usage:
#   Rscript generate_throughput_time_charts.R [input_path] [output_path]
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
output_path <- if (length(args) >= 2) args[2] else "/home/douglas/Documentos/repart-kv/results/charts/throughput_time"

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
  
  # Extract info from filename (e.g., ycsb_a__lmdb__1.csv)
  filename <- basename(csv_file)
  parts <- str_split(str_remove(filename, "\\.csv$"), "__")[[1]]
  workload <- parts[1]
  storage_engine <- parts[2]
  num_workers <- parts[3]
  
  cat(paste("Generating chart for", workload, "-", storage_engine, "(Workers:", num_workers, ") ...\n"))
  
  # Create nice labels for the legend
  data$label <- paste0(
    data$storage_type, 
    " (p=", data$partitions, 
    ", d=", data$paths, 
    ", i=", data$interval / 1000, "s)"
  )
  
  # Identify start and end points for repartitioning
  # We consider an event "active" if its probability is > 0
  # Since we have multiple lines (labels) in the same dataframe, we group by label
  data <- data %>%
    arrange(label, elapsed_s) %>%
    group_by(label) %>%
    mutate(
      repartitioning_active = repartitioning_prob > 0,
      # Repartitioning transitions
      repartitioning_start = repartitioning_active & !lag(repartitioning_active, default = FALSE),
      repartitioning_end = !repartitioning_active & lag(repartitioning_active, default = FALSE)
    ) %>%
    ungroup()

  # Create dataframes for the lines
  repartitioning_starts <- data %>% filter(repartitioning_start)
  repartitioning_ends <- data %>% filter(repartitioning_end)

  # Create the plot
  p <- ggplot(data, aes(x = elapsed_s, y = mean / 1000, color = label, fill = label)) +
    # Min-Max band
    geom_ribbon(aes(ymin = min / 1000, ymax = max / 1000), alpha = 0.2, color = NA) +
    # Mean line
    geom_line(linewidth = 1, alpha = 0.6) +
    # Repartitioning Start (Dot, same color as line, lighter)
    geom_point(data = repartitioning_starts, 
               aes(x = elapsed_s, y = mean / 1000, color = label),
               alpha = 0.4, size = 3, shape = 16, inherit.aes = FALSE) +
    # Repartitioning End (Vertical line, same color as line, solid)
    geom_point(data = repartitioning_ends, 
               aes(x = elapsed_s, y = mean / 1000, color = label),
               alpha = 1.0, size = 3, shape = 16, inherit.aes = FALSE) +
    labs(
      x = "Elapsed Time (s)",
      y = "Thousand Operations per Second",
      title = paste("Throughput over Time:", workload, "-", storage_engine),
      subtitle = paste("Number of Test Workers:", num_workers),
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
    scale_y_continuous(
      expand = expansion(mult = c(0, 0.1)), 
      limits = c(0, NA),
      breaks = pretty_breaks(n = 8)
    )
  
  # Save chart
  output_file <- file.path(output_path, paste(workload, storage_engine, num_workers, "throughput_time.png", sep = "."))
  
  ggsave(output_file, plot = p, width = 12, height = 8, dpi = 300, bg = "white")
  
  cat(paste("Generated chart:", output_file, "\n"))
}

cat(paste("\nAll charts saved to:", output_path, "\n"))
