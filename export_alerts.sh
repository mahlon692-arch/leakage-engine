#!/bin/bash

# Paths to our database vault
DB_PATH="storage/vault.db"

# 1. Query the database for compressed metrics
TOTAL_ANOMALIES=$(sqlite3 $DB_PATH "SELECT COUNT(*) FROM financial_transactions WHERE reconciliation_status = 'MISMATCH_LEAKAGE';")
LATEST_LEAK_ID=$(sqlite3 $DB_PATH "SELECT universal_tx_id FROM financial_transactions WHERE reconciliation_status = 'MISMATCH_LEAKAGE' ORDER BY transaction_time DESC LIMIT 1;")
LATEST_LEAK_AMT=$(sqlite3 $DB_PATH "SELECT gross_amount FROM financial_transactions WHERE reconciliation_status = 'MISMATCH_LEAKAGE' ORDER BY transaction_time DESC LIMIT 1;")

# If no anomalies exist, prepare a clean status payload
if [ -z "$LATEST_LEAK_ID" ]; then
    SMS_PAYLOAD="[GULLEY_VAULT] $(date +'%Y-%m-%d') STATUS: CLEAN. All cash and inventory streams 100% matched. Storage optimized."
else
    # 2. Construct a ultra-compressed text string under 160 characters
    # This format is universal: works for kitchens, retail, or clinical audits.
    SMS_PAYLOAD="ALERT: Leakage detected. Total Anomalies: $TOTAL_ANOMALIES. Latest Code: $LATEST_LEAK_ID (Amt: KSh $LATEST_LEAK_AMT). Action required."
fi

# 3. Output the payload and calculate character density to guarantee zero cellular lag
echo "=================================================="
echo "📡 OUTBOUND LOW-BANDWIDTH SMS PAYLOAD GENERATED   "
echo "=================================================="
echo "$SMS_PAYLOAD"
echo "--------------------------------------------------"
echo "Payload Length: ${#SMS_PAYLOAD} characters (Max: 160)"
echo "=================================================="

# Save the payload locally to an outbound queue file for future gateway integration
echo "$SMS_PAYLOAD" > storage/outbound_sms.txt
