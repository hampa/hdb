# Makefile.am for HDB 
lib_LTLIBRARIES = libhdb.la libnhdb.la

COMMON = ../include/hdb.h libcommon.c urlencode.c ../include/urlencode.h

noinst_HEADERS = ../include/urlencode.h

# light HDB 
libhdb_la_SOURCES = libhdb.c $(COMMON)
libhdb_la_LDFLAGS = -version-info @LIB_CURRENT@:@LIB_REVISION@:@LIB_AGE@ -ldb

# network HDB
libnhdb_la_SOURCES = libnhdb.c $(COMMON) 
libnhdb_la_LDFLAGS = -version-info @LIB_CURRENT@:@LIB_REVISION@:@LIB_AGE@ -ldb


