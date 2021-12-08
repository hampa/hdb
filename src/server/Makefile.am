# Makefile.am for HDB 
bin_PROGRAMS = hdbd	
				
# src files
hdbd_SOURCES = phdbd.c ../lib/libhdb.c ../lib/urlencode.c ../lib/libcommon.c
hdbd_LDADD = -ldb -lpthread
hdbd_LDFLAGS =  @HDBD_CPPFLAGS@ @LIBDFLAGS@ -DNDEBUG

EXTRA_DIST = initd_hdb

# R for RPM_BUILD_ROOT or other build systems
install-data-hook:
	install -d $(R)/etc/init.d
	install -m 755 initd_hdb $(R)/etc/init.d/hdbd

