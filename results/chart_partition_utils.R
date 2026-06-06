# Shared helpers for plotting storage types with matched partition settings.
#
# Repartitioning types (hard, lock_stripping) can use different partition counts
# across experiments. Each chart shows one partition setting for those types, with
# engine always at ENGINE_PARTITIONS (typically 1).

REPARTITIONING_STORAGE_TYPES <- c("hard", "lock_stripping")
ENGINE_STORAGE_TYPE <- "engine"
ENGINE_PARTITIONS <- 1L
ROCKSDB_STORAGE_ENGINE <- "rocksdb"
ROCKSDB_BASELINE_LABEL <- "rocksdb"

.rocksdb_engine_cache <- new.env(parent = emptyenv())

clear_rocksdb_engine_cache <- function() {
  rm(list = ls(envir = .rocksdb_engine_cache), envir = .rocksdb_engine_cache)
}

is_rocksdb_baseline_row <- function(data) {
  data$storage_type == ENGINE_STORAGE_TYPE &
    data$storage_engine == ROCKSDB_STORAGE_ENGINE
}

get_rocksdb_engine_rows <- function(input_dir, workload, sync_mode) {
  key <- paste(workload, sync_mode, sep = "|")
  if (!exists(key, envir = .rocksdb_engine_cache, inherits = FALSE)) {
    path <- file.path(input_dir, paste0(workload, "__", ROCKSDB_STORAGE_ENGINE, "__", sync_mode, ".csv"))
    if (!file.exists(path)) {
      assign(key, NULL, envir = .rocksdb_engine_cache)
    } else {
      df <- read.csv(path)
      df <- df[
        df$storage_type == ENGINE_STORAGE_TYPE & df$partitions == ENGINE_PARTITIONS,
        ,
        drop = FALSE
      ]
      if ("throughput_ops_per_sec" %in% names(df) && nrow(df) > 0) {
        df <- df[
          !is.na(df$throughput_ops_per_sec) &
            df$throughput_ops_per_sec != "" &
            df$throughput_ops_per_sec > 0,
          ,
          drop = FALSE
        ]
        if (nrow(df) > 0) {
          df$throughput_ops_per_sec <- as.numeric(df$throughput_ops_per_sec)
        }
      }
      if (nrow(df) == 0) {
        df <- NULL
      }
      assign(key, df, envir = .rocksdb_engine_cache)
    }
  }
  get(key, envir = .rocksdb_engine_cache, inherits = FALSE)
}

# Add RocksDB engine baseline to a thinking-time subset when plotting another engine.
append_rocksdb_baseline_rows <- function(subset_data, rocksdb_engine_rows, storage_engine, think_time) {
  if (identical(storage_engine, ROCKSDB_STORAGE_ENGINE)) {
    return(subset_data)
  }
  if (is.null(rocksdb_engine_rows) || nrow(rocksdb_engine_rows) == 0) {
    return(subset_data)
  }
  overlay <- rocksdb_engine_rows[rocksdb_engine_rows$thinking_time == think_time, , drop = FALSE]
  if (nrow(overlay) == 0) {
    return(subset_data)
  }
  rbind(subset_data, overlay)
}

get_partition_chart_settings <- function(data) {
  repart <- data[data$storage_type %in% REPARTITIONING_STORAGE_TYPES, , drop = FALSE]
  if (nrow(repart) == 0) {
    return(integer(0))
  }
  sort(unique(repart$partitions))
}

filter_data_for_partition_chart <- function(data, partition_setting) {
  data[
    (data$storage_type == ENGINE_STORAGE_TYPE & data$partitions == ENGINE_PARTITIONS) |
      (data$storage_type %in% REPARTITIONING_STORAGE_TYPES & data$partitions == partition_setting),
    ,
    drop = FALSE
  ]
}

# Returns a list of list(setting = <int or NULL>, data = <dataframe>)
partition_chart_jobs <- function(data) {
  settings <- get_partition_chart_settings(data)
  if (length(settings) == 0) {
    return(list(list(setting = NULL, data = data)))
  }
  lapply(settings, function(ps) {
    list(setting = ps, data = filter_data_for_partition_chart(data, ps))
  })
}

partition_chart_filename_parts <- function(partition_setting) {
  if (is.null(partition_setting)) {
    character(0)
  } else {
    paste0("p", partition_setting)
  }
}

