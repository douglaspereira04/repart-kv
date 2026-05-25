# Shared helpers for plotting storage types with matched partition settings.
#
# Repartitioning types (hard, lock_stripping) can use different partition counts
# across experiments. Each chart shows one partition setting for those types, with
# engine always at ENGINE_PARTITIONS (typically 1).

REPARTITIONING_STORAGE_TYPES <- c("hard", "lock_stripping")
ENGINE_STORAGE_TYPE <- "engine"
ENGINE_PARTITIONS <- 1L

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

partition_chart_subtitle <- function(partition_setting, sync_mode = NULL, extra = NULL) {
  parts <- character(0)
  if (!is.null(partition_setting)) {
    parts <- c(
      parts,
      paste0("Partitions: engine=", ENGINE_PARTITIONS, ", hard/lock_stripping=", partition_setting)
    )
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
  is_engine <- data$storage_type == ENGINE_STORAGE_TYPE
  paste0(
    data$storage_type,
    ifelse(
      is_engine,
      paste0(" (p=", data$partitions, ", d=", data$paths, ", i=", data$interval / 1000, "s)"),
      paste0(" (d=", data$paths, ", i=", data$interval / 1000, "s)")
    )
  )
}

make_time_series_label <- function(data) {
  is_engine <- data$storage_type == ENGINE_STORAGE_TYPE
  paste0(
    data$storage_type,
    ifelse(
      is_engine,
      paste0(
        " (p=", data$partitions,
        ", d=", data$paths,
        ", i=", data$interval / 1000, "s",
        ", t=", data$thinking_time, "ns)"
      ),
      paste0(
        " (d=", data$paths,
        ", i=", data$interval / 1000, "s",
        ", t=", data$thinking_time, "ns)"
      )
    )
  )
}
