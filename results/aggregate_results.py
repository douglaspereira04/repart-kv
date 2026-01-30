#!/usr/bin/env python3
import os
import re
import csv
import sys
from collections import defaultdict

def parse_filename(filename):
    # Format: workload__testworkers__storagetype__partitions__storageengine__paths__interval(REP).csv
    # Example: ycsb_a__1__engine__1__tkrzw_tree__1__0(1).csv
    
    # Remove extension
    name = filename.rsplit('.', 1)[0]
    
    # Extract REP
    match = re.search(r'\((\d+)\)$', name)
    if not match:
        return None
    
    rep = int(match.group(1))
    # Remove (REP) from name
    name_no_rep = name[:match.start()]
    
    parts = name_no_rep.split('__')
    if len(parts) != 7:
        return None
        
    return {
        'workload': parts[0],
        'workers': int(parts[1]),
        'storage_type': parts[2],
        'partitions': int(parts[3]),
        'storage_engine': parts[4],
        'paths': int(parts[5]),
        'interval': int(parts[6]),
        'rep': rep,
        'key': name_no_rep # Unique key for the experiment configuration
    }

def calculate_metrics(filepath):
    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()
            if len(lines) < 2:
                return None
            
            last_line = lines[-1].strip()
            if not last_line:
                if len(lines) < 3: return None
                last_line = lines[-2].strip()
                
            parts = last_line.split(',')
            if len(parts) < 2:
                return None
            
            # Remove dots (thousand separators)
            elapsed_ms = float(parts[0].replace('.', ''))
            executed_count = float(parts[1].replace('.', ''))
            
            if elapsed_ms <= 0:
                return None
                
            ops_per_second = executed_count / (elapsed_ms / 1000.0)
            makespan_s = elapsed_ms / 1000.0
            
            return {
                'ops_per_second': ops_per_second,
                'makespan_s': makespan_s
            }
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return None

def main():
    input_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output_base_dir = sys.argv[2] if len(sys.argv) > 2 else "aggregated_results"
    
    throughput_dir = os.path.join(output_base_dir, "throughput")
    makespan_dir = os.path.join(output_base_dir, "makespan")
    
    for d in [throughput_dir, makespan_dir]:
        if not os.path.exists(d):
            os.makedirs(d)
        
    # experiment_key -> list of metrics
    results = defaultdict(list)
    # experiment_key -> metadata
    metadata = {}
    
    files = [f for f in os.listdir(input_dir) if f.endswith('.csv') and '(' in f]
    
    for f in files:
        params = parse_filename(f)
        if not params:
            continue
            
        metrics = calculate_metrics(os.path.join(input_dir, f))
        if metrics is not None:
            key = params['key']
            results[key].append(metrics)
            if key not in metadata:
                metadata[key] = params

    # Group by workload and engine for output files
    grouped_throughput = defaultdict(list)
    grouped_makespan = defaultdict(list)
    
    for key, metrics_list in results.items():
        meta = metadata[key]
        
        mean_ops = sum(m['ops_per_second'] for m in metrics_list) / len(metrics_list)
        mean_makespan = sum(m['makespan_s'] for m in metrics_list) / len(metrics_list)
        
        output_key = f"{meta['workload']}__{meta['storage_engine']}"
        
        common_meta = {
            'workload': meta['workload'],
            'workers': meta['workers'],
            'storage_type': meta['storage_type'],
            'partitions': meta['partitions'],
            'storage_engine': meta['storage_engine'],
            'paths': meta['paths'],
            'interval': meta['interval'],
        }
        
        grouped_throughput[output_key].append({**common_meta, 'ops_per_second': mean_ops})
        grouped_makespan[output_key].append({**common_meta, 'makespan_s': mean_makespan})

    # Write throughput CSVs
    for output_key, data_list in grouped_throughput.items():
        output_file = os.path.join(throughput_dir, f"{output_key}.csv")
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=['workload', 'workers', 'storage_type', 'partitions', 'storage_engine', 'paths', 'interval', 'ops_per_second'])
            writer.writeheader()
            sorted_data = sorted(data_list, key=lambda x: (x['workers'], x['storage_type'], x['partitions']))
            writer.writerows(sorted_data)
        print(f"Created throughput CSV: {output_file}")

    # Write makespan CSVs
    for output_key, data_list in grouped_makespan.items():
        output_file = os.path.join(makespan_dir, f"{output_key}.csv")
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=['workload', 'workers', 'storage_type', 'partitions', 'storage_engine', 'paths', 'interval', 'makespan_s'])
            writer.writeheader()
            sorted_data = sorted(data_list, key=lambda x: (x['workers'], x['storage_type'], x['partitions']))
            writer.writerows(sorted_data)
        print(f"Created makespan CSV: {output_file}")

if __name__ == "__main__":
    main()
