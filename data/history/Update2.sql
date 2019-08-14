/* Covering index for shortcuts */
CREATE UNIQUE INDEX speed_dial ON history (date, image, title, uri) WHERE image <> '';
