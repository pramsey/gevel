subdir = contrib/gevel
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

MODULES = gevel
DATA_built = gevel.sql
DOCS = README.gevel
#REGRESS = gevel

include $(top_srcdir)/contrib/contrib-global.mk
