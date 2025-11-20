#!/bin/bash

###############################################################################
# FastDFS Benchmark Runner
# 
# Runs all benchmarks and collects results
###############################################################################

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCHMARK_DIR="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$BENCHMARK_DIR/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/benchmark_${TIMESTAMP}.json"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
TRACKER_SERVER="127.0.0.1:22122"
THREADS=10
VERBOSE=0

###############################################################################
# Functions
###############################################################################

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Run all FastDFS benchmarks and generate comprehensive report.

Options:
    -t, --tracker SERVER    Tracker server address (default: 127.0.0.1:22122)
    -T, --threads NUM       Number of threads (default: 10)
    -o, --output FILE       Output file (default: results/benchmark_TIMESTAMP.json)
    -v, --verbose           Enable verbose output
    -h, --help              Show this help message

Examples:
    $0
    $0 --tracker 192.168.1.100:22122 --threads 20
    $0 -v -o my_results.json

EOF
}

check_prerequisites() {
    print_header "Checking Prerequisites"
    
    # Check if benchmark binaries exist
    local missing=0
    
    for bench in benchmark_upload benchmark_download benchmark_concurrent \
                 benchmark_small_files benchmark_large_files benchmark_metadata; do
        if [ ! -f "$BENCHMARK_DIR/$bench" ]; then
            print_error "Missing benchmark: $bench"
            missing=1
        fi
    done
    
    if [ $missing -eq 1 ]; then
        print_error "Some benchmarks are missing. Please run 'make' first."
        exit 1
    fi
    
    # Check if tracker is reachable
    print_info "Checking tracker connectivity: $TRACKER_SERVER"
    local tracker_host=$(echo $TRACKER_SERVER | cut -d: -f1)
    local tracker_port=$(echo $TRACKER_SERVER | cut -d: -f2)
    
    if command -v nc &> /dev/null; then
        if nc -z -w5 $tracker_host $tracker_port 2>/dev/null; then
            print_success "Tracker is reachable"
        else
            print_warning "Cannot reach tracker at $TRACKER_SERVER"
            print_warning "Continuing anyway, but benchmarks may fail..."
        fi
    fi
    
    # Create results directory
    mkdir -p "$RESULTS_DIR"
    print_success "Results directory: $RESULTS_DIR"
    
    echo ""
}

run_benchmark() {
    local name=$1
    local binary=$2
    shift 2
    local args="$@"
    
    print_header "Running $name Benchmark"
    print_info "Command: $binary $args"
    
    local output_file="$RESULTS_DIR/${name}_${TIMESTAMP}.json"
    
    if [ $VERBOSE -eq 1 ]; then
        "$BENCHMARK_DIR/$binary" $args 2>&1 | tee "$output_file"
    else
        "$BENCHMARK_DIR/$binary" $args > "$output_file" 2>&1
    fi
    
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        print_success "$name benchmark completed"
        echo "$output_file"
        return 0
    else
        print_error "$name benchmark failed (exit code: $exit_code)"
        return 1
    fi
    
    echo ""
}

combine_results() {
    print_header "Combining Results"
    
    local upload_result="$RESULTS_DIR/upload_${TIMESTAMP}.json"
    local download_result="$RESULTS_DIR/download_${TIMESTAMP}.json"
    local concurrent_result="$RESULTS_DIR/concurrent_${TIMESTAMP}.json"
    local small_files_result="$RESULTS_DIR/small_files_${TIMESTAMP}.json"
    local large_files_result="$RESULTS_DIR/large_files_${TIMESTAMP}.json"
    local metadata_result="$RESULTS_DIR/metadata_${TIMESTAMP}.json"
    
    cat > "$RESULT_FILE" << EOF
{
  "benchmark_suite": "FastDFS Performance Benchmark",
  "version": "1.0.0",
  "timestamp": "$(date -Iseconds)",
  "system_info": {
    "hostname": "$(hostname)",
    "os": "$(uname -s)",
    "kernel": "$(uname -r)",
    "cpu": "$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs || echo 'Unknown')",
    "memory_gb": $(free -g 2>/dev/null | awk '/^Mem:/{print $2}' || echo 0)
  },
  "configuration": {
    "tracker_server": "$TRACKER_SERVER",
    "threads": $THREADS
  },
  "results": {
EOF

    # Add individual benchmark results
    local first=1
    for result_file in "$upload_result" "$download_result" "$concurrent_result" \
                       "$small_files_result" "$large_files_result" "$metadata_result"; do
        if [ -f "$result_file" ]; then
            if [ $first -eq 0 ]; then
                echo "," >> "$RESULT_FILE"
            fi
            first=0
            
            local bench_name=$(basename "$result_file" "_${TIMESTAMP}.json")
            echo "    \"$bench_name\": " >> "$RESULT_FILE"
            cat "$result_file" >> "$RESULT_FILE"
        fi
    done
    
    cat >> "$RESULT_FILE" << EOF

  }
}
EOF
    
    print_success "Combined results saved to: $RESULT_FILE"
    echo ""
}

generate_summary() {
    print_header "Benchmark Summary"
    
    echo "Results saved to: $RESULT_FILE"
    echo ""
    echo "Individual results:"
    ls -lh "$RESULTS_DIR"/*_${TIMESTAMP}.json 2>/dev/null || true
    echo ""
    
    print_info "To generate HTML report, run:"
    echo "  python scripts/generate_report.py --input $RESULT_FILE --output report.html"
    echo ""
}

###############################################################################
# Main
###############################################################################

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--tracker)
            TRACKER_SERVER="$2"
            shift 2
            ;;
        -T|--threads)
            THREADS="$2"
            shift 2
            ;;
        -o|--output)
            RESULT_FILE="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Main execution
print_header "FastDFS Benchmark Suite"
echo "Timestamp: $(date)"
echo "Tracker: $TRACKER_SERVER"
echo "Threads: $THREADS"
echo ""

check_prerequisites

# Run benchmarks
run_benchmark "upload" "benchmark_upload" \
    --tracker "$TRACKER_SERVER" \
    --threads $THREADS \
    --files 1000 \
    --size 1048576

run_benchmark "download" "benchmark_download" \
    --tracker "$TRACKER_SERVER" \
    --threads $THREADS \
    --iterations 1000 \
    --prepare 100

run_benchmark "concurrent" "benchmark_concurrent" \
    --tracker "$TRACKER_SERVER" \
    --users $THREADS \
    --duration 60 \
    --mix "50:45:5"

run_benchmark "small_files" "benchmark_small_files" \
    --tracker "$TRACKER_SERVER" \
    --threads $THREADS \
    --count 10000 \
    --min-size 1024 \
    --max-size 102400

run_benchmark "large_files" "benchmark_large_files" \
    --tracker "$TRACKER_SERVER" \
    --threads 5 \
    --count 10 \
    --min-size 104857600 \
    --max-size 524288000

run_benchmark "metadata" "benchmark_metadata" \
    --tracker "$TRACKER_SERVER" \
    --threads $THREADS \
    --operations 10000 \
    --mix "70:20:10"

# Combine results
combine_results

# Generate summary
generate_summary

print_success "All benchmarks completed successfully!"
exit 0
