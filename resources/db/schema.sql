CREATE TABLE "type_options" (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type_name TEXT NOT NULL UNIQUE
);

CREATE TABLE timeset_list (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timeset_name TEXT NOT NULL UNIQUE,
    period REAL NOT NULL
);

CREATE TABLE wave_options (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    wave_type TEXT NOT NULL UNIQUE
);

CREATE TABLE "pin_settings" (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pin_id INTEGER NOT NULL REFERENCES pin_list(id),
    channel_count INTEGER NOT NULL,
    station_bit_index INTEGER NOT NULL,
    station_number INTEGER NOT NULL
);

CREATE TABLE vector_table_group_values(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    vector_data_id INTEGER NOT NULL REFERENCES vector_table_data(id), 
    group_id INTEGER NOT NULL REFERENCES pin_groups(group_id), 
    group_level TEXT NOT NULL
);

CREATE UNIQUE INDEX idx_vector_group_unique
ON vector_table_group_values(vector_data_id, group_id);

CREATE TABLE pin_options(
    id INTEGER PRIMARY KEY NOT NULL, 
    pin_value TEXT
);

CREATE TABLE instruction_options(
    id INTEGER PRIMARY KEY, 
    instruction_value TEXT NOT NULL UNIQUE, 
    instruction_class INT
);

CREATE TABLE vector_tables(
    id INTEGER PRIMARY KEY, 
    table_name VARCHAR NOT NULL UNIQUE, 
    table_nav_note TEXT
);

CREATE TABLE pin_list(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    pin_name TEXT NOT NULL, 
    pin_note TEXT, 
    pin_nav_note TEXT
);

CREATE TABLE vector_table_data(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    table_id INTEGER NOT NULL REFERENCES vector_tables(id), 
    label TEXT, 
    instruction_id INTEGER NOT NULL REFERENCES instruction_options(id), 
    timeset_id INTEGER NOT NULL REFERENCES timeset_list(id), 
    capture TEXT, 
    ext TEXT, 
    comment TEXT, 
    sort_index INTEGER
);

CREATE TABLE timeset_settings(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    timeset_id INTEGER NOT NULL REFERENCES timeset_list(id), 
    pin_id INTEGER REFERENCES pin_list(id), 
    T1R REAL, 
    T1F REAL, 
    STBR REAL, 
    wave_id INTEGER REFERENCES wave_options(id)
);

CREATE TABLE vector_table_pin_values(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    vector_data_id NOT NULL REFERENCES vector_table_data(id), 
    vector_pin_id NOT NULL REFERENCES vector_table_pins(id), 
    pin_level INTEGER NOT NULL REFERENCES pin_options(id)
);

CREATE UNIQUE INDEX idx_vector_pin_unique
ON vector_table_pin_values(
    vector_data_id, 
    vector_pin_id
);

CREATE TABLE vector_table_pins(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    table_id INTEGER NOT NULL REFERENCES vector_tables(id), 
    pin_id INTEGER NOT NULL REFERENCES pin_list(id), 
    pin_channel_count INT NOT NULL DEFAULT 1, 
    pin_type INTEGER NOT NULL DEFAULT 3 REFERENCES type_options(id)
);

CREATE UNIQUE INDEX idx_table_pin_unique
ON vector_table_pins(
    table_id, 
    pin_id
);

CREATE TABLE pin_groups(
    group_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    table_id INTEGER NOT NULL REFERENCES vector_tables(id), 
    group_name TEXT NOT NULL UNIQUE, 
    group_channel_count INT NOT NULL DEFAULT 1, 
    group_type INTEGER NOT NULL DEFAULT 3 REFERENCES type_options(id)
);

