SET client_min_messages = warning;
\set ECHO none
\i gevel.btree.sql
\set ECHO all
RESET client_min_messages;

CREATE TABLE test__val( a int[] );
\copy test__val from 'data/test__int.data'
INSERT INTO test__val ( SELECT ARRAY[t] || '{1000}'::_int4 FROM generate_series (1,300) as t );
INSERT INTO test__val ( SELECT ARRAY[t] || '{1001}'::_int4 FROM generate_series (1,300) as t, generate_series(1,12) );
VACUUM ANALYZE test__val;

--Btree
CREATE INDEX btree_idx ON test__val USING btree ( a );

SELECT btree_stat('btree_idx');
SELECT btree_tree('btree_idx');
SELECT * FROM btree_print('btree_idx') as t(level int, valid bool, a int[]) where level=1;
