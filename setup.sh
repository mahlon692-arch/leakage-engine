#!/bin/bash
# Setup script for Leakage-engine with Safety Backup

echo "Initializing Leakage-engine..."

# 1. Create directory structure
mkdir -p core storage/backups scripts tests profiles

# 2. Backup Function: Save current state before new operations
backup_data() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    echo "Creating safety backup: backup_$timestamp.tar.gz"
    tar -czf storage/backups/backup_$timestamp.tar.gz storage/vault.db tests/*.csv 2>/dev/null
}

# 3. Perform backup if data exists
if [ -f "tests/mock_statement.csv" ]; then
    backup_data
fi

# 4. Permissions
chmod +x scripts/*.sh

# 5. Path persistence
if ! grep -q "ENGINE_PATH" ~/.bashrc; then
    echo "export ENGINE_PATH=$(pwd)" >> ~/.bashrc
fi

echo "Setup complete. Engine is protected and ready."
