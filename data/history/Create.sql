CREATE TABLE IF NOT EXISTS history
(
    uri text,
    title text,
    date integer,
    day integer
);
CREATE TABLE IF NOT EXISTS search
(
    keywords text,
    uri text,
    day integer
);