storage_type_display_name <- function(storage_type, storage_engine, interval = NULL) {
  if (missing(storage_engine) || length(storage_engine) == 0) {
    return(storage_type)
  }
  if (length(storage_engine) == 1L && length(storage_type) > 1L) {
    storage_engine <- rep(storage_engine, length(storage_type))
  }
  if (!is.null(interval) && length(interval) == 1L && length(storage_type) > 1L) {
    interval <- rep(interval, length(storage_type))
  }
  display <- as.character(storage_type)
  is_engine <- storage_type == ENGINE_STORAGE_TYPE
  is_hard <- storage_type == "hard"
  is_lock_stripping <- storage_type == "lock_stripping"
  if (any(is_engine)) display[is_engine] <- storage_engine[is_engine]
  if (any(is_hard)) {
    if (!is.null(interval) && length(interval) == length(storage_type)) {
      hard_indexed <- is_hard & interval == 0
      hard_repartitioning <- is_hard & interval != 0
      if (any(hard_indexed)) {
        display[hard_indexed] <- paste("index", storage_engine[hard_indexed])
      }
      if (any(hard_repartitioning)) {
        display[hard_repartitioning] <- paste(
          "repartitioning", storage_engine[hard_repartitioning]
        )
      }
    } else {
      display[is_hard] <- paste("index", storage_engine[is_hard])
    }
  }
  if (any(is_lock_stripping)) {
    display[is_lock_stripping] <- paste("ls", storage_engine[is_lock_stripping])
  }
  display
}

storage_type_config_suffix <- function(storage_type, paths, interval,
                                       is_engine = NULL, thinking_time = NULL) {
  if (is.null(is_engine)) {
    is_engine <- storage_type == ENGINE_STORAGE_TYPE
  }
  is_hard <- storage_type == "hard"
  is_lock_stripping <- storage_type == "lock_stripping"
  tt_suffix <- if (is.null(thinking_time)) "" else paste0(", t=", thinking_time, "ns")

  ifelse(
    is_engine,
    "",
    ifelse(
      is_hard | is_lock_stripping,
      paste0(" (d=", paths, tt_suffix, ")"),
      paste0(" (d=", paths, ", i=", interval / 1000, "s", tt_suffix, ")")
    )
  )
}

partition_chart_subtitle <- function(partition_setting, sync_mode = NULL, extra = NULL,
                                     storage_engine = NULL) {
  parts <- character(0)
  if (!is.null(partition_setting)) {
    if (!is.null(storage_engine) && nzchar(storage_engine)) {
      parts <- c(
        parts,
        paste0(
          "Partitions: ", storage_engine, "=", ENGINE_PARTITIONS,
          ", index/LS=", partition_setting
        )
      )
    } else {
      parts <- c(
        parts,
        paste0("Partitions: engine=", ENGINE_PARTITIONS, ", hard/lock_stripping=", partition_setting)
      )
    }
  }
  if (!is.null(sync_mode) && nzchar(sync_mode)) {
    parts <- c(parts, paste("Sync:", sync_mode))
  }
  if (!is.null(extra) && nzchar(extra)) {
    parts <- c(parts, extra)
  }
  paste(parts, collapse = " | ")
}

make_storage_type_label <- function(data) {
  baseline <- is_rocksdb_baseline_row(data)
  is_engine <- data$storage_type == ENGINE_STORAGE_TYPE
  display_name <- storage_type_display_name(
    data$storage_type, data$storage_engine, data$interval
  )
  ifelse(
    baseline,
    ROCKSDB_BASELINE_LABEL,
    paste0(
      display_name,
      storage_type_config_suffix(
        data$storage_type, data$paths, data$interval, is_engine
      )
    )
  )
}

make_time_series_label <- function(data, storage_engine = NULL) {
  is_engine <- data$storage_type == ENGINE_STORAGE_TYPE
  engine_vec <- if (!is.null(storage_engine)) {
    rep(storage_engine, nrow(data))
  } else if ("storage_engine" %in% names(data)) {
    data$storage_engine
  } else {
    character(0)
  }
  display_name <- if (length(engine_vec) > 0) {
    storage_type_display_name(data$storage_type, engine_vec, data$interval)
  } else {
    data$storage_type
  }
  paste0(
    display_name,
    storage_type_config_suffix(
      data$storage_type,
      data$paths,
      data$interval,
      is_engine,
      data$thinking_time
    )
  )
}

