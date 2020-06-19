SET client_min_messages = warning;
\set ECHO none
\i gevel.brin.sql
\set ECHO all
RESET client_min_messages;

CREATE TABLE gevelb ( t box );
\copy gevelb from 'data/rect.data'

--BRIN
CREATE INDEX brin_idx ON gevelb USING brin ( t );

SELECT brin_stat('brin_idx');
SELECT brin_print('brin_idx');
