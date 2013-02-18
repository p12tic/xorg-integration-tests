ACLOCAL_AMFLAGS = -Im4

SUBDIRS = . tests recordings

EXTRA_DIST = autogen.sh HACKING 99-ignore-virtual-devices.conf 99-tag-virtual-devices.rules

check-local:
	@export DATE=`date +%Y-%m-%d-%H:%M`; \
            mkdir -p $(top_builddir)/results/$$DATE; \
            ln -sfT $$DATE $(top_builddir)/results/latest
