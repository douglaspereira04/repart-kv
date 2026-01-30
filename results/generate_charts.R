#!/usr/bin/env Rscript
#
# Generate charts from aggregated CSV metrics files.
#
# Usage:
#   Rscript generate_charts.R [input_path] [output_path]
#
# Arguments:
#   input_path:  Directory containing aggregated CSV files (default: ./aggregated_results)
#   output_path: Directory to save charts (default: ./charts)

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

# Parse command line arguments
args <- commandArgs(trailingOnly = TRUE)
input_path <- if (length(args) >= 1) args[1] else "./aggregated_results"
output_path <- if (length(args) >= 2) args[2] else "./charts"

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
  
  workload <- unique(data$workload)
  storage_engine <- unique(data$storage_engine)
  
  cat(paste("Generating chart for", workload, "-", storage_engine, "...\n"))
  
  # Create storage type labels with partition count, paths, and interval
  data$storage_type_label <- paste0(
    data$storage_type, 
    " (p=", data$partitions, 
    ", d=", data$paths, 
    ", i=", data$interval / 1000, "s)"
  )
  
  # Create a factor for workers to ensure equal spacing
  data$workers_factor <- factor(data$workers, levels = sort(unique(data$workers)))
  
  # Create the plot
  p <- ggplot(data, aes(x = workers_factor, y = ops_per_second / 1000, color = storage_type_label, group = storage_type_label)) +
    geom_line(linewidth = 1.5, alpha = 0.8) +
    geom_point(size = 4, alpha = 0.8) +
    labs(
      x = "Number of Workers",
      y = "Thousand Operations per Second",
      title = paste(workload, "-", storage_engine),
      color = "Storage Type"
    ) +
    theme_minimal() +
    theme(
      plot.title = element_text(size = 20, face = "bold", hjust = 0.5),
      axis.title = element_text(size = 16),
      axis.text = element_text(size = 14),
      legend.title = element_text(size = 14, face = "bold"),
      legend.text = element_text(size = 12),
      legend.position = "right",
      panel.grid.major = element_line(color = "gray90", linewidth = 0.5),
      panel.grid.minor = element_line(color = "gray95", linewidth = 0.25)
    ) +
    scale_color_brewer(palette = "Set1") +
    scale_y_continuous(
      expand = expansion(mult = c(0, 0.15)), 
      limits = c(0, NA),
      breaks = pretty_breaks(n = 10)
    )
  
  # Save chart
  safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
  safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")
  output_file <- file.path(output_path, paste(safe_workload, safe_engine, "png", sep = "."))
  
  ggsave(output_file, plot = p, width = 12, height = 6, dpi = 300, bg = "white")
  
  cat(paste("Generated chart:", output_file, "\n"))
}

cat(paste("\nAll charts saved to:", output_path, "\n"))
