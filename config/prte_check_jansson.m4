# -*- autoconf -*-
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2006 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2006      QLogic Corp. All rights reserved.
# Copyright (c) 2009-2016 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2016-2020 Intel, Inc.  All rights reserved.
# Copyright (c) 2015      Research Organization for Information Science
#                         and Technology (RIST). All rights reserved.
# Copyright (c) 2016      Los Alamos National Security, LLC. All rights
#                         reserved.
# Copyright (c) 2021      Nanook Consulting.  All rights reserved.
# Copyright (c) 2022      Amazon.com, Inc. or its affiliates.
#                         All Rights reserved.
# Copyright (c) 2025-2026 Triad National Security, LLC. All rights
#                         reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# PRTE_CHECK_FLUX(prefix, [action-if-found], [action-if-not-found])
# --------------------------------------------------------
AC_DEFUN([PRTE_CHECK_JANSSON],[
    PRTE_VAR_SCOPE_PUSH([prte_check_jansson_save_CPPFLAGS])

    AC_ARG_WITH([jansson],
                [AS_HELP_STRING([--with-jansson(=DIR)],
                   [Build jansson support adding DIR/include, DIR/lib, and DIR/lib64 to the search path for headers and libraries])])
    AC_ARG_WITH([jansson-libdir],
            [AS_HELP_STRING([--with-jansson-libdir=DIR],
                    [Search for Jansson libraries in DIR])])

    OAC_CHECK_PACKAGE([jansson],
                      [$1],
                      [jansson.h],
                      [jansson],
                      [json_loads],
                      [prte_check_jansson_happy="yes"],
                      [prte_check_jansson_happy="no"])

    if test "$prte_check_jansson_happy" = "yes"; then
        AC_MSG_CHECKING([if libjansson version is 2.11 or greater])
        prte_check_jansson_save_CPPFLAGS="${CPPFLAGS}"
        PRTE_FLAGS_APPEND_UNIQ([CPPFLAGS], [${prte_check_jansson_CPPFLAGS}])
        AC_COMPILE_IFELSE(
              [AC_LANG_PROGRAM([[#include <jansson.h>]],
              [[
        #if JANSSON_VERSION_HEX < 0x00020b00
        #error "jansson API version is less than 2.11"
        #endif
                  ]])],
              [AC_MSG_RESULT([yes])],
              [AC_MSG_RESULT([no])
               prte_check_jansson_happy=no])
    	CPPFLAGS="$prte_check_jansson_save_CPPFLAGS"
    fi

    AS_IF([test "$prte_check_jansson_happy" = "no" -a "$with_jansson" != "no"],
          [AC_MSG_ERROR([Jansson support requested but not found.  Aborting])])

    AC_MSG_CHECKING([Jansson support available])
    AC_MSG_RESULT([$prte_check_jansson_happy])

    AS_IF([test "$prte_check_jansson_happy" = "yes"],
          [$2],
          [$3])


    AM_CONDITIONAL([HAVE_JANSSON], [test "$prte_check_jansson_happy" = "yes"])

    PRTE_SUMMARY_ADD([External Packages], [Jansson], [], [${prte_check_jansson_happy}])

    PRTE_VAR_SCOPE_POP
])
