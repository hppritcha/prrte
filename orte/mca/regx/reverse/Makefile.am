#
# Copyright (c) 2016-2019 Intel, Inc.  All rights reserved.
# Copyright (c) 2017      IBM Corporation.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
          regx_reverse_component.c \
          regx_reverse.h \
          regx_reverse.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_regx_reverse_DSO
component_noinst =
component_install = mca_regx_reverse.la
else
component_noinst = libmca_regx_reverse.la
component_install =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_regx_reverse_la_SOURCES = $(sources)
mca_regx_reverse_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_regx_reverse_la_SOURCES = $(sources)
libmca_regx_reverse_la_LDFLAGS = -module -avoid-version
