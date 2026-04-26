#!/usr/bin/env Rscript
#
# Generate makespan charts from aggregated CSV metrics files.
#
# Usage:
#   Rscript generate_makespan_charts.R [input_path] [output_path]
#
# Arguments:
#   input_path:  Directory containing aggregated makespan CSV files (default: ./aggregated_results/makespan)
#   output_path: Directory to save charts (default: ./charts/makespan)

suppressPackageStartupMessages({
  library(ggplot2)
  library(dplyr)
  library(stringr)
  library(scales)
})

# Parse command line arguments
args <- commandArgs(trailingOnly = TRUE)
input_path <- if (length(args) >= 1) args[1] else "./aggregated_results/makespan"
output_path <- if (length(args) >= 2) args[2] else "./charts/makespan"

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

  for (tt in thinking_times) {
    subset_data <- data %>% filter(thinking_time == tt)
    if (nrow(subset_data) == 0) next

    cat(paste("Generating makespan chart for", workload, "-", storage_engine, "(Thinking time:", tt, "ns, Sync:", sync_mode, ") ...\n"))

    # Create storage type labels (p=partitions, d=paths, i=interval) - exclude thinking_time since it's fixed per chart
    subset_data$storage_type_label <- paste0(
      subset_data$storage_type,
      " (p=", subset_data$partitions,
      ", d=", subset_data$paths,
      ", i=", subset_data$interval / 1000, "s)"
    )

    # Create a factor for workers to ensure equal spacing
    subset_data$workers_factor <- factor(subset_data$workers, levels = sort(unique(subset_data$workers)))

    # Create the plot
    p <- ggplot(subset_data, aes(x = workers_factor, y = makespan_s, fill = storage_type_label)) +
      geom_bar(stat = "identity", position = position_dodge(width = 0.8), width = 0.7, alpha = 0.8) +
      labs(
        x = "Number of Workers",
        y = "Makespan (seconds)",
        title = paste("Execution Time:", workload, "-", storage_engine),
        subtitle = paste("Thinking time:", tt, "ns | Sync:", sync_mode),
        fill = "Storage Type"
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
      scale_fill_brewer(palette = "Set1") +
      scale_y_continuous(
        expand = expansion(mult = c(0, 0.15)),
        limits = c(0, NA),
        breaks = pretty_breaks(n = 10)
      )

    # Save chart: one file per workload, storage_engine, thinking_time
    safe_workload <- str_replace_all(workload, "[^\\w\\-_]", "_")
    safe_engine <- str_replace_all(storage_engine, "[^\\w\\-_]", "_")
    output_file <- file.path(output_path, paste(safe_workload, safe_engine, tt, sync_mode, "makespan.png", sep = "."))

    ggsave(output_file, plot = p, width = 12, height = 6, dpi = 300, bg = "white")

    cat(paste("Generated chart:", output_file, "\n"))
  }
}

cat(paste("\nAll makespan charts saved to:", output_path, "\n"))
