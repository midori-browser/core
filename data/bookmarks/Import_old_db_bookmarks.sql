INSERT INTO main.bookmarks (parentid, title, uri, desc, app, toolbar)
SELECT NULL AS parentid, title, uri, desc, app, toolbar 
FROM old_db.bookmarks;
UPDATE main.bookmarks SET parentid = (
SELECT id FROM main.bookmarks AS b1 WHERE b1.title = (
SELECT folder FROM old_db.bookmarks WHERE title = main.bookmarks.title));
