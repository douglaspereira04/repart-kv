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
        # Try to see if it matches the older format or if something is missing
        # The user's example has 7 parts before (REP)
        # ycsb_a (1) __ 1 (2) __ engine (3) __ 1 (4) __ tkrzw_tree (5) __ 1 (6) __ 0 (7)
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

def calculate_ops_per_second(filepath):
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
                
            return executed_count / (elapsed_ms / 1000.0)
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return None

def main():
    input_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "aggregated_results"
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    # experiment_key -> list of ops_per_second
    results = defaultdict(list)
    # experiment_key -> metadata
    metadata = {}
    
    files = [f for f in os.listdir(input_dir) if f.endswith('.csv') and '(' in f]
    
    for f in files:
        params = parse_filename(f)
        if not params:
            continue
            
        ops = calculate_ops_per_second(os.path.join(input_dir, f))
        if ops is not None:
            key = params['key']
            results[key].append(ops)
            if key not in metadata:
                metadata[key] = params

    # Group by workload and engine for output files
    grouped_results = defaultdict(list)
    for key, ops_list in results.items():
        meta = metadata[key]
        mean_ops = sum(ops_list) / len(ops_list)
        
        output_key = f"{meta['workload']}__{meta['storage_engine']}"
        grouped_results[output_key].append({
            'workload': meta['workload'],
            'workers': meta['workers'],
            'storage_type': meta['storage_type'],
            'partitions': meta['partitions'],
            'storage_engine': meta['storage_engine'],
            'paths': meta['paths'],
            'interval': meta['interval'],
            'ops_per_second': mean_ops
        })

    for output_key, data_list in grouped_results.items():
        output_file = os.path.join(output_dir, f"{output_key}.csv")
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=['workload', 'workers', 'storage_type', 'partitions', 'storage_engine', 'paths', 'interval', 'ops_per_second'])
            writer.writeheader()
            # Sort by workers for consistency
            sorted_data = sorted(data_list, key=lambda x: (x['workers'], x['storage_type'], x['partitions']))
            writer.writerows(sorted_data)
        print(f"Created {output_file}")

if __name__ == "__main__":
    main()
