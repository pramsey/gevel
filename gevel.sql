SET search_path = public;
BEGIN;

create or replace function gist_tree(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;

create or replace function gist_tree(text,int4)
        returns text
        as '$libdir/gevel'
        language C
        strict;

create or replace function gist_stat(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;

create or replace function gist_print(text)
        returns setof record
        as '$libdir/gevel'
        language C
        strict;

create or replace function gin_stat(text)
        returns setof record
        as '$libdir/gevel'
        language C
        strict;

create or replace function gin_stat(text, int)
        returns setof record
        as '$libdir/gevel'
        language C
        strict;

create or replace function gin_statpage(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;

create or replace function gin_count_estimate(text, tsquery)
        returns bigint 
        as '$libdir/gevel'
        language C
        strict;

create or replace function spgist_stat(text)
        returns text 
        as '$libdir/gevel'
        language C
        strict;

create or replace function spgist_print(text)
        returns setof record
        as '$libdir/gevel'
        language C
        strict;

END;
