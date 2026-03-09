#!/usr/bin/env python3
"""
Aggregate latency CSVs that contain pre-computed latency_median_ns and latency_p95_ns.
Each input file has format:
  latency_median_ns,latency_p95_ns
  <value>,<value>

Files with same base name but different (1), (2), etc. are averaged.
"""
import os
import re
import csv
import sys
from collections import defaultdict


def parse_latency_filename(filename):
    # Format: workload__testworkers__storagetype__partitions__storageengine__paths__interval__thinking_latency(REP).csv
    # Example: ycsb_a__1__engine__1__lmdb__1__0__0_latency(1).csv
    if not filename.endswith('.csv') or '__latency' not in filename or '(' not in filename:
        return None

    name = filename.rsplit('.', 1)[0]
    match = re.search(r'\((\d+)\)$', name)
    if not match:
        return None

    name_no_rep = name[: match.start()]
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
        'key': name_no_latency,
    }


def load_latency_metrics(filepath):
    """Load latency_median_ns and latency_p95_ns from new-format CSV.
    Returns dict with latency_median and latency_95 (in ns), or None on error.
    """
    try:
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            row = next(reader, None)
            if row is None:
                return None
            median_ns = float(row.get('latency_median_ns', row.get('latency_median', '0')).replace(',', '.'))
            p95_ns = float(row.get('latency_p95_ns', row.get('latency_p95', '0')).replace(',', '.'))
            return {'latency_median': median_ns, 'latency_95': p95_ns}
    except (KeyError, ValueError, StopIteration, OSError) as e:
        print(f"Warning: Skipping {filepath}: {e}", file=sys.stderr)
        return None


def load_throughput_data(throughput_dir):
    """Load throughput (ops_per_second) from CSVs produced by aggregate_results.py."""
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
    output_base_dir = sys.argv[2] if len(sys.argv) > 2 else "aggregated_latency_latency_average"
    throughput_dir = sys.argv[3] if len(sys.argv) > 3 else "aggregated_results/throughput"
    throughput_dir = os.path.abspath(throughput_dir)

    os.makedirs(output_base_dir, exist_ok=True)
    throughput_lookup = load_throughput_data(throughput_dir)

    # key -> list of {latency_median, latency_95} per rep
    results = defaultdict(list)
    metadata = {}

    files = [f for f in os.listdir(input_dir) if f.endswith('.csv') and '_latency' in f and '(' in f]

    for f in files:
        params = parse_latency_filename(f)
        if not params:
            continue

        metrics = load_latency_metrics(os.path.join(input_dir, f))
        if metrics is not None:
            key = params['key']
            results[key].append(metrics)
            if key not in metadata:
                metadata[key] = params

    # Group by workload and engine for output files
    grouped = defaultdict(list)

    for key, metrics_list in results.items():
        meta = metadata[key]
        n = len(metrics_list)
        latency_median = sum(m['latency_median'] for m in metrics_list) / n
        latency_95 = sum(m['latency_95'] for m in metrics_list) / n

        throughput_key = (
            meta['workload'], meta['workers'], meta['storage_type'], meta['partitions'],
            meta['storage_engine'], meta['paths'], meta['interval'], meta['thinking_time'],
        )
        throughput_ops_per_sec = throughput_lookup.get(throughput_key)

        output_key = f"{meta['workload']}__{meta['storage_engine']}"
        grouped[output_key].append({
            'workload': meta['workload'],
            'workers': meta['workers'],
            'storage_type': meta['storage_type'],
            'partitions': meta['partitions'],
            'storage_engine': meta['storage_engine'],
            'paths': meta['paths'],
            'interval': meta['interval'],
            'thinking_time': meta['thinking_time'],
            'latency_median': latency_median,
            'latency_95': latency_95,
            'throughput_ops_per_sec': throughput_ops_per_sec if throughput_ops_per_sec is not None else '',
        })

    for output_key, data_list in grouped.items():
        output_file = os.path.join(output_base_dir, f"{output_key}.csv")
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(
                f,
                fieldnames=[
                    'workload', 'workers', 'storage_type', 'partitions',
                    'storage_engine', 'paths', 'interval', 'thinking_time',
                    'latency_median', 'latency_95', 'throughput_ops_per_sec',
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
