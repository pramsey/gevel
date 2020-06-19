SET search_path = public;
BEGIN;

create or replace function btree_stat(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;

create or replace function btree_print(text)
        returns setof record
        as '$libdir/gevel'
        language C
        strict;
        
create or replace function btree_tree(text)
        returns text
        as '$libdir/gevel'
        language C
        strict;
        
create or replace function btree_tree(text, int)
        returns text
        as '$libdir/gevel'
        language C
        strict;

END;
