# FastDFS Prometheus Exporter

Prometheus exporter for FastDFS that exposes metrics for monitoring storage capacity, performance, and health.

## Metrics Exposed

- **Storage capacity and usage** - Total/free space per group and storage server
- **Upload/download rates** - Operation counts and bytes transferred
- **Connection counts** - Current and maximum connections
- **Error rates** - Failed operations (calculated from total vs success)
- **Sync status and delays** - Sync bytes and heartbeat delays
- **Disk I/O statistics** - Operation counts (upload, download, delete, append, modify)
- **Network throughput** - Bytes uploaded/downloaded

## Build

```bash
make
```

**Prerequisites:** FastDFS client library, FastCommon library, GCC

## Usage

```bash
./fdfs_exporter /etc/fdfs/client.conf [port]
```

- Default port: `9898`
- Metrics endpoint: `http://localhost:9898/metrics`

**Example:**
```bash
./fdfs_exporter /etc/fdfs/client.conf 9898
```

## Prometheus Configuration

```yaml
scrape_configs:
  - job_name: 'fastdfs'
    static_configs:
      - targets: ['localhost:9898']
    scrape_interval: 30s
```

## Grafana Dashboard

Import `grafana_dashboard.json` into Grafana for pre-built visualizations.

## License

GPL V3