parse_aggregated_throughput_time_filename <- function(csv_file) {
  filename <- basename(csv_file)
  parts <- strsplit(strsplit(filename, "\\.csv$")[[1]], "__")[[1]]
  thinking_time <- if (length(parts) >= 4) as.character(parts[4]) else "0"
  sync_mode <- if (length(parts) >= 5) parts[5] else "sync_off"
  list(
    workload = parts[1],
    storage_engine = parts[2],
    workers = as.integer(parts[3]),
    thinking_time = thinking_time,
    sync_mode = sync_mode,
    group_key = paste(parts[1], parts[2], thinking_time, sync_mode, sep = "|")
  )
}

# Summarize a KiB time-series metric into mean/sd per workers and storage configuration.
# Returns a list of list(setting, workload, storage_engine, thinking_time, sync_mode, data).
summarize_resource_by_workers_jobs <- function(input_path, metric_kb_col) {
  if (!requireNamespace("dplyr", quietly = TRUE)) {
    stop("Package 'dplyr' is required")
  }
  csv_files <- list.files(path = input_path, pattern = "\\.csv$", full.names = TRUE)
  if (length(csv_files) == 0) {
    return(list())
  }

  file_groups <- split(csv_files, vapply(
    csv_files,
    function(path) parse_aggregated_throughput_time_filename(path)$group_key,
  character(1)))

  jobs <- list()
  for (group_key in names(file_groups)) {
    group_files <- file_groups[[group_key]]
    meta0 <- parse_aggregated_throughput_time_filename(group_files[[1]])
    combined_parts <- list()

    for (csv_file in group_files) {
      meta <- parse_aggregated_throughput_time_filename(csv_file)
      data <- utils::read.csv(csv_file)
      if (nrow(data) == 0 || !(metric_kb_col %in% names(data))) next
      data$workers <- meta$workers
      data$storage_engine <- meta$storage_engine
      if (!"thinking_time" %in% names(data)) {
        data$thinking_time <- meta$thinking_time
      }
      if (!"sync_mode" %in% names(data)) {
        data$sync_mode <- meta$sync_mode
      }
      combined_parts[[length(combined_parts) + 1]] <- data
    }
    if (length(combined_parts) == 0) next

    combined <- dplyr::bind_rows(combined_parts)
    for (job in partition_chart_jobs(combined)) {
      chart_data <- job$data
      if (nrow(chart_data) == 0) next

      summary_data <- chart_data %>%
        dplyr::group_by(
          workers, storage_type, partitions, paths, interval,
          thinking_time, sync_mode, storage_engine
        ) %>%
        dplyr::summarize(
          usage_mean_mib = mean(.data[[metric_kb_col]], na.rm = TRUE) / 1024,
          usage_sd_mib = sd(.data[[metric_kb_col]], na.rm = TRUE) / 1024,
          .groups = "drop"
        ) %>%
        dplyr::mutate(usage_sd_mib = ifelse(is.na(usage_sd_mib), 0, usage_sd_mib))

      if (nrow(summary_data) == 0) next

      jobs[[length(jobs) + 1]] <- list(
        setting = job$setting,
        workload = meta0$workload,
        storage_engine = meta0$storage_engine,
        thinking_time = meta0$thinking_time,
        sync_mode = meta0$sync_mode,
        data = summary_data
      )
    }
  }
  jobs
}

CHART_DPI <- 300L
CHART_FONT_SIZE <- 18
CHART_DOT_LABEL_FONT_SIZE <- 10

chart_dot_label_size <- function(font_size = CHART_DOT_LABEL_FONT_SIZE) {
  font_size / 2.835052
}

chart_geom_text_size <- chart_dot_label_size

chart_theme <- function(font_size = CHART_FONT_SIZE, legend_position = "right",
                        legend_direction = NULL) {
  p <- ggplot2::theme_minimal(base_size = font_size) +
    ggplot2::theme(
      plot.title = ggplot2::element_text(size = font_size, face = "bold", hjust = 0.5),
      plot.subtitle = ggplot2::element_text(size = font_size, hjust = 0.5),
      axis.title = ggplot2::element_text(size = font_size),
      axis.text = ggplot2::element_text(size = font_size),
      legend.title = ggplot2::element_text(size = font_size, face = "bold"),
      legend.text = ggplot2::element_text(size = font_size),
      legend.position = legend_position,
      panel.grid.major = ggplot2::element_line(color = "gray90", linewidth = 0.5),
      panel.grid.minor = ggplot2::element_line(color = "gray95", linewidth = 0.25)
    )
  if (!is.null(legend_direction)) {
    p <- p + ggplot2::theme(legend.direction = legend_direction)
  }
  p
}

