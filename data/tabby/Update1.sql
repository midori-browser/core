ALTER TABLE tabs ADD sorting REAL DEFAULT 0;

CREATE INDEX sorting on tabs (sorting ASC);
CREATE INDEX tstamp on tabs (tstamp ASC);
