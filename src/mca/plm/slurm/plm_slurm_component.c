/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2015      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2017-2019 Intel, Inc.  All rights reserved.
 * Copyright (c) 2019      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2020      Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2021-2025 Nanook Consulting  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "prte_config.h"
#include "constants.h"

#include "src/mca/base/pmix_mca_base_var.h"
#include "src/runtime/prte_globals.h"
#include "src/util/name_fns.h"
#include "src/util/pmix_environ.h"
#include "src/util/pmix_show_help.h"
#include "src/util/pmix_string_copy.h"

#include "plm_slurm.h"
#include "src/mca/plm/base/plm_private.h"
#include "src/mca/plm/plm.h"

/*
 * Public string showing the plm ompi_slurm component version number
 */
const char *prte_mca_plm_slurm_component_version_string
    = "PRTE slurm plm MCA component version " PRTE_VERSION;

/*
 * Local functions
 */
static int plm_slurm_register(void);
static int plm_slurm_open(void);
static int plm_slurm_close(void);
static int prte_mca_plm_slurm_component_query(pmix_mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

prte_mca_plm_slurm_component_t prte_mca_plm_slurm_component = {
    .super = {
        PRTE_PLM_BASE_VERSION_2_0_0,

        /* Component name and version */
        .pmix_mca_component_name = "slurm",
        PMIX_MCA_BASE_MAKE_VERSION(component,
                                   PRTE_MAJOR_VERSION,
                                   PRTE_MINOR_VERSION,
                                   PMIX_RELEASE_VERSION),

        /* Component open and close functions */
        .pmix_mca_open_component = plm_slurm_open,
        .pmix_mca_close_component = plm_slurm_close,
        .pmix_mca_query_component = prte_mca_plm_slurm_component_query,
        .pmix_mca_register_component_params = plm_slurm_register,
    }

    /* Other prte_mca_plm_slurm_component_t items -- left uninitialized
       here; will be initialized in plm_slurm_open() */
};
PMIX_MCA_BASE_COMPONENT_INIT(prte, plm, slurm)

static int plm_slurm_register(void)
{
    pmix_mca_base_component_t *comp = &prte_mca_plm_slurm_component.super;


    prte_mca_plm_slurm_component.custom_args = NULL;
    pmix_mca_base_component_var_register(comp, "args", "Custom arguments to srun",
                                         PMIX_MCA_BASE_VAR_TYPE_STRING,
                                         &prte_mca_plm_slurm_component.custom_args);

    return PRTE_SUCCESS;
}

static int plm_slurm_open(void)
{
    return PRTE_SUCCESS;
}

static int prte_mca_plm_slurm_component_query(pmix_mca_base_module_t **module, int *priority)
{
    FILE *fp;
    char version[1024], *ptr;

    /* Are we running under a SLURM job? */
    if (NULL != getenv("SLURM_JOBID")) {
        *priority = 75;

        PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                             "%s plm:slurm: available for selection",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));

        // check the version
        fp = popen("srun --version", "r");
        if (NULL == fp) {
            // cannot run srun, so we cannot support this job
            *module = NULL;
            return PRTE_ERROR;
        }
        if (NULL == fgets(version, sizeof(version), fp)) {
            pclose(fp);
            *module = NULL;
            return PRTE_ERROR;
        }
        pclose(fp);
        /* move cptr past the 1st space. if the line doesn't have a space, then ignore it */
        ptr = strchr(version, ' ');
        if (NULL == ptr) {
            // play it safe
            *module = NULL;
            return PRTE_ERROR;
        }
        ++ptr;
        // parse on the dots
        prte_mca_plm_slurm_component.major = strtol(ptr, &ptr, 10);
        ++ptr;
        prte_mca_plm_slurm_component.minor = strtol(ptr, NULL, 10);

        if (23 > prte_mca_plm_slurm_component.major) {
            prte_mca_plm_slurm_component.early = true;
        } else if (23 < prte_mca_plm_slurm_component.major) {
            prte_mca_plm_slurm_component.early = false;
        } else if (11 > prte_mca_plm_slurm_component.minor) {
            prte_mca_plm_slurm_component.early = true;
        } else {
            prte_mca_plm_slurm_component.early = false;
        }
        // check for ancient
        if (prte_mca_plm_slurm_component.major < 17) {
            prte_mca_plm_slurm_component.ancient = true;
        } else if (17 == prte_mca_plm_slurm_component.major &&
                   prte_mca_plm_slurm_component.minor < 11) {
            prte_mca_plm_slurm_component.ancient = true;
        } else {
            prte_mca_plm_slurm_component.ancient = false;
        }
        *module = (pmix_mca_base_module_t *) &prte_plm_slurm_module;
        return PRTE_SUCCESS;
    }

    /* Sadly, no */
    *module = NULL;
    return PRTE_ERROR;
}

static int plm_slurm_close(void)
{
    return PRTE_SUCCESS;
}
