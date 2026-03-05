#!/usr/bin/env python3
import os
import random
import re
import csv
import sys
from collections import defaultdict

import numpy as np

# Sample 1/50 of values for faster aggregation (mod 50 == CHOSEN yields ~2% sample)
SAMPLE_MOD = 1000
SAMPLE_REMAINDER = 0  # include value when rand % 50 == 0


def parse_latency_filename(filename):
    # Format: workload__testworkers__storagetype__partitions__storageengine__paths__interval__thinking_latency(REP).csv
    # Example: ycsb_a__1__engine__1__lmdb__1__0__0_latency(1).csv
    # thinking is in nanoseconds (ns)
    # Backward compat: 7 parts (no thinking) -> thinking_time=0

    if not filename.endswith('.csv') or '__latency' not in filename or '(' not in filename:
        return None

    name = filename.rsplit('.', 1)[0]

    # Extract REP
    match = re.search(r'\((\d+)\)$', name)
    if not match:
        return None

    rep = int(match.group(1))
    name_no_rep = name[: match.start()]

    # Remove _latency suffix
    if not name_no_rep.endswith('__latency'):
        return None
    name_no_latency = name_no_rep[: -len('__latency')]

    parts = name_no_latency.split('__')
    if len(parts) not in (7, 8):
        return None

    thinking_time = int(parts[7]) if len(parts) == 8 else 0

    return {
        'workload': parts[0],
        'workers': int(parts[1]),
        'storage_type': parts[2],
        'partitions': int(parts[3]),
        'storage_engine': parts[4],
        'paths': int(parts[5]),
        'interval': int(parts[6]),
        'thinking_time': thinking_time,
        'rep': rep,
        'key': name_no_latency,
    }


def calculate_latency_metrics(filepath):
    """Compute latency_median and latency_95 (95th percentile) from a latency CSV.
    Uses 1/SAMPLE_MOD sampling for speed (~0.1% of values when SAMPLE_MOD=1000).
    """
    try:
        sampled = []
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            fields = reader.fieldnames or []
            # Support both ns (new) and ms (legacy) column names
            start_col = 'start_ns' if 'start_ns' in fields else 'start_ms'
            end_col = 'end_ns' if 'end_ns' in fields else 'end_ms'
            if start_col not in fields or end_col not in fields:
                return None
            for row in reader:
                if random.randrange(SAMPLE_MOD) != SAMPLE_REMAINDER:
                    continue
                try:
                    start = float(row[start_col].replace(',', '.'))
                    end = float(row[end_col].replace(',', '.'))
                    sampled.append(end - start)
                except (ValueError, KeyError):
                    continue

        if not sampled:
            return None
        arr = np.array(sampled)
        latency_median = float(np.median(arr))
        latency_95 = float(np.percentile(arr, 95))
        print(f"Latency metrics{filepath}: {latency_median}, {latency_95}")
        return {
            'latency_median': latency_median,
            'latency_95': latency_95,
        }
    except Exception as e:
        print(f"Error processing {filepath}: {e}", file=sys.stderr)
        return None


def load_throughput_data(throughput_dir):
    """Load throughput (ops_per_second) from CSVs produced by aggregate_results.py.
    Returns dict: (workload, workers, storage_type, partitions, storage_engine, paths, interval, thinking_time) -> ops_per_second
    """
    throughput = {}
    if not os.path.isdir(throughput_dir):
        return throughput
    for f in os.listdir(throughput_dir):
        if not f.endswith('.csv'):
            continue
        filepath = os.path.join(throughput_dir, f)
        try:
            with open(filepath, 'r') as fp:
                reader = csv.DictReader(fp)
                for row in reader:
                    key = (
                        row['workload'],
                        int(row['workers']),
                        row['storage_type'],
                        int(row['partitions']),
                        row['storage_engine'],
                        int(row['paths']),
                        int(row['interval']),
                        int(row['thinking_time']),
                    )
                    throughput[key] = float(row['ops_per_second'].replace(',', '.'))
        except (KeyError, ValueError, OSError) as e:
            print(f"Warning: Skipping {filepath}: {e}", file=sys.stderr)
    return throughput


def main():
    input_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output_base_dir = sys.argv[2] if len(sys.argv) > 2 else "aggregated_latency"
    throughput_dir = sys.argv[3] if len(sys.argv) > 3 else "aggregated_results/throughput"
    throughput_dir = os.path.abspath(throughput_dir)

    os.makedirs(output_base_dir, exist_ok=True)
    throughput_lookup = load_throughput_data(throughput_dir)

    # key -> list of metrics per rep
    results = defaultdict(list)
    metadata = {}

    files = [f for f in os.listdir(input_dir) if f.endswith('.csv') and '_latency' in f and '(' in f]

    for f in files:
        params = parse_latency_filename(f)
        if not params:
            continue

        metrics = calculate_latency_metrics(os.path.join(input_dir, f))
        if metrics is not None:
            key = params['key']
            results[key].append(metrics)
            if key not in metadata:
                metadata[key] = params

    # Group by workload and engine for output files (same structure as aggregate_results)
    grouped = defaultdict(list)

    for key, metrics_list in results.items():
        meta = metadata[key]

        mean_median = sum(m['latency_median'] for m in metrics_list) / len(metrics_list)
        mean_p95 = sum(m['latency_95'] for m in metrics_list) / len(metrics_list)
        throughput_key = (
            meta['workload'], meta['workers'], meta['storage_type'], meta['partitions'],
            meta['storage_engine'], meta['paths'], meta['interval'], meta['thinking_time'],
        )
        throughput_ops_per_sec = throughput_lookup.get(throughput_key)

        output_key = f"{meta['workload']}__{meta['storage_engine']}"

        common_meta = {
            'workload': meta['workload'],
            'workers': meta['workers'],
            'storage_type': meta['storage_type'],
            'partitions': meta['partitions'],
            'storage_engine': meta['storage_engine'],
            'paths': meta['paths'],
            'interval': meta['interval'],
            'thinking_time': meta['thinking_time'],
        }

        grouped[output_key].append({
            **common_meta,
            'latency_median': mean_median,
            'latency_95': mean_p95,
            'throughput_ops_per_sec': throughput_ops_per_sec if throughput_ops_per_sec is not None else '',
        })

    # Write latency CSVs
    for output_key, data_list in grouped.items():
        output_file = os.path.join(output_base_dir, f"{output_key}.csv")
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(
                f,
                fieldnames=[
                    'workload',
                    'workers',
                    'storage_type',
                    'partitions',
                    'storage_engine',
                    'paths',
                    'interval',
                    'thinking_time',
                    'latency_median',
                    'latency_95',
                    'throughput_ops_per_sec',
                ],
            )
            writer.writeheader()
            sorted_data = sorted(
                data_list,
                key=lambda x: (x['workers'], x['storage_type'], x['partitions'], x['thinking_time']),
            )
            writer.writerows(sorted_data)
        print(f"Created latency CSV: {output_file}")


if __name__ == "__main__":
    main()
