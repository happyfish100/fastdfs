# FastDFS Benchmarking & Performance Testing Suite

## Overview

Comprehensive performance testing suite for FastDFS that measures throughput, IOPS, latency percentiles, and resource utilization. Use it to establish baselines, identify bottlenecks, compare versions, and optimize your FastDFS deployment.

## Features

✅ **6 Specialized Benchmarks**: Upload, Download, Concurrent, Small Files, Large Files, Metadata  
✅ **Multi-threaded Testing**: Simulate concurrent user loads  
✅ **Comprehensive Metrics**: Throughput (MB/s), IOPS, Latency (p50/p95/p99), Success Rates  
✅ **Automated Execution**: Run all benchmarks with a single command  
✅ **Beautiful HTML Reports**: Generate professional performance reports  
✅ **Version Comparison**: Compare performance across FastDFS versions  
✅ **No External Dependencies**: Python scripts use only standard library (no pip install needed)

## Directory Structure

```
benchmarks/
├── README.md                   # This file
├── benchmark_upload.c          # Upload performance testing
├── benchmark_download.c        # Download performance testing
├── benchmark_concurrent.c      # Concurrent operations testing
├── benchmark_small_files.c     # Small file optimization testing
├── benchmark_large_files.c     # Large file performance
├── benchmark_metadata.c        # Metadata operations benchmark
├── results/                    # Benchmark results storage
│   └── template.json           # Result template
├── scripts/                    # Automation scripts
│   ├── run_all_benchmarks.sh   # Run all benchmarks
│   ├── generate_report.py      # Generate HTML/PDF reports
│   └── compare_versions.py     # Compare versions
└── config/                     # Configuration files
    └── benchmark.conf          # Benchmark configuration
```

## Prerequisites

- **FastDFS**: Installed and running (tracker + storage servers)
- **GCC Compiler**: For building C benchmarks
- **Make**: Build automation tool
- **Python 3.7+**: For report generation (no external packages needed)

**Platform Support**: Linux (recommended), macOS, Windows (use WSL)

## Quick Start (5 Minutes)

### 1. Build

```bash
cd benchmarks
make
```

### 2. Configure

Edit `config/benchmark.conf` and set your tracker server:

```ini
tracker_server = YOUR_TRACKER_IP:22122
```

### 3. Run Benchmarks

**Run all benchmarks:**
```bash
./scripts/run_all_benchmarks.sh
```

**Or run individual benchmark:**
```bash
./benchmark_upload --tracker 127.0.0.1:22122 --threads 10 --files 1000
```

### 4. Generate Report

```bash
python scripts/generate_report.py \
  --input results/benchmark_*.json \
  --output report.html
```

Open `report.html` in your browser to view results.

### 5. Compare Versions (Optional)

```bash
python scripts/compare_versions.py \
  --baseline results/v1.json \
  --current results/v2.json \
  --output comparison.html
```

## Benchmark Details

### 1. Upload Benchmark
Tests file upload performance with configurable concurrency.

```bash
./benchmark_upload --tracker 127.0.0.1:22122 --threads 10 --files 1000 --size 1048576
```

**Key Options**: `--threads`, `--files`, `--size`, `--warmup`  
**Use For**: Write performance, storage backend testing

### 2. Download Benchmark
Tests file download performance with multiple concurrent downloads.

```bash
./benchmark_download --tracker 127.0.0.1:22122 --threads 10 --iterations 1000
```

**Key Options**: `--threads`, `--iterations`, `--file-list`, `--prepare`  
**Use For**: Read performance, network bandwidth testing

### 3. Concurrent Operations
Simulates real-world mixed workload (upload/download/delete).

```bash
./benchmark_concurrent --tracker 127.0.0.1:22122 --users 50 --duration 300 --mix "50:45:5"
```

**Key Options**: `--users`, `--duration`, `--mix` (upload:download:delete ratio)  
**Use For**: Stress testing, capacity planning

### 4. Small Files Benchmark
Optimized for testing small file performance (<100KB).

```bash
./benchmark_small_files --tracker 127.0.0.1:22122 --threads 20 --count 10000
```

**Key Options**: `--threads`, `--count`, `--min-size`, `--max-size`  
**Use For**: High IOPS testing, metadata overhead measurement

### 5. Large Files Benchmark
Tests large file handling (>100MB).

```bash
./benchmark_large_files --tracker 127.0.0.1:22122 --threads 5 --count 10
```

**Key Options**: `--threads`, `--count`, `--min-size`, `--max-size`  
**Use For**: Streaming performance, disk I/O testing

### 6. Metadata Operations
Tests metadata query, update, and delete operations.

```bash
./benchmark_metadata --tracker 127.0.0.1:22122 --threads 10 --operations 10000
```

**Key Options**: `--threads`, `--operations`, `--mix` (query:update:delete ratio)  
**Use For**: Metadata performance, database backend testing

**Tip**: Run `./benchmark_* --help` for all available options.

## Understanding Results

### Key Metrics

**Throughput (MB/s)** - Data transfer rate (higher is better)  
- Good: >100 MB/s for modern systems

**IOPS** - Operations per second (higher is better)  
- Good: >1000 for small files, >100 for large files

**Latency (ms)** - Response time (lower is better)  
- **p50 (Median)**: 50% of requests complete within this time
- **p95**: 95% of requests complete within this time  
- **p99**: 99% of requests complete within this time
- Good: p95 < 50ms for small files

**Success Rate (%)** - Percentage of successful operations  
- Should be >99%

### Performance Indicators

✅ **Good Performance**
- High throughput (>100 MB/s)
- High IOPS (>1000 for small files)
- Low latency (p95 < 50ms)
- Success rate > 99%

