# contrib/gevel/Makefile

MODULE_big = gevel
OBJS = gevel.o
REGRESS = gevel
DATA = gevel.sql
DOCS = README.gevel
EXTRA_CLEAN =  pg_version.txt expected/gevel.out

all installcheck: pg_version.txt

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

VERSION = $(MAJORVERSION)
ifeq ($(VERSION),12)
	REGRESS += gevel_btree gevel_brin
	DATA += gevel.btree.sql gevel.brin.sql
endif


pg_version.txt:
	echo PG_MAJORVERSION | $(CPP) -undef -x c -w  -P $(CPPFLAGS) -include pg_config.h -o - - |  grep -v '^$$' | sed -e 's/"//g' > $@
	if [ -f expected/gevel.out.$(VERSION)`cat pg_version.txt` ] ; \
	then \
		cp expected/gevel.out.$(VERSION)`cat pg_version.txt` expected/gevel.out ; \
	else \
		cp expected/gevel.out.$(VERSION) expected/gevel.out ; \
	fi
