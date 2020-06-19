SET search_path = public;
BEGIN;
        
create or replace function brin_stat(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;
        
create or replace function brin_print(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;

END;
