# FastDFS Health Check Service

Monitors FastDFS cluster health and sends alerts when issues are detected.

## Features

- **Tracker connectivity** - Verifies connection to tracker servers
- **Storage server status** - Checks if storage servers are active
- **Disk space monitoring** - Alerts on low disk space (warning: <20%, critical: <10%)
- **Heartbeat monitoring** - Detects unresponsive storage servers (>60s)
- **Error rate tracking** - Monitors upload/download failure rates
- **Alert management** - Sends alerts via log and syslog with cooldown (5 min)

## Build

```bash
make
```

**Prerequisites:** FastDFS client library, FastCommon library, GCC

## Usage

```bash
./health_checker /etc/fdfs/client.conf [options]
```

**Options:**
- `-d` - Run as daemon
- `-i <seconds>` - Check interval (default: 30, minimum: 10)

**Examples:**
```bash
# Foreground mode with default 30s interval
./health_checker /etc/fdfs/client.conf

# Daemon mode with 60s interval
./health_checker /etc/fdfs/client.conf -d -i 60
```

## Alert Thresholds

- **Disk Space Warning:** <20% free
- **Disk Space Critical:** <10% free
- **Heartbeat Timeout:** >60 seconds
- **Error Rate Warning:** >10% failures
- **Alert Cooldown:** 5 minutes (prevents duplicate alerts)

## Output

Health check results are printed to stdout and logged:
```
=== FastDFS Cluster Health Check ===
Overall Status: OK
Groups: 2 total, 2 healthy
Storage Servers: 4 total, 4 healthy, 0 warning, 0 critical
Timestamp: Tue Nov 19 21:00:00 2025
=====================================
```

Alerts are sent to:
- Application log (via FastCommon logger)
- System syslog (facility: daemon)

## Systemd Service

Create `/etc/systemd/system/fdfs-health-check.service`:

```ini
[Unit]
Description=FastDFS Health Check Service
After=network.target

[Service]
Type=simple
User=fdfs
ExecStart=/usr/local/bin/health_checker /etc/fdfs/client.conf -d -i 30
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
systemctl daemon-reload
systemctl enable fdfs-health-check
systemctl start fdfs-health-check
```

## License

GPL V3
