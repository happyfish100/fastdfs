#!/usr/bin/env python3
"""
FastDFS Version Comparison Tool

Compares performance between two FastDFS versions
"""

import json
import argparse
import sys
from pathlib import Path


def load_results(filepath):
    """Load benchmark results from JSON file"""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {filepath}: {e}", file=sys.stderr)
        sys.exit(1)


def calculate_change(baseline, current):
    """Calculate percentage change"""
    if baseline == 0:
        return 0
    return ((current - baseline) / baseline) * 100


def format_change(change):
    """Format change with color indicator"""
    if change > 0:
        return f"+{change:.2f}% ⬆️"
    elif change < 0:
        return f"{change:.2f}% ⬇️"
    else:
        return "0.00% ➡️"


def compare_metrics(baseline_metrics, current_metrics, metric_name):
    """Compare a specific metric between versions"""
    baseline_val = baseline_metrics.get(metric_name, 0)
    current_val = current_metrics.get(metric_name, 0)
    change = calculate_change(baseline_val, current_val)
    
    return {
        'baseline': baseline_val,
        'current': current_val,
        'change': change,
        'change_str': format_change(change)
    }


def compare_benchmark(baseline_data, current_data, bench_name):
    """Compare a specific benchmark between versions"""
    print(f"\n{'='*80}")
    print(f"  {bench_name.upper()} BENCHMARK")
    print(f"{'='*80}")
    
    if 'metrics' not in baseline_data or 'metrics' not in current_data:
        print("  ⚠️  Insufficient data for comparison")
        return
    
    baseline_metrics = baseline_data['metrics']
    current_metrics = current_data['metrics']
    
    # Compare throughput
    if 'throughput_mbps' in baseline_metrics and 'throughput_mbps' in current_metrics:
        comp = compare_metrics(baseline_metrics, current_metrics, 'throughput_mbps')
        print(f"\n  Throughput:")
        print(f"    Baseline: {comp['baseline']:.2f} MB/s")
        print(f"    Current:  {comp['current']:.2f} MB/s")
        print(f"    Change:   {comp['change_str']}")
    
    # Compare IOPS
    if 'iops' in baseline_metrics and 'iops' in current_metrics:
        comp = compare_metrics(baseline_metrics, current_metrics, 'iops')
        print(f"\n  IOPS:")
        print(f"    Baseline: {comp['baseline']:.2f} ops/s")
        print(f"    Current:  {comp['current']:.2f} ops/s")
        print(f"    Change:   {comp['change_str']}")
    
    # Compare latency
    if 'latency_ms' in baseline_metrics and 'latency_ms' in current_metrics:
        baseline_latency = baseline_metrics['latency_ms']
        current_latency = current_metrics['latency_ms']
        
        print(f"\n  Latency (ms):")
        
        for metric in ['mean', 'p50', 'p95', 'p99']:
            if metric in baseline_latency and metric in current_latency:
                baseline_val = baseline_latency[metric]
                current_val = current_latency[metric]
                change = calculate_change(baseline_val, current_val)
                
                # For latency, lower is better, so invert the indicator
                if change < 0:
                    indicator = "⬆️ (better)"
                elif change > 0:
                    indicator = "⬇️ (worse)"
                else:
                    indicator = "➡️"
                
                print(f"    {metric.upper():6s}: {baseline_val:8.2f} → {current_val:8.2f} ({change:+.2f}%) {indicator}")
    
    # Compare success rate
    if 'operations' in baseline_metrics and 'operations' in current_metrics:
        baseline_ops = baseline_metrics['operations']
        current_ops = current_metrics['operations']
        
        if 'success_rate' in baseline_ops and 'success_rate' in current_ops:
            baseline_rate = baseline_ops['success_rate']
            current_rate = current_ops['success_rate']
            change = current_rate - baseline_rate
            
            print(f"\n  Success Rate:")
            print(f"    Baseline: {baseline_rate:.2f}%")
            print(f"    Current:  {current_rate:.2f}%")
            print(f"    Change:   {change:+.2f}% {'⬆️' if change > 0 else '⬇️' if change < 0 else '➡️'}")


