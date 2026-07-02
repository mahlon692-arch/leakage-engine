#!/bin/bash
# Usage: ./ingest.sh <item_name> <qty> <amount>

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then
    echo "Error: Missing arguments."
    echo "Usage: ./ingest.sh <item> <quantity> <amount>"
    exit 1
fi

FILE="tests/mock_statement.csv"

# Ensure headers exist
if [ ! -f "$FILE" ]; then
    echo "Date,Item,Quantity,Amount" > "$FILE"
fi

# Append with basic validation (ensuring input isn't empty)
echo "$(date '+%Y-%m-%d'),$1,$2,$3" >> "$FILE"
echo "Log updated: $1 recorded at $(date '+%H:%M:%S')."
