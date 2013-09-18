CREATE TEMPORARY TABLE backup
(
    uri text,
    title text,
    date integer
);
INSERT INTO backup SELECT uri, title, date FROM history;
DROP TABLE history;
CREATE TABLE history (uri text, title text, date integer, day integer);
INSERT INTO history SELECT uri, title, date,
    julianday(date(date,'unixepoch','start of day','+1 day'))
    - julianday('0001-01-01','start of day')
    FROM backup;
DROP TABLE backup;
