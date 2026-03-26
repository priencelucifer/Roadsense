DROP TABLE IF EXISTS users;
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

DROP TABLE IF EXISTS hazards;
CREATE TABLE hazards (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    lat REAL NOT NULL,
    lng REAL NOT NULL,
    type TEXT NOT NULL,
    severity REAL NOT NULL,
    location_name TEXT,
    geohash TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Initial Guwahati Data
INSERT INTO hazards (lat, lng, type, severity, location_name, geohash) VALUES 
(26.1445, 91.7362, 'Pothole', 8.5, 'Ganeshguri', 'whr0m'),
(26.1158, 91.7931, 'Severe Pothole', 9.2, 'Dispur', 'whr0q'),
(28.6139, 77.2090, 'Pothole', 9.5, 'Delhi CP', 'ttnfv');
