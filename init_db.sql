-- MedEvac schema reference.
-- The C++ server creates/migrates these tables and seeds the illustrative road graph
-- and resource records on first startup. This file is kept as the DBMS documentation
-- artifact for the semester project.
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS roads (
    source TEXT NOT NULL,
    target TEXT NOT NULL,
    travel_time_mins INTEGER NOT NULL CHECK (travel_time_mins > 0),
    PRIMARY KEY (source, target)
);

CREATE TABLE IF NOT EXISTS ambulances (
    id TEXT PRIMARY KEY,
    location TEXT NOT NULL,
    is_available INTEGER NOT NULL DEFAULT 1 CHECK (is_available IN (0, 1)),
    type TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS hospitals (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    location TEXT NOT NULL,
    specialties TEXT NOT NULL,
    available_beds INTEGER NOT NULL CHECK (available_beds >= 0),
    total_beds INTEGER NOT NULL DEFAULT 5 CHECK (total_beds >= available_beds)
);

CREATE TABLE IF NOT EXISTS emergencies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id TEXT NOT NULL UNIQUE,
    patient_name TEXT NOT NULL,
    severity INTEGER NOT NULL CHECK (severity BETWEEN 1 AND 4),
    specialty TEXT NOT NULL,
    location TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'PENDING'
        CHECK (status IN ('PENDING', 'ASSIGNED', 'CONFIRMED', 'RESOLVED')),
    assigned_ambulance TEXT,
    assigned_hospital TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (assigned_ambulance) REFERENCES ambulances(id),
    FOREIGN KEY (assigned_hospital) REFERENCES hospitals(id)
);

CREATE INDEX IF NOT EXISTS idx_emergencies_queue
    ON emergencies(status, severity, created_at);