CREATE TABLE pin_group_members(
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    group_id INTEGER NOT NULL REFERENCES pin_groups(group_id), 
    pin_id INTEGER NOT NULL REFERENCES pin_list(id), 
    sort_index INTEGER NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX idx_group_member_unique
ON pin_group_members(
    group_id, 
    pin_id
);

CREATE VIEW view_vector_table_data_display AS
SELECT
    vtd.id AS vector_data_id,
    vt.id AS table_id,
    vt.table_name,
    vtd.label,
    io.instruction_value,
    tl.timeset_name,
    vtd.capture,
    vtd.ext,
    vtd.comment,
    vtd.sort_index
FROM vector_table_data vtd
JOIN vector_tables vt ON vtd.table_id = vt.id
JOIN instruction_options io ON vtd.instruction_id = io.id
JOIN timeset_list tl ON vtd.timeset_id = tl.id;

CREATE VIEW view_vector_table_pins_display AS
SELECT
    vtp.id AS vector_pin_id,
    vt.table_name,
    pl.pin_name,
    pl.pin_note,
    vtp.pin_channel_count,
    topt.type_name AS pin_type_name
FROM vector_table_pins vtp
JOIN vector_tables vt ON vtp.table_id = vt.id
JOIN pin_list pl ON vtp.pin_id = pl.id
JOIN type_options topt ON vtp.pin_type = topt.id;

CREATE VIEW view_vector_table_pin_values_display AS
SELECT
    vtd.id AS vector_data_id,
    vt.table_name,
    pl.pin_name,
    po.pin_value AS pin_level_value
FROM vector_table_pin_values vtpv
JOIN vector_table_data vtd ON vtpv.vector_data_id = vtd.id
JOIN vector_tables vt ON vtd.table_id = vt.id
JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id
JOIN pin_list pl ON vtp.pin_id = pl.id
JOIN pin_options po ON vtpv.pin_level = po.id;

CREATE VIEW view_pin_groups_display AS
SELECT
    pg.group_id,
    vt.table_name,
    pg.group_name,
    pg.group_channel_count,
    topt.type_name AS group_type_name
FROM pin_groups pg
JOIN vector_tables vt ON pg.table_id = vt.id
JOIN type_options topt ON pg.group_type = topt.id;

CREATE VIEW view_pin_group_members_display AS
SELECT
    pg.group_name,
    pl.pin_name,
    pgm.sort_index
FROM pin_group_members pgm
JOIN pin_groups pg ON pgm.group_id = pg.group_id
JOIN pin_list pl ON pgm.pin_id = pl.id
ORDER BY pg.group_name, pgm.sort_index;

CREATE TABLE VectorTableMasterRecord (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    original_vector_table_id INTEGER UNIQUE,
    table_name TEXT NOT NULL UNIQUE,
    binary_data_filename TEXT NOT NULL UNIQUE,
    file_format_version INTEGER NOT NULL DEFAULT 1,
    data_schema_version INTEGER NOT NULL DEFAULT 1,
    row_count INTEGER NOT NULL DEFAULT 0,
    column_count INTEGER NOT NULL DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE VectorTableColumnConfiguration (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    master_record_id INTEGER NOT NULL,
    column_name TEXT NOT NULL,
    column_order INTEGER NOT NULL,
    column_type TEXT NOT NULL,
    data_properties TEXT,
    IsVisible BOOLEAN NOT NULL DEFAULT 1,
    UNIQUE (master_record_id, column_name),
    UNIQUE (master_record_id, column_order),
    FOREIGN KEY (master_record_id) REFERENCES VectorTableMasterRecord(id) ON DELETE CASCADE
);

CREATE TRIGGER trigger_update_vector_table_master_record_updated_at
AFTER UPDATE ON VectorTableMasterRecord
FOR EACH ROW
BEGIN
    UPDATE VectorTableMasterRecord SET updated_at = CURRENT_TIMESTAMP WHERE id = OLD.id;
END;

CREATE TABLE VectorTableRowIndex (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    master_record_id INTEGER NOT NULL,
    logical_row_number INTEGER NOT NULL,
    physical_offset INTEGER NOT NULL,
    physical_size INTEGER NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (master_record_id) REFERENCES VectorTableMasterRecord(id) ON DELETE CASCADE,
    UNIQUE (master_record_id, logical_row_number)
);