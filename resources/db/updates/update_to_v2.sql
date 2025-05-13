-- Upgrade script from database version 1 to version 2
-- Adds VectorTableMasterRecord and VectorTableColumnConfiguration tables for hybrid storage.

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS VectorTableMasterRecord (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    original_vector_table_id INTEGER UNIQUE,      -- Maps to old vector_tables.id
    table_name TEXT NOT NULL UNIQUE,
    binary_data_filename TEXT NOT NULL UNIQUE,
    file_format_version INTEGER NOT NULL DEFAULT 1,
    data_schema_version INTEGER NOT NULL DEFAULT 1,
    row_count INTEGER NOT NULL DEFAULT 0,
    column_count INTEGER NOT NULL DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS VectorTableColumnConfiguration (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    master_record_id INTEGER NOT NULL,
    column_name TEXT NOT NULL,
    column_order INTEGER NOT NULL,          -- 0-indexed
    column_type TEXT NOT NULL,              -- e.g., "TEXT", "INSTRUCTION_ID", "TIMESET_ID", "PIN_STATE_ID"
    data_properties TEXT,                   -- JSON for additional pin properties etc.
    UNIQUE (master_record_id, column_name),
    UNIQUE (master_record_id, column_order),
    FOREIGN KEY (master_record_id) REFERENCES VectorTableMasterRecord(id) ON DELETE CASCADE
);

CREATE TRIGGER IF NOT EXISTS trigger_update_vector_table_master_record_updated_at
AFTER UPDATE ON VectorTableMasterRecord
FOR EACH ROW
BEGIN
    UPDATE VectorTableMasterRecord SET updated_at = CURRENT_TIMESTAMP WHERE id = OLD.id;
END;

-- Note: The actual update to the db_version table to version 2
-- will be handled by the C++ code in DatabaseManager::performSchemaUpgradeToV2
-- after this script is successfully executed.

COMMIT; 