parse_boolean_cli <- function(value, default = TRUE) {
  if (missing(value) || length(value) == 0 || !nzchar(as.character(value)[1])) {
    return(default)
  }
  normalized <- tolower(trimws(as.character(value)[1]))
  if (normalized %in% c("1", "true", "yes", "on")) return(TRUE)
  if (normalized %in% c("0", "false", "no", "off")) return(FALSE)
  default
}

# Parse common chart CLI arguments shared by all generate_*_charts.R scripts.
# Args: [input_path] [output_path] [max_y] [max_x] [width_px] [height_px] [show_titles]
# max_y / max_x: 0 = automatic axis range (ggplot default behavior).
# width_px / height_px: output size in pixels; 0 = script default dimensions.
# show_titles: 1/true = show title and subtitle; 0/false = hide (default: 1).
parse_chart_cli_args <- function(args, default_input, default_output,
                                 default_width_px = 3600L, default_height_px = 1800L,
                                 default_show_titles = TRUE) {
  input_path <- if (length(args) >= 1) args[1] else default_input
  output_path <- if (length(args) >= 2) args[2] else default_output
  max_y <- if (length(args) >= 3) as.numeric(args[3]) else 0
  max_x <- if (length(args) >= 4) as.numeric(args[4]) else 0
  chart_width_px <- if (length(args) >= 5) as.numeric(args[5]) else 0
  chart_height_px <- if (length(args) >= 6) as.numeric(args[6]) else 0
  show_titles <- if (length(args) >= 7) parse_boolean_cli(args[7], default_show_titles) else default_show_titles

  if (is.na(max_y)) max_y <- 0
  if (is.na(max_x)) max_x <- 0
  if (is.na(chart_width_px) || chart_width_px <= 0) chart_width_px <- default_width_px
  if (is.na(chart_height_px) || chart_height_px <= 0) chart_height_px <- default_height_px

  list(
    input_path = input_path,
    output_path = output_path,
    max_y = max_y,
    max_x = max_x,
    chart_width_px = chart_width_px,
    chart_height_px = chart_height_px,
    show_titles = show_titles,
    chart_dpi = CHART_DPI
  )
}

ggsave_chart <- function(filename, plot, chart_opts) {
  ggsave(
    filename,
    plot = plot,
    width = chart_opts$chart_width_px / chart_opts$chart_dpi,
    height = chart_opts$chart_height_px / chart_opts$chart_dpi,
    dpi = chart_opts$chart_dpi,
    bg = "white"
  )
}

# Scale limits only for automatic ranging. Fixed caps use coord_cartesian instead
# so geom_path/geom_line segments to off-screen points clip at the panel edge
# rather than disappearing when out-of-bounds data is dropped.
scale_y_limits <- function(max_y) {
  if (is.null(max_y) || max_y <= 0) c(0, NA) else NULL
}

scale_x_limits <- function(max_x) {
  NULL
}

coord_y_limit <- function(max_y) {
  if (is.null(max_y) || max_y <= 0) NULL else c(0, max_y)
}

coord_x_limit <- function(max_x, lower = 0) {
  if (is.null(max_x) || max_x <= 0) {
    NULL
  } else if (is.na(lower)) {
    c(NA, max_x)
  } else {
    c(lower, max_x)
  }
}

add_coord_axis_limits <- function(plot, chart_opts) {
  ylim <- coord_y_limit(chart_opts$max_y)
  xlim <- coord_x_limit(chart_opts$max_x)
  if (is.null(ylim) && is.null(xlim)) {
    return(plot)
  }
  args <- list(clip = "on")
  if (!is.null(ylim)) args$ylim <- ylim
  if (!is.null(xlim)) args$xlim <- xlim
  plot + do.call(coord_cartesian, args)
}

apply_chart_titles <- function(plot, chart_opts) {
  if (isTRUE(chart_opts$show_titles)) {
    return(plot)
  }
  plot + theme(plot.title = element_blank(), plot.subtitle = element_blank())
}

y_axis_limits <- scale_y_limits
x_axis_limits <- scale_x_limits
