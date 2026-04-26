#!/usr/bin/env python3
import os
import re
import csv
import sys
from collections import defaultdict

def parse_filename(filename):
    # Format:
    # workload__testworkers__storagetype__partitions__storageengine__paths__interval__thinking__[sync_mode](REP).csv
    # thinking is in nanoseconds (ns)
    # Backward compat:
    # - 7 parts (no thinking, no sync_mode) -> thinking_time=0, sync_mode=sync_off
    # - 8 parts can be either thinking or sync_mode
    name = filename.rsplit('.', 1)[0]
    match = re.search(r'\((\d+)\)$', name)
    if not match:
        return None
    
    rep = int(match.group(1))
    name_no_rep = name[:match.start()]
    
    parts = name_no_rep.split('__')
    if len(parts) not in (7, 8, 9):
        return None

    thinking_time = 0
    sync_mode = "sync_off"
    if len(parts) == 8:
        if parts[7] in ("sync_on", "sync_off"):
            sync_mode = parts[7]
        else:
            thinking_time = int(parts[7])
    elif len(parts) == 9:
        thinking_time = int(parts[7])
        sync_mode = parts[8]

    return {
        'workload': parts[0],
        'workers': int(parts[1]),
        'storage_type': parts[2],
        'partitions': int(parts[3]),
        'storage_engine': parts[4],
        'paths': int(parts[5]),
        'interval': int(parts[6]),
        'thinking_time': thinking_time,
        'sync_mode': sync_mode,
        'rep': rep,
        'config_key': name_no_rep,
        'chart_key': f"{parts[0]}__{parts[4]}__{parts[1]}__{thinking_time}__{sync_mode}"  # workload, engine, workers, thinking, sync mode
    }

def process_file(filepath):
    results = []
    try:
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            prev_time = None
            prev_count = None
            
            for row in reader:
                time_ms = parse_number(row['elapsed_time_ms'])
                count = parse_number(row['executed_count'])
                memory_kb = parse_number(row.get('memory_kb', '') or '0')
                disk_kb = parse_number(row.get('disk_kb', '') or '0')

                # C++ writes 'o' for active, 'x' for inactive
                tracking = 1 if row.get('Tracking', '').strip().lower() == 'o' else 0
                repartitioning = 1 if row.get('Repartitioning', '').strip().lower() == 'o' else 0

                if prev_time is not None:
                    delta_time_s = (time_ms - prev_time) / 1000.0
                    delta_count = count - prev_count

                    if delta_time_s > 0:
                        throughput = delta_count / delta_time_s
                        # Round time to 2 decimal places (100ms) for alignment
                        # (metrics are sampled every 100ms)
                        elapsed_s = round(time_ms / 1000.0, 2)
                        results.append({
                            'elapsed_s': elapsed_s,
                            'throughput': throughput,
                            'memory_kb': memory_kb,
                            'disk_kb': disk_kb,
                            'tracking': tracking,
                            'repartitioning': repartitioning
                        })
                
                prev_time = time_ms
                prev_count = count
        return results
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return None

def parse_number(s):
    """Parse number with dot as thousand separator (C++ format_with_separators output)."""
    s = str(s).strip().replace('.', '')
    return float(s) if s else 0.0

def main():
    input_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "aggregated_throughput_time"

    # Resolve to absolute paths for consistent behavior
    input_dir = os.path.abspath(input_dir)
    output_dir = os.path.abspath(output_dir)

    if not os.path.exists(input_dir):
        print(f"Error: Input directory does not exist: {input_dir}", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    config_groups = defaultdict(list)
    files = [f for f in os.listdir(input_dir) if f.endswith('.csv') and '(' in f and '_latency' not in f]
    
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
            mems = [e['memory_kb'] for e in entries]
            disks = [e['disk_kb'] for e in entries]

            mean_tp = sum(tps) / len(tps)
            min_tp = min(tps)
            max_tp = max(tps)

            mean_mem = sum(mems) / len(mems)
            min_mem = min(mems)
            max_mem = max(mems)

            mean_disk = sum(disks) / len(disks)
            min_disk = min(disks)
            max_disk = max(disks)

            # Probability/Frequency of tracking and repartitioning at this time point
            tracking_freq = sum(e['tracking'] for e in entries) / len(entries)
            repartitioning_freq = sum(e['repartitioning'] for e in entries) / len(entries)

            aggregated_rows.append({
                'elapsed_s': t,
                'mean': mean_tp,
                'min': min_tp,
                'max': max_tp,
                'memory_kb_mean': mean_mem,
                'memory_kb_min': min_mem,
                'memory_kb_max': max_mem,
                'disk_kb_mean': mean_disk,
                'disk_kb_min': min_disk,
                'disk_kb_max': max_disk,
                'tracking_prob': tracking_freq,
                'repartitioning_prob': repartitioning_freq,
                'storage_type': params['storage_type'],
                'partitions': params['partitions'],
                'paths': params['paths'],
                'interval': params['interval'],
                'thinking_time': params['thinking_time'],
                'sync_mode': params['sync_mode'],
                'line_label': f"{params['storage_type']}_p{params['partitions']}_w{params['paths']}_i{params['interval']}_t{params['thinking_time']}"
            })
        
        chart_data[params['chart_key']].extend(aggregated_rows)
        
    for chart_key, rows in chart_data.items():
        output_file = os.path.join(output_dir, f"{chart_key}.csv")
        if not rows: continue
        
        fieldnames = [
            'elapsed_s', 'mean', 'min', 'max',
            'memory_kb_mean', 'memory_kb_min', 'memory_kb_max',
            'disk_kb_mean', 'disk_kb_min', 'disk_kb_max',
            'tracking_prob', 'repartitioning_prob',
            'storage_type', 'partitions', 'paths', 'interval', 'thinking_time', 'sync_mode', 'line_label'
        ]
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        print(f"Created chart data: {output_file}")

if __name__ == "__main__":
    main()
