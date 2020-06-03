/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2015      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2019      Intel, Inc.  All rights reserved.
 * Copyright (c) 2020      Geoffroy Vallee. All rights reserved.
 * Copyright (c) 2020      Cisco Systems, Inc.  All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "prte_config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#include "src/util/daemon_init.h"
#include "constants.h"

#include <stdio.h>

int prte_daemon_init_callback(char *working_dir, int (*parent_fn)(pid_t))
{
#if defined(HAVE_FORK)
    pid_t pid;
    int fd;

    if ((pid = fork()) < 0) {
        return PRTE_ERROR;
    } else if (pid != 0) {
        /* parent goes bye-bye */
        int rc = 0;
        fprintf(stderr, "about to invoke partent_fn\n");
        if (NULL != parent_fn) {
            rc = parent_fn(pid);
        }
        exit(rc);
    }

    fprintf(stderr,"continuing in child\n");
    /* child continues */
#if defined(HAVE_SETSID)
    setsid();  /* become session leader */
#endif

    if (NULL != working_dir) {
        if (-1 == chdir(working_dir)) {  /* change working directory */
            return PRTE_ERR_FATAL;
	}
    }

    fprintf(stderr,"about to shut off stdin\n");
    /* connect input to /dev/null */
    fd = open("/dev/null", O_RDONLY);
    if (0 > fd) {
        return PRTE_ERR_FATAL;
    }
    dup2(fd, STDIN_FILENO);
    if(fd != STDIN_FILENO) {
        close(fd);
    }

    fprintf(stderr,"about to shut off stdout .. but not really\n");
    /* connect outputs to /dev/null */
#if 0
    fd = open("/dev/null", O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        /* just to be safe, make sure we aren't trying
         * to close stdout or stderr! since we dup'd both
         * of them to the same fd, we can't just close it
         * since one of the two would still be open and
         * someone could attempt to use it.
         */
        if(fd != STDOUT_FILENO && fd != STDERR_FILENO) {
           close(fd);
        }
    } else {
        return PRTE_ERR_FATAL;
    }
#endif

    fprintf(stderr,"returning from prte_daemon_init_callback\n");
    return PRTE_SUCCESS;

#else /* HAVE_FORK */
    fprintf(stderr,"well I don't think this is supported\n");
    return PRTE_ERR_NOT_SUPPORTED;
#endif
}
