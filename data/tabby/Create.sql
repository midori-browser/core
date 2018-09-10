CREATE TABLE IF NOT EXISTS sessions
(
    id INTEGER PRIMARY KEY,
    parent_id INTEGER DEFAULT 0,
    crdate INTEGER DEFAULT 0,
    tstamp INTEGER DEFAULT 0,
    closed INTEGER DEFAULT 0,
    title TEXT DEFAULT NULL,
    FOREIGN KEY(parent_id) REFERENCES sessions(id)
);

CREATE TABLE IF NOT EXISTS tabs
(
    id INTEGER PRIMARY KEY,
    session_id INTEGER NOT NULL,
    uri TEXT DEFAULT NULL,
    icon TEXT DEFAULT NULL,
    title TEXT DEFAULT NULL,
    crdate INTEGER DEFAULT 0,
    tstamp INTEGER DEFAULT 0,
    closed INTEGER DEFAULT 0,
    FOREIGN KEY(session_id) REFERENCES sessions(id)
);

CREATE TABLE IF NOT EXISTS tab_history
(
    id INTEGER PRIMARY KEY,
    tab_id INTEGER,
    url TEXT,
    icon TEXT,
    title TEXT,
    FOREIGN KEY(tab_id) REFERENCES tabs(id)
);