def generate_html_comparison(baseline, current, output_file):
    """Generate HTML comparison report"""
    
    html = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FastDFS Version Comparison</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            line-height: 1.6;
            color: #333;
            background: #f5f5f5;
            padding: 20px;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 40px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        h1 {{
            color: #2c3e50;
            border-bottom: 3px solid #3498db;
            padding-bottom: 10px;
        }}
        h2 {{
            color: #34495e;
            margin-top: 30px;
            border-bottom: 2px solid #ecf0f1;
            padding-bottom: 5px;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background: #3498db;
            color: white;
        }}
        .improvement {{
            color: #27ae60;
            font-weight: bold;
        }}
        .regression {{
            color: #e74c3c;
            font-weight: bold;
        }}
        .neutral {{
            color: #95a5a6;
        }}
        .summary-box {{
            background: #ecf0f1;
            padding: 20px;
            border-radius: 5px;
            margin: 20px 0;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🔄 FastDFS Version Comparison</h1>
        
        <div class="summary-box">
            <h3>Comparison Details</h3>
            <p><strong>Baseline Version:</strong> {baseline_version}</p>
            <p><strong>Current Version:</strong> {current_version}</p>
            <p><strong>Baseline Date:</strong> {baseline_date}</p>
            <p><strong>Current Date:</strong> {current_date}</p>
        </div>
        
        {comparison_tables}
        
        <div class="summary-box">
            <h3>Summary</h3>
            <p>This comparison shows the performance differences between two versions of FastDFS.</p>
            <p>Green values indicate improvements, red values indicate regressions.</p>
        </div>
    </div>
</body>
</html>
"""
    
    baseline_version = baseline.get('fastdfs_version', 'Unknown')
    current_version = current.get('fastdfs_version', 'Unknown')
    baseline_date = baseline.get('timestamp', 'Unknown')
    current_date = current.get('timestamp', 'Unknown')
    
    comparison_tables = ""
    
    baseline_results = baseline.get('results', {})
    current_results = current.get('results', {})
    
    for bench_name in baseline_results.keys():
        if bench_name not in current_results:
            continue
        
        baseline_data = baseline_results[bench_name]
        current_data = current_results[bench_name]
        
        if 'metrics' not in baseline_data or 'metrics' not in current_data:
            continue
        
        baseline_metrics = baseline_data['metrics']
        current_metrics = current_data['metrics']
        
        comparison_tables += f"<h2>{bench_name.replace('_', ' ').title()}</h2>"
        comparison_tables += "<table><thead><tr><th>Metric</th><th>Baseline</th><th>Current</th><th>Change</th></tr></thead><tbody>"
        
        # Compare key metrics
        metrics_to_compare = [
            ('throughput_mbps', 'Throughput (MB/s)', True),
            ('iops', 'IOPS', True),
        ]
        
        for metric_key, metric_label, higher_is_better in metrics_to_compare:
            if metric_key in baseline_metrics and metric_key in current_metrics:
                baseline_val = baseline_metrics[metric_key]
                current_val = current_metrics[metric_key]
                change = calculate_change(baseline_val, current_val)
                
                if higher_is_better:
                    css_class = 'improvement' if change > 0 else 'regression' if change < 0 else 'neutral'
                else:
                    css_class = 'regression' if change > 0 else 'improvement' if change < 0 else 'neutral'
                
                comparison_tables += f"""
                <tr>
                    <td>{metric_label}</td>
                    <td>{baseline_val:.2f}</td>
                    <td>{current_val:.2f}</td>
                    <td class="{css_class}">{change:+.2f}%</td>
                </tr>
                """
        
        # Add latency comparison
        if 'latency_ms' in baseline_metrics and 'latency_ms' in current_metrics:
            baseline_latency = baseline_metrics['latency_ms']
            current_latency = current_metrics['latency_ms']
            
            for lat_metric in ['mean', 'p95', 'p99']:
                if lat_metric in baseline_latency and lat_metric in current_latency:
                    baseline_val = baseline_latency[lat_metric]
                    current_val = current_latency[lat_metric]
                    change = calculate_change(baseline_val, current_val)
                    
                    # For latency, lower is better
                    css_class = 'improvement' if change < 0 else 'regression' if change > 0 else 'neutral'
                    
                    comparison_tables += f"""
                    <tr>
                        <td>Latency {lat_metric.upper()} (ms)</td>
                        <td>{baseline_val:.2f}</td>
                        <td>{current_val:.2f}</td>
                        <td class="{css_class}">{change:+.2f}%</td>
                    </tr>
                    """
        
        comparison_tables += "</tbody></table>"
    
    final_html = html.format(
        baseline_version=baseline_version,
        current_version=current_version,
        baseline_date=baseline_date,
        current_date=current_date,
        comparison_tables=comparison_tables
    )
    
    with open(output_file, 'w') as f:
        f.write(final_html)
    
    print(f"\n✓ HTML comparison report generated: {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Compare FastDFS benchmark results between versions'
    )
    parser.add_argument(
        '-b', '--baseline',
        required=True,
        help='Baseline benchmark results (JSON)'
    )
    parser.add_argument(
        '-c', '--current',
        required=True,
        help='Current benchmark results (JSON)'
    )
    parser.add_argument(
        '-o', '--output',
        default='comparison.html',
        help='Output HTML file (default: comparison.html)'
    )
    parser.add_argument(
        '--text-only',
        action='store_true',
        help='Only print text comparison (no HTML)'
    )
    
    args = parser.parse_args()
    
    # Load results
    print(f"Loading baseline results from {args.baseline}...")
    baseline = load_results(args.baseline)
    
    print(f"Loading current results from {args.current}...")
    current = load_results(args.current)
    
    print("\n" + "="*80)
    print("  FASTDFS VERSION COMPARISON")
    print("="*80)
    
    baseline_version = baseline.get('fastdfs_version', 'Unknown')
    current_version = current.get('fastdfs_version', 'Unknown')
    
    print(f"\n  Baseline: {baseline_version} ({baseline.get('timestamp', 'Unknown')})")
    print(f"  Current:  {current_version} ({current.get('timestamp', 'Unknown')})")
    
    # Compare each benchmark
    baseline_results = baseline.get('results', {})
    current_results = current.get('results', {})
    
    for bench_name in baseline_results.keys():
        if bench_name in current_results:
            compare_benchmark(
                baseline_results[bench_name],
                current_results[bench_name],
                bench_name
            )
    
    # Generate HTML report
    if not args.text_only:
        print(f"\n{'='*80}")
        print("  Generating HTML report...")
        generate_html_comparison(baseline, current, args.output)
    
    print(f"\n{'='*80}")
    print("  Comparison complete!")
    print(f"{'='*80}\n")


if __name__ == '__main__':
    main()
