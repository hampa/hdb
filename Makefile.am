# The top-level input Makefile for HDB 

# require automake 1.4
AUTOMAKE_OPTIONS = 1.4

SUBDIRS = src $(perl_dirs)

## Any directories you want a part of the distribution should be listed
## here, as well as have a Makefile generated at the end of configure.in
##
## This only works for subdirectories one level deep.
DIST_SUBDIRS = $(SUBDIRS)

# All the rest of the distributed files
EXTRA_DIST =		\
	BUGS		\
	TODO		\
	COPYING		\
	CREDITS		\
	INSTALL		\
	README		\
	hdb.spec	\
	hdb-lite.spec	\
	autogen.sh


# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpm -ta $(PACKAGE)-$(VERSION).tar.gz


# Rule to install the libraries only - prevent rebuilding apps
install-lib:
	cd src && $(MAKE) install-libLTLIBRARIES

# Run ldconfig after installing the library:
install-hook:
	-ldconfig

