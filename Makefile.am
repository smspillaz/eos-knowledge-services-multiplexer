# Copyright (C) 2016-2018 Endless Mobile, Inc.
#
# Note that this file includes Makefile.am.dbus-service.inc for the DBus Service
# and Makefile.am.multi-services.inc for the multi-service dispatcher shim.
#
# We define all automake variables first and then include the parts of the
# library that we are building.

BUILT_SOURCES =
CLEANFILES =
MAINTAINERCLEANFILES = $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL)
EXTRA_DIST =

# # # SUBSTITUTED FILES # # #
# These files need to be filled in with make variables
do_subst = $(SED) -e 's|%bindir%|$(bindir)|g'
subst_files = \
	data/com.endlessm.EknServices.SearchProviderV1.service \
	data/com.endlessm.EknServices2.SearchProviderV2.service \
	$(NULL)

$(subst_files): %: %.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(@D) && \
	$(do_subst) $< > $@

CLEANFILES += $(subst_files)
EXTRA_DIST += $(patsubst %,%.in,$(subst_files))

bin_PROGRAMS = eks-multi-services-dispatcher
eks_multi_services_dispatcher_SOURCES = \
	multi-services/eks-multi-services-dispatcher.c \
	$(NULL)
eks_multi_services_dispatcher_CFLAGS = \
	@MULTI_SERVICES_CFLAGS@ \
	-I $(builddir)/multi-services \
	$(AM_CFLAGS) \
	$(NULL)
eks_multi_services_dispatcher_LDADD = \
	@MULTI_SERVICES_LIBS@ \
	$(NULL)

servicedir = $(datadir)/dbus-1/services
service_DATA = \
	data/com.endlessm.EknServices.SearchProviderV1.service \
	data/com.endlessm.EknServices2.SearchProviderV2.service \
	$(NULL)


-include $(top_srcdir)/git.mk

