# Makefile.am for HDB 
if WITHOUT_HDB_NETWORK
bin_PROGRAMS = 	lhdb lhdb-cut lhdb-get lhdb-set lhdb-query \
				lhdb-filter lhdb-find lhdb-del lhdb-import
else
bin_PROGRAMS = 	lhdb lhdb-cut lhdb-get lhdb-set lhdb-query \
				lhdb-filter lhdb-find lhdb-del lhdb-import \
				nhdb nhdb-cut nhdb-get nhdb-set nhdb-query \
				nhdb-filter nhdb-find nhdb-del nhdb-import
endif

# bash scripts
bin_SCRIPTS = hdb-dump hgrep hdb-test hdb-edit 

EXTRA_DIST = $(bin_SCRIPTS) hdb-completion.sh

NETDFLAGS = -DHDB_NET

# src files
lhdb_SOURCES = hdb.c
nhdb_SOURCES = hdb.c
lhdb_get_SOURCES = hdb-get.c
nhdb_get_SOURCES = hdb-get.c
lhdb_set_SOURCES = hdb-set.c
nhdb_set_SOURCES = hdb-set.c
lhdb_del_SOURCES = hdb-del.c
nhdb_del_SOURCES = hdb-del.c
lhdb_cut_SOURCES = hdb-cut.c
nhdb_cut_SOURCES = hdb-cut.c
lhdb_query_SOURCES = hdb-query.c
nhdb_query_SOURCES = hdb-query.c
lhdb_find_SOURCES = hdb-find.c
nhdb_find_SOURCES = hdb-find.c
lhdb_import_SOURCES = hdb-import.c
nhdb_import_SOURCES = hdb-import.c
lhdb_filter_SOURCES = hdb-filter.c
nhdb_filter_SOURCES = hdb-filter.c

# linked libs 
lhdb_LDADD = -lhdb
nhdb_LDADD = -lnhdb
lhdb_get_LDADD = -lhdb
nhdb_get_LDADD = -lnhdb
lhdb_set_LDADD = -lhdb
nhdb_set_LDADD = -lnhdb
lhdb_cut_LDADD = -lhdb
nhdb_cut_LDADD = -lnhdb
lhdb_del_LDADD = -lhdb
nhdb_del_LDADD = -lnhdb
lhdb_find_LDADD = -lhdb
nhdb_find_LDADD = -lnhdb
lhdb_query_LDADD = -lhdb
nhdb_query_LDADD = -lnhdb
lhdb_import_LDADD = -lhdb
nhdb_import_LDADD = -lnhdb
lhdb_filter_LDADD = -lhdb
nhdb_filter_LDADD = -lnhdb

# flags
lhdb_LDFLAGS = @CMDDFLAGS@
nhdb_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_get_LDFLAGS = @CMDDFLAGS@
nhdb_get_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_set_LDFLAGS = @CMDDFLAGS@
nhdb_set_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_cut_LDFLAGS = @CMDDFLAGS@
nhdb_cut_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_del_LDFLAGS = @CMDDFLAGS@
nhdb_del_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_query_LDFLAGS = @CMDDFLAGS@
nhdb_query_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_find_LDFLAGS = @CMDDFLAGS@
nhdb_find_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_import_LDFLAGS = @CMDDFLAGS@
nhdb_import_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@
lhdb_filter_LDFLAGS = @CMDDFLAGS@
nhdb_filter_LDFLAGS = $(NETDFLAGS) @CMDDFLAGS@

install-exec-hook :
	ln -fs @CMDPREFIX@ $(DESTDIR)$(bindir)/hdb
	ln -fs @CMDPREFIX@-import $(DESTDIR)$(bindir)/hdb-import
	ln -fs @CMDPREFIX@-filter $(DESTDIR)$(bindir)/hdb-filter
	ln -fs @CMDPREFIX@-query $(DESTDIR)$(bindir)/hdb-query
	ln -fs @CMDPREFIX@-find $(DESTDIR)$(bindir)/hdb-find
	ln -fs @CMDPREFIX@-del $(DESTDIR)$(bindir)/hdb-del
	ln -fs @CMDPREFIX@-set $(DESTDIR)$(bindir)/hdb-set
	ln -fs @CMDPREFIX@-get $(DESTDIR)$(bindir)/hdb-get
	ln -fs @CMDPREFIX@-cut $(DESTDIR)$(bindir)/hdb-cut

# R for RPM_BUILD_ROOT or other build system
install-data-hook:
	install -d $(R)/etc/profile.d
	install -m 755 hdb-completion.sh $(R)/etc/profile.d/

