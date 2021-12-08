EXTRA_DIST = HDB.pm 
PERLDIR = $(R)/usr/lib/perl5/site_perl/5.8.5/i386-linux-thread-multi/

# R for RPM_BUILD_ROOT or other build system
install-data-hook:
	install -d $(PERLDIR)
	install -m 755 HDB.pm $(PERLDIR)


