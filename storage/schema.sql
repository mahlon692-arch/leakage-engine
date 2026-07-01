-- 1. UNIVERSAL ENTITIES (What are we tracking?)
CREATE TABLE IF NOT EXISTS entities (
    entity_id INTEGER PRIMARY KEY AUTOINCREMENT,
    external_sku TEXT UNIQUE,        -- The merchant's own barcode, SKU, or item code
    name TEXT NOT NULL,              -- e.g., "Minced Beef", "Insulin Vial", "Bale 40"
    category TEXT,                   -- General grouping for sector intelligence
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. INVENTORY STATES (Where is it, and what condition is it in?)
CREATE TABLE IF NOT EXISTS entity_states (
    state_id INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_id INTEGER,
    quantity REAL NOT NULL,          -- Real number to handle kilograms, liters, or pieces
    state_type TEXT NOT NULL,        -- 'AVAILABLE', 'IN_TRANSIT', 'WASTED', 'RESERVED'
    location_tag TEXT,               -- 'Shelf A', 'Cold Room', 'Store Room'
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(entity_id) REFERENCES entities(entity_id)
);

-- 3. FINANCIAL INFLOWS & RECONCILIATION (The Universal Ledger)
CREATE TABLE IF NOT EXISTS financial_transactions (
    transaction_id INTEGER PRIMARY KEY AUTOINCREMENT,
    universal_tx_id TEXT UNIQUE,     -- Normalized ID (e.g., M-Pesa code, POS receipt number)
    source_channel TEXT NOT NULL,    -- 'MPESA_TILL', 'CASH', 'JUMIA_FOOD', 'INSURANCE'
    gross_amount REAL NOT NULL,      -- The total money paid
    reported_fees REAL DEFAULT 0.0,  -- Commissions/fees charged by the platform
    expected_fees REAL DEFAULT 0.0,  -- What the fee *should* be based on our rule profiles
    transaction_time DATETIME,
    reconciliation_status TEXT DEFAULT 'UNRECONCILED', -- 'MATCHED', 'MISMATCH_LEAKAGE'
    shift_id INTEGER
);

-- 4. THE SHIFT HANDOVER AUDIT TRAIL (The Trust Layer)
CREATE TABLE IF NOT EXISTS shifts (
    shift_id INTEGER PRIMARY KEY AUTOINCREMENT,
    attendant_name TEXT NOT NULL,
    start_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    end_time DATETIME,
    expected_cash REAL DEFAULT 0.0,  -- Calculated by the engine via recorded sales
    actual_cash_reported REAL,       -- Counted physically by the worker
    variance REAL DEFAULT 0.0        -- The missing cash gap (Expected - Actual)
);
