#!/usr/bin/env python3
"""
FastDFS Benchmark Report Generator

Generates HTML/PDF reports from benchmark results
"""

import json
import argparse
import sys
from datetime import datetime
from pathlib import Path
import statistics

# HTML template
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FastDFS Benchmark Report</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
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
            margin-bottom: 30px;
        }}
        
        h2 {{
            color: #34495e;
            margin-top: 30px;
            margin-bottom: 15px;
            padding-bottom: 5px;
            border-bottom: 2px solid #ecf0f1;
        }}
        
        h3 {{
            color: #7f8c8d;
            margin-top: 20px;
            margin-bottom: 10px;
        }}
        
        .metadata {{
            background: #ecf0f1;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 30px;
        }}
        
        .metadata p {{
            margin: 5px 0;
        }}
        
        .metric-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }}
        
        .metric-card {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }}
        
        .metric-card.success {{
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
        }}
        
        .metric-card.warning {{
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
        }}
        
        .metric-card.info {{
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
        }}
        
        .metric-label {{
            font-size: 14px;
            opacity: 0.9;
            margin-bottom: 5px;
        }}
        
        .metric-value {{
            font-size: 32px;
            font-weight: bold;
        }}
        
        .metric-unit {{
            font-size: 16px;
            opacity: 0.8;
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
            font-weight: 600;
        }}
        
        tr:hover {{
            background: #f5f5f5;
        }}
        
        .benchmark-section {{
            margin: 40px 0;
            padding: 20px;
            background: #fafafa;
            border-radius: 5px;
        }}
        
        .chart-container {{
            margin: 20px 0;
            padding: 20px;
            background: white;
            border-radius: 5px;
        }}
        
        .progress-bar {{
            width: 100%;
            height: 30px;
            background: #ecf0f1;
            border-radius: 15px;
            overflow: hidden;
            margin: 10px 0;
        }}
        
        .progress-fill {{
            height: 100%;
            background: linear-gradient(90deg, #11998e 0%, #38ef7d 100%);
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
            transition: width 0.3s ease;
        }}
        
        .footer {{
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
            text-align: center;
            color: #7f8c8d;
        }}
        
        @media print {{
            body {{
                background: white;
            }}
            .container {{
                box-shadow: none;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 FastDFS Performance Benchmark Report</h1>
        
        <div class="metadata">
            <p><strong>Generated:</strong> {generated_time}</p>
            <p><strong>Benchmark Date:</strong> {benchmark_time}</p>
            <p><strong>System:</strong> {system_info}</p>
            <p><strong>Tracker:</strong> {tracker_server}</p>
        </div>
        
        <h2>Executive Summary</h2>
        <div class="metric-grid">
            {summary_metrics}
        </div>
        
        {benchmark_sections}
        
        <div class="footer">
            <p>Generated by FastDFS Benchmark Suite v1.0.0</p>
            <p>Report generated at {generated_time}</p>
        </div>
    </div>
</body>
</html>
"""


def load_results(input_file):
    """Load benchmark results from JSON file"""
    try:
        with open(input_file, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File not found: {input_file}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {input_file}: {e}", file=sys.stderr)
        sys.exit(1)


def format_bytes(bytes_val):
    """Format bytes to human readable format"""
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if bytes_val < 1024.0:
            return f"{bytes_val:.2f} {unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:.2f} PB"


def format_number(num):
    """Format number with thousands separator"""
    return f"{num:,.2f}"


def create_metric_card(label, value, unit="", card_type="info"):
    """Create a metric card HTML"""
    return f"""
    <div class="metric-card {card_type}">
        <div class="metric-label">{label}</div>
        <div class="metric-value">{value} <span class="metric-unit">{unit}</span></div>
    </div>
    """


def create_progress_bar(label, percentage):
    """Create a progress bar HTML"""
    return f"""
    <div>
        <strong>{label}</strong>
        <div class="progress-bar">
            <div class="progress-fill" style="width: {percentage}%">
                {percentage:.1f}%
            </div>
        </div>
    </div>
    """


def create_latency_table(latency_data):
    """Create latency statistics table"""
    if not latency_data:
        return "<p>No latency data available</p>"
    
    html = """
    <table>
        <thead>
            <tr>
                <th>Metric</th>
                <th>Value (ms)</th>
            </tr>
        </thead>
        <tbody>
    """
    
    metrics = [
        ('Mean', 'mean'),
        ('Median (p50)', 'median'),
        ('p75', 'p75'),
        ('p90', 'p90'),
        ('p95', 'p95'),
        ('p99', 'p99'),
        ('Min', 'min'),
        ('Max', 'max'),
        ('Std Dev', 'stddev')
    ]
    
    for label, key in metrics:
        value = latency_data.get(key, 0)
        html += f"<tr><td>{label}</td><td>{value:.2f}</td></tr>\n"
    
    html += "</tbody></table>"
    return html


def generate_upload_section(data):
    """Generate upload benchmark section"""
    if 'metrics' not in data:
        return ""
    
    metrics = data['metrics']
    config = data.get('configuration', {})
    
    html = """
    <div class="benchmark-section">
        <h2>📤 Upload Performance</h2>
        <p><strong>Configuration:</strong> {threads} threads, {file_count} files, {file_size} file size</p>
        
        <div class="metric-grid">
    """.format(
        threads=config.get('threads', 'N/A'),
        file_count=config.get('file_count', 'N/A'),
        file_size=format_bytes(config.get('file_size', 0))
    )
    
    html += create_metric_card(
        "Throughput",
        format_number(metrics.get('throughput_mbps', 0)),
        "MB/s",
        "success"
    )
    
    html += create_metric_card(
        "IOPS",
        format_number(metrics.get('iops', 0)),
        "ops/s",
        "info"
    )
    
    ops = metrics.get('operations', {})
    success_rate = ops.get('success_rate', 0)
    html += create_metric_card(
        "Success Rate",
        f"{success_rate:.1f}",
        "%",
        "success" if success_rate > 95 else "warning"
    )
    
    html += create_metric_card(
        "Duration",
        format_number(metrics.get('duration_seconds', 0)),
        "seconds",
        "info"
    )
    
    html += "</div>"
    
    # Latency statistics
    if 'latency_ms' in metrics:
        html += "<h3>Latency Statistics</h3>"
        html += create_latency_table(metrics['latency_ms'])
    
    # Operations breakdown
    if 'operations' in metrics:
        ops = metrics['operations']
        html += "<h3>Operations</h3>"
        html += create_progress_bar(
            f"Successful: {ops.get('successful', 0)} / {ops.get('total', 0)}",
            ops.get('success_rate', 0)
        )
    
    html += "</div>"
    return html


def generate_download_section(data):
    """Generate download benchmark section"""
    if 'metrics' not in data:
        return ""
    
    metrics = data['metrics']
    config = data.get('configuration', {})
    
    html = """
    <div class="benchmark-section">
        <h2>📥 Download Performance</h2>
        <p><strong>Configuration:</strong> {threads} threads, {iterations} iterations</p>
        
        <div class="metric-grid">
    """.format(
        threads=config.get('threads', 'N/A'),
        iterations=config.get('iterations', 'N/A')
    )
    
    html += create_metric_card(
        "Throughput",
        format_number(metrics.get('throughput_mbps', 0)),
        "MB/s",
        "success"
    )
    
    html += create_metric_card(
        "IOPS",
        format_number(metrics.get('iops', 0)),
        "ops/s",
        "info"
    )
    
    ops = metrics.get('operations', {})
    success_rate = ops.get('success_rate', 0)
    html += create_metric_card(
        "Success Rate",
        f"{success_rate:.1f}",
        "%",
        "success" if success_rate > 95 else "warning"
    )
    
    html += "</div>"
    
    if 'latency_ms' in metrics:
        html += "<h3>Latency Statistics</h3>"
        html += create_latency_table(metrics['latency_ms'])
    
    html += "</div>"
    return html


def generate_concurrent_section(data):
    """Generate concurrent operations section"""
    if 'metrics' not in data:
        return ""
    
    metrics = data['metrics']
    config = data.get('configuration', {})
    
    html = """
    <div class="benchmark-section">
        <h2>👥 Concurrent Operations</h2>
        <p><strong>Configuration:</strong> {users} users, {duration}s duration, mix: {mix}</p>
        
        <div class="metric-grid">
    """.format(
        users=config.get('users', 'N/A'),
        duration=config.get('duration', 'N/A'),
        mix=config.get('operation_mix', 'N/A')
    )
    
    ops = metrics.get('operations', {})
    html += create_metric_card(
        "Total Operations",
        format_number(ops.get('total', 0)),
        "ops",
        "info"
    )
    
    html += create_metric_card(
        "Ops/Second",
        format_number(ops.get('per_second', 0)),
        "ops/s",
        "success"
    )
    
    html += "</div>"
    
    # Operation breakdown
    html += "<h3>Operation Breakdown</h3>"
    
    if 'uploads' in ops:
        uploads = ops['uploads']
        html += create_progress_bar(
            f"Uploads: {uploads.get('successful', 0)} / {uploads.get('total', 0)}",
            uploads.get('success_rate', 0)
        )
    
    if 'downloads' in ops:
        downloads = ops['downloads']
        html += create_progress_bar(
            f"Downloads: {downloads.get('successful', 0)} / {downloads.get('total', 0)}",
            downloads.get('success_rate', 0)
        )
    
    if 'deletes' in ops:
        deletes = ops['deletes']
        html += create_progress_bar(
            f"Deletes: {deletes.get('successful', 0)} / {deletes.get('total', 0)}",
            deletes.get('success_rate', 0)
        )
    
    html += "</div>"
    return html


def generate_small_files_section(data):
    """Generate small files section"""
    if 'metrics' not in data:
        return ""
    
    metrics = data['metrics']
    config = data.get('configuration', {})
    
    html = """
    <div class="benchmark-section">
        <h2>📄 Small Files Performance</h2>
        <p><strong>Configuration:</strong> {threads} threads, {count} files, {min_size} - {max_size}</p>
        
        <div class="metric-grid">
    """.format(
        threads=config.get('threads', 'N/A'),
        count=config.get('file_count', 'N/A'),
        min_size=format_bytes(config.get('min_size', 0)),
        max_size=format_bytes(config.get('max_size', 0))
    )
    
    html += create_metric_card(
        "Throughput",
        format_number(metrics.get('throughput_mbps', 0)),
        "MB/s",
        "success"
    )
    
    html += create_metric_card(
        "IOPS",
        format_number(metrics.get('iops', 0)),
        "ops/s",
        "info"
    )
    
    html += create_metric_card(
        "Avg File Size",
        format_number(metrics.get('avg_file_size_kb', 0)),
        "KB",
        "info"
    )
    
    html += "</div>"
    
    if 'latency_ms' in metrics:
        html += "<h3>Latency Statistics</h3>"
        html += create_latency_table(metrics['latency_ms'])
    
    html += "</div>"
    return html


def generate_large_files_section(data):
    """Generate large files section"""
    if 'metrics' not in data:
        return ""
    
    metrics = data['metrics']
    config = data.get('configuration', {})
    
    html = """
    <div class="benchmark-section">
        <h2>📦 Large Files Performance</h2>
        <p><strong>Configuration:</strong> {threads} threads, {count} files, {min_size} - {max_size}</p>
        
        <div class="metric-grid">
    """.format(
        threads=config.get('threads', 'N/A'),
        count=config.get('file_count', 'N/A'),
        min_size=format_number(config.get('min_size_mb', 0)) + " MB",
        max_size=format_number(config.get('max_size_mb', 0)) + " MB"
    )
    
    html += create_metric_card(
        "Throughput",
        format_number(metrics.get('throughput_mbps', 0)),
        "MB/s",
        "success"
    )
    
    html += create_metric_card(
        "Total Data",
        format_number(metrics.get('total_gb', 0)),
        "GB",
        "info"
    )
    
    html += create_metric_card(
        "Avg File Size",
        format_number(metrics.get('avg_file_size_mb', 0)),
        "MB",
        "info"
    )
    
    html += "</div>"
    
    if 'latency_seconds' in metrics:
        html += "<h3>Latency Statistics (seconds)</h3>"
        html += create_latency_table(metrics['latency_seconds'])
    
    html += "</div>"
    return html


def generate_metadata_section(data):
    """Generate metadata operations section"""
    if 'metrics' not in data:
        return ""
    
    metrics = data['metrics']
    config = data.get('configuration', {})
    
    html = """
    <div class="benchmark-section">
        <h2>🏷️ Metadata Operations</h2>
        <p><strong>Configuration:</strong> {threads} threads, {ops} operations, mix: {mix}</p>
        
        <div class="metric-grid">
    """.format(
        threads=config.get('threads', 'N/A'),
        ops=config.get('operation_count', 'N/A'),
        mix=config.get('operation_mix', 'N/A')
    )
    
    html += create_metric_card(
        "Total Operations",
        format_number(metrics.get('total_operations', 0)),
        "ops",
        "info"
    )
    
    html += create_metric_card(
        "Ops/Second",
        format_number(metrics.get('ops_per_second', 0)),
        "ops/s",
        "success"
    )
    
    html += "</div>"
    
    # Operation breakdown
    if 'operations' in metrics:
        ops = metrics['operations']
        html += "<h3>Operation Breakdown</h3>"
        
        for op_type in ['query', 'update', 'delete']:
            if op_type in ops:
                op_data = ops[op_type]
                html += create_progress_bar(
                    f"{op_type.capitalize()}: {op_data.get('successful', 0)} / {op_data.get('total', 0)}",
                    op_data.get('success_rate', 0)
                )
    
    html += "</div>"
    return html


def generate_report(results, output_file):
    """Generate HTML report"""
    
    # Extract system info
    system_info = results.get('system_info', {})
    system_str = f"{system_info.get('hostname', 'Unknown')} - {system_info.get('os', 'Unknown')} {system_info.get('kernel', '')}"
    
    # Extract configuration
    config = results.get('configuration', {})
    tracker = config.get('tracker_server', 'Unknown')
    
    # Generate timestamp
    generated_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    benchmark_time = results.get('timestamp', 'Unknown')
    
    # Generate summary metrics
    summary_metrics = ""
    benchmark_results = results.get('results', {})
    
    # Calculate overall metrics
    total_throughput = 0
    total_iops = 0
    count = 0
    
    for bench_name, bench_data in benchmark_results.items():
        if 'metrics' in bench_data:
            metrics = bench_data['metrics']
            if 'throughput_mbps' in metrics:
                total_throughput += metrics['throughput_mbps']
                count += 1
            if 'iops' in metrics:
                total_iops += metrics['iops']
    
    if count > 0:
        summary_metrics += create_metric_card(
            "Avg Throughput",
            format_number(total_throughput / count),
            "MB/s",
            "success"
        )
        summary_metrics += create_metric_card(
            "Total IOPS",
            format_number(total_iops),
            "ops/s",
            "info"
        )
    
    summary_metrics += create_metric_card(
        "Benchmarks Run",
        str(len(benchmark_results)),
        "tests",
        "info"
    )
    
    # Generate benchmark sections
    benchmark_sections = ""
    
    section_generators = {
        'upload': generate_upload_section,
        'download': generate_download_section,
        'concurrent': generate_concurrent_section,
        'small_files': generate_small_files_section,
        'large_files': generate_large_files_section,
        'metadata': generate_metadata_section
    }
    
    for bench_name, bench_data in benchmark_results.items():
        if bench_name in section_generators:
            benchmark_sections += section_generators[bench_name](bench_data)
    
    # Generate final HTML
    html = HTML_TEMPLATE.format(
        generated_time=generated_time,
        benchmark_time=benchmark_time,
        system_info=system_str,
        tracker_server=tracker,
        summary_metrics=summary_metrics,
        benchmark_sections=benchmark_sections
    )
    
    # Write to file
    with open(output_file, 'w') as f:
        f.write(html)
    
    print(f"✓ Report generated: {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate HTML report from FastDFS benchmark results'
    )
    parser.add_argument(
        '-i', '--input',
        required=True,
        help='Input JSON file with benchmark results'
    )
    parser.add_argument(
        '-o', '--output',
        default='report.html',
        help='Output HTML file (default: report.html)'
    )
    
    args = parser.parse_args()
    
    # Load results
    print(f"Loading results from {args.input}...")
    results = load_results(args.input)
    
    # Generate report
    print(f"Generating report...")
    generate_report(results, args.output)
    
    print(f"\n✓ Report successfully generated!")
    print(f"  Open {args.output} in your browser to view the report.")


if __name__ == '__main__':
    main()