⚠️ **Performance Issues**
- Low throughput → Check network, disk I/O
- High latency → Check system load, network latency
- Low success rate → Check logs, system resources

## Result Format

Benchmark results are stored in JSON format:

```json
{
  "benchmark": "upload",
  "timestamp": "2024-01-15T10:30:00Z",
  "version": "6.12.1",
  "config": {
    "threads": 10,
    "files": 1000,
    "file_size": 1048576
  },
  "metrics": {
    "throughput_mbps": 125.5,
    "iops": 1250,
    "latency_ms": {
      "mean": 8.2,
      "p50": 7.5,
      "p95": 15.3,
      "p99": 22.1,
      "max": 45.6
    },
    "success_rate": 99.8,
    "duration_seconds": 80.5
  },
  "resources": {
    "cpu_percent": 45.2,
    "memory_mb": 256.8,
    "network_mbps": 130.2
  }
}
```

## Common Use Cases

### 1. Baseline Performance Test
```bash
./scripts/run_all_benchmarks.sh --output results/baseline.json
```

### 2. Stress Testing
```bash
./benchmark_concurrent --users 100 --duration 300 --mix "50:45:5"
```

### 3. Version Comparison
```bash
# Before upgrade
./scripts/run_all_benchmarks.sh -o results/v6.11.json

# After upgrade
./scripts/run_all_benchmarks.sh -o results/v6.12.json

# Compare
python scripts/compare_versions.py \
  --baseline results/v6.11.json \
  --current results/v6.12.json \
  --output comparison.html
```

### 4. CI/CD Integration
```bash
#!/bin/bash
./scripts/run_all_benchmarks.sh --output results/ci_${BUILD_ID}.json
# Add threshold checks for automated validation
```

## Best Practices

1. **Warm-up Phase**: Enable warm-up to stabilize performance
   ```bash
   ./benchmark_upload --warmup 10 ...
   ```

2. **Isolated Environment**: Run on dedicated test systems (not production)

3. **Multiple Runs**: Run each test 3-5 times and average results

4. **Monitor Resources**: Watch CPU, memory, disk, network during tests
   ```bash
   # In separate terminal
   watch -n 1 'top -b -n 1 | head -20'
   iostat -x 1
   ```

5. **Document Conditions**: Record system state, configuration, and results

## Troubleshooting

### Build Issues

**Error: `fdfs_client.h: No such file or directory`**
```bash
# Update include paths in Makefile
CFLAGS = -I/path/to/fastdfs/client -I/path/to/fastdfs/common
```

**Error: `cannot find -lfdfsclient`**
```bash
# Set library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Runtime Issues

**Connection Refused**
- Check if tracker is running: `ps aux | grep fdfs_trackerd`
- Verify tracker address and port
- Test connectivity: `telnet tracker_ip 22122`
- Check firewall: `sudo iptables -L`

**Out of Memory**
- Reduce threads: `--threads 5`
- Reduce file size: `--size 524288`
- Reduce file count: `--files 100`
- Increase system memory

**Too Many Open Files**
```bash
# Increase file descriptor limit
ulimit -n 65536
```

### Performance Issues

**Low Throughput**
```bash
# Check network bandwidth
iperf3 -c tracker_server

# Check disk I/O
iostat -x 1

# Check CPU
top
```

**High Latency**
```bash
# Check network latency
ping tracker_server

# Check system load
uptime
vmstat 1
```

**Low Success Rate**
```bash
# Check FastDFS logs
tail -f /var/log/fastdfs/trackerd.log
tail -f /var/log/fastdfs/storaged.log

# Check system resources
free -h
df -h
```

### Windows/WSL Issues

**Build on Windows**
```powershell
# Install WSL
wsl --install

# In WSL terminal
cd /mnt/d/dev/bit/74/fastdfs/benchmarks
make
```

**Missing Build Tools in WSL**
```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc make
```

## Advanced Configuration

Customize `config/benchmark.conf` for default settings:

```ini
# Connection settings
tracker_server = 127.0.0.1:22122
connect_timeout = 30
network_timeout = 60

# Upload benchmark defaults
[upload]
threads = 10
file_count = 1000
file_size = 1048576
warmup_enabled = true

# Concurrent benchmark defaults
[concurrent]
concurrent_users = 50
duration = 300
operation_mix = 50:45:5
```

## Architecture

- **6 C Benchmark Programs**: ~1,666 lines of multi-threaded C code
- **3 Python Scripts**: Report generation using only standard library (no pip install)
- **Thread-safe Statistics**: Mutex-protected metrics collection
- **JSON Output**: Machine-readable results for integration
- **No External Dependencies**: Python uses only standard library

## FAQ

**Q: Do I need to install Python packages?**  
A: No! All Python scripts use only the standard library.

**Q: Can I run this on Windows?**  
A: Yes, use WSL (Windows Subsystem for Linux).

**Q: How long do benchmarks take?**  
A: Quick test: ~5 minutes. Full suite: ~30-60 minutes.

**Q: Can I run on production?**  
A: Not recommended. Benchmarks generate load that may impact production.

**Q: What FastDFS version is required?**  
A: Designed for FastDFS 6.x, compatible with 5.x.

## Contributing

To add new benchmarks:
1. Create `benchmark_newtest.c` following existing structure
2. Add to Makefile
3. Output results in JSON format
4. Update this README
5. Test thoroughly

## License

This benchmarking suite follows the same license as FastDFS.

## Support

- **FastDFS GitHub**: https://github.com/happyfish100/fastdfs
- **FastDFS Wiki**: https://github.com/happyfish100/fastdfs/wiki
- **Issues**: https://github.com/happyfish100/fastdfs/issues
