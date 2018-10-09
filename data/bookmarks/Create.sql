CREATE TABLE IF NOT EXISTS bookmarks
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    parentid INTEGER DEFAULT NULL,
    title TEXT,
    uri TEXT,
    desc TEXT,
    app INTEGER,
    toolbar INTEGER,
    pos_panel INTEGER,
    pos_bar INTEGER,
    created DATE DEFAULT CURRENT_TIMESTAMP,
    last_visit DATE,
    visit_count INTEGER DEFAULT 0,
    nick TEXT,

    FOREIGN KEY(parentid) REFERENCES bookmarks(id) ON DELETE CASCADE
);
