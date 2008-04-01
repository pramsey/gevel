SET client_min_messages = warning;
\set ECHO none
\i gevel.sql
\set ECHO all
RESET client_min_messages;


CREATE TABLE gevelt ( t box );
\copy gevelt from 'data/rect.data'

CREATE INDEX gist_idx ON gevelt USING gist ( t );

--GiST
SELECT gist_stat('gist_idx');
SELECT gist_tree('gist_idx');
SELECT * FROM gist_print('gist_idx') as t(level int, valid bool, a box) where level=1;
