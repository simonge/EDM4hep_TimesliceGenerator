#!/bin/bash
# Memory monitoring script for CI pipeline
# Usage: monitor_memory.sh <PID> <output_csv>

PID=$1
OUTPUT=$2

if [ -z "$PID" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: $0 <PID> <output_csv>"
    exit 1
fi

echo "time_sec,rss_mb,vms_mb" > "$OUTPUT"
START=$(date +%s)

while kill -0 $PID 2>/dev/null; do
    CURRENT=$(date +%s)
    ELAPSED=$((CURRENT - START))
    if [ -f /proc/$PID/status ]; then
        RSS=$(grep VmRSS /proc/$PID/status | awk '{print $2}')
        VMS=$(grep VmSize /proc/$PID/status | awk '{print $2}')
        echo "$ELAPSED,$((RSS/1024)),$((VMS/1024))" >> "$OUTPUT"
    fi
    sleep 0.5
done
