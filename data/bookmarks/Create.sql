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

/* trigger: insert panel position */
CREATE TRIGGER IF NOT EXISTS bookmarkInsertPosPanel 
AFTER INSERT ON bookmarks FOR EACH ROW 
BEGIN UPDATE bookmarks SET pos_panel = (
SELECT ifnull(MAX(pos_panel),0)+1 FROM bookmarks WHERE 
(NEW.parentid IS NOT NULL AND parentid = NEW.parentid) 
OR (NEW.parentid IS NULL AND parentid IS NULL)) 
WHERE id = NEW.id; END;

/* trigger: insert Bookmarkbar position */
CREATE TRIGGER IF NOT EXISTS bookmarkInsertPosBar 
AFTER INSERT ON bookmarks FOR EACH ROW WHEN NEW.toolbar=1 
BEGIN UPDATE bookmarks SET pos_bar = (
SELECT ifnull(MAX(pos_bar),0)+1 FROM bookmarks WHERE 
((NEW.parentid IS NOT NULL AND parentid = NEW.parentid) 
OR (NEW.parentid IS NULL AND parentid IS NULL)) AND toolbar=1) 
WHERE id = NEW.id; END;

/* trigger: update panel position */
CREATE TRIGGER IF NOT EXISTS bookmarkUpdatePosPanel 
BEFORE UPDATE OF parentid ON bookmarks FOR EACH ROW 
WHEN ((NEW.parentid IS NULL OR OLD.parentid IS NULL) 
AND NEW.parentid IS NOT OLD.parentid) OR 
((NEW.parentid IS NOT NULL AND OLD.parentid IS NOT NULL) 
AND NEW.parentid!=OLD.parentid) 
BEGIN UPDATE bookmarks SET pos_panel = pos_panel-1 
WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) 
OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_panel > OLD.pos_panel; 
UPDATE bookmarks SET pos_panel = (
SELECT ifnull(MAX(pos_panel),0)+1 FROM bookmarks 
WHERE (NEW.parentid IS NOT NULL AND parentid = NEW.parentid) 
OR (NEW.parentid IS NULL AND parentid IS NULL)) 
WHERE id = OLD.id; END;

/* trigger: update Bookmarkbar position */
CREATE TRIGGER IF NOT EXISTS bookmarkUpdatePosBar0 
AFTER UPDATE OF parentid, toolbar ON bookmarks FOR EACH ROW 
WHEN ((NEW.parentid IS NULL OR OLD.parentid IS NULL) 
AND NEW.parentid IS NOT OLD.parentid) 
OR ((NEW.parentid IS NOT NULL AND OLD.parentid IS NOT NULL) 
AND NEW.parentid!=OLD.parentid) OR (OLD.toolbar=1 AND NEW.toolbar=0) 
BEGIN UPDATE bookmarks SET pos_bar = NULL WHERE id = NEW.id; 
UPDATE bookmarks SET pos_bar = pos_bar-1 
WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) 
OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_bar > OLD.pos_bar; END;

/* trigger: update Bookmarkbar position */
CREATE TRIGGER IF NOT EXISTS bookmarkUpdatePosBar1 
BEFORE UPDATE OF parentid, toolbar ON bookmarks FOR EACH ROW 
WHEN ((NEW.parentid IS NULL OR OLD.parentid IS NULL) 
AND NEW.parentid IS NOT OLD.parentid) OR 
((NEW.parentid IS NOT NULL AND OLD.parentid IS NOT NULL) 
AND NEW.parentid!=OLD.parentid) OR (OLD.toolbar=0 AND NEW.toolbar=1) 
BEGIN UPDATE bookmarks SET pos_bar = (
SELECT ifnull(MAX(pos_bar),0)+1 FROM bookmarks WHERE 
(NEW.parentid IS NOT NULL AND parentid = NEW.parentid) 
OR (NEW.parentid IS NULL AND parentid IS NULL)) 
WHERE id = OLD.id; END;

/* trigger: delete panel position */
CREATE TRIGGER IF NOT EXISTS bookmarkDeletePosPanel 
AFTER DELETE ON bookmarks FOR EACH ROW 
BEGIN UPDATE bookmarks SET pos_panel = pos_panel-1 
WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) 
OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_panel > OLD.pos_panel; END;

/* trigger: delete Bookmarkbar position */
CREATE TRIGGER IF NOT EXISTS bookmarkDeletePosBar 
AFTER DELETE ON bookmarks FOR EACH ROW WHEN OLD.toolbar=1 
BEGIN UPDATE bookmarks SET pos_bar = pos_bar-1 
WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) 
OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_bar > OLD.pos_bar; END;
