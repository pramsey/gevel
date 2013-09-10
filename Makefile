subdir = contrib/gevel
top_builddir = ../..

MODULES = gevel
DATA_built = gevel.sql
DOCS = README.gevel
REGRESS = gevel

EXTRA_CLEAN =  pg_version.txt expected/gevel.out

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gevel
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

all installcheck: pg_version.txt

pg_version.txt:
	echo PG_MAJORVERSION | $(CPP) -undef -x c -w  -P $(CPPFLAGS) -include pg_config.h -o - - |  grep -v '^$$' | sed -e 's/"//g' > $@
	if [ -f expected/gevel.out.`cat pg_version.txt` ] ; \
	then \
		cp $(top_srcdir)/$(subdir)/expected/gevel.out.`cat pg_version.txt` expected/gevel.out ; \
	else \
		cp $(top_srcdir)/$(subdir)/expected/gevel.out.st expected/gevel.out ; \
	fi
