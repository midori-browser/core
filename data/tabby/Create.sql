CREATE TABLE sessions
(
    id INTEGER PRIMARY KEY,
    tstamp INTEGER,
    title TEXT
);

CREATE TABLE tabs
(
    id INTEGER PRIMARY KEY,
    session_id INTEGER,
    uri TEXT,
    icon TEXT,
    title TEXT,
    crdate INTEGER,
    tstamp INTEGER,
    closed INTEGER,
    FOREIGN KEY(session_id) REFERENCES sessions(id)
);

CREATE TABLE tab_history
(
    id INTEGER PRIMARY KEY,
    tab_id INTEGER,
    url TEXT,
    icon TEXT,
    title TEXT,
    FOREIGN KEY(tab_id) REFERENCES tabs(id)
);
