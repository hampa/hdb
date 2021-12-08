noinst_PROGRAMS = hdb-lite-tapir hdb-net-tapir 
EXTRA_DIST = hdb-tapir.c

AM_CFLAGS = -ggdb
SRC = hdb-tapir.c ../lib/libcommon.c ../lib/urlencode.c
LITE_SRC = ../lib/libhdb.c $(SRC)
NET_SRC = ../lib/libnhdb.c $(SRC)
hdb_lite_tapir_SOURCES = 
hdb_net_tapir_SOURCES = 

hdb-net-tapir : $(LITE_SRC)
	$(COMPILE) -o hdb-net-tapir -DHDB_NET $(NET_SRC) @CMDDFLAGS@

hdb-lite-tapir : $(LITE_SRC) 
	$(COMPILE) -ldb -o hdb-lite-tapir $(LITE_SRC) @CMDDFLAGS@
