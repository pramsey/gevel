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

CREATE TABLE test__int( a int[] );
\copy test__int from 'data/test__int.data'

CREATE INDEX gin_idx ON test__int USING gin ( a );

INSERT INTO test__int ( SELECT ARRAY[t] || '{1000}'::_int4 FROM generate_series (1,300) as t );
INSERT INTO test__int ( SELECT ARRAY[t] || '{1001}'::_int4 FROM generate_series (1,300) as t, generate_series(1,12) );
VACUUM ANALYZE test__int; 
SELECT * FROM gin_stat('gin_idx') as t(value int, nrow int);
