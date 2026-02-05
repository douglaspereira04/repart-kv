#!/usr/bin/env python3
import os
import re
import csv
import sys
from collections import defaultdict

def parse_filename(filename):
    # Format: workload__testworkers__storagetype__partitions__storageengine__paths__interval(REP).csv
    name = filename.rsplit('.', 1)[0]
    match = re.search(r'\((\d+)\)$', name)
    if not match:
        return None
    
    rep = int(match.group(1))
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
        'config_key': name_no_rep,
        'chart_key': f"{parts[0]}__{parts[4]}__{parts[1]}" # workload, engine, workers
    }

def process_file(filepath):
    results = []
    try:
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            prev_time = None
            prev_count = None
            
            for row in reader:
                # Handle thousand separators (dots)
                time_ms = float(row['elapsed_time_ms'].replace('.', ''))
                count = float(row['executed_count'].replace('.', ''))
                
                # Check for tracking and repartitioning flags
                # Assuming 'x' means active, empty or other means inactive
                tracking = 1 if row.get('Tracking', '').strip().lower() == 'x' else 0
                repartitioning = 1 if row.get('Repartitioning', '').strip().lower() == 'x' else 0
                
                if prev_time is not None:
                    delta_time_s = (time_ms - prev_time) / 1000.0
                    delta_count = count - prev_count
                    
                    if delta_time_s > 0:
                        throughput = delta_count / delta_time_s
                        # Round time to 1 decimal place (100ms) for alignment
                        elapsed_s = round(time_ms / 1000.0, 1)
                        results.append({
                            'elapsed_s': elapsed_s,
                            'throughput': throughput,
                            'tracking': tracking,
                            'repartitioning': repartitioning
                        })
                
                prev_time = time_ms
                prev_count = count
        return results
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return None

def main():
    input_dir = "/home/douglas/Documentos/repart-kv/results"
    output_dir = "/home/douglas/Documentos/repart-kv/results/aggregated_throughput_time"
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    config_groups = defaultdict(list)
    files = [f for f in os.listdir(input_dir) if f.endswith('.csv') and '(' in f]
    
    for f in files:
        params = parse_filename(f)
        if params:
            config_groups[params['config_key']].append((f, params))
            
    chart_data = defaultdict(list)
    
    for config_key, file_list in config_groups.items():
        print(f"Processing config: {config_key}")
        params = file_list[0][1]
        
        # time -> list of data points from different repetitions
        time_points = defaultdict(list)
        
        for f, p in file_list:
            data = process_file(os.path.join(input_dir, f))
            if data:
                for entry in data:
                    time_points[entry['elapsed_s']].append(entry)
        
        if not time_points:
            continue
            
        # For each time point, calculate mean, min, max and event probabilities
        aggregated_rows = []
        for t in sorted(time_points.keys()):
            entries = time_points[t]
            tps = [e['throughput'] for e in entries]
            
            mean_tp = sum(tps) / len(tps)
            min_tp = min(tps)
            max_tp = max(tps)
            
            # Probability/Frequency of tracking and repartitioning at this time point
            tracking_freq = sum(e['tracking'] for e in entries) / len(entries)
            repartitioning_freq = sum(e['repartitioning'] for e in entries) / len(entries)
            
            aggregated_rows.append({
                'elapsed_s': t,
                'mean': mean_tp,
                'min': min_tp,
                'max': max_tp,
                'tracking_prob': tracking_freq,
                'repartitioning_prob': repartitioning_freq,
                'storage_type': params['storage_type'],
                'partitions': params['partitions'],
                'paths': params['paths'],
                'interval': params['interval'],
                'line_label': f"{params['storage_type']}_p{params['partitions']}_w{params['paths']}_i{params['interval']}"
            })
        
        chart_data[params['chart_key']].extend(aggregated_rows)
        
    for chart_key, rows in chart_data.items():
        output_file = os.path.join(output_dir, f"{chart_key}.csv")
        if not rows: continue
        
        fieldnames = [
            'elapsed_s', 'mean', 'min', 'max', 
            'tracking_prob', 'repartitioning_prob',
            'storage_type', 'partitions', 'paths', 'interval', 'line_label'
        ]
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        print(f"Created chart data: {output_file}")

if __name__ == "__main__":
    main()
