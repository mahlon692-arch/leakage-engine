#!/bin/bash
# 1. Compile and execute the native core C engine
gcc core/engine.c -o core/engine -lsqlite3
./core/engine

# 2. Run the low-bandwidth alert compression layer
./export_alerts.sh
