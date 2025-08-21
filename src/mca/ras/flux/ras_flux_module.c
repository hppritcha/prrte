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
 * Copyright (c) 2006-2020 Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2014-2019 Intel, Inc.  All rights reserved.
 * Copyright (c) 2016      IBM Corporation.  All rights reserved.
 * Copyright (c) 2019      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2021-2025 Nanook Consulting  All rights reserved.
 * Copyright (c) 2025-2026 Triad National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "prte_config.h"
#include "constants.h"
#include "types.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdio.h>

#include "src/util/pmix_net.h"
#include "src/util/pmix_argv.h"
#include "src/util/pmix_os_path.h"
#include "src/util/pmix_show_help.h"

#include "src/mca/errmgr/errmgr.h"
#include "src/runtime/prte_globals.h"
#include "src/util/name_fns.h"

#include "ras_flux.h"
#include "src/mca/ras/base/base.h"

/*
 * Local functions
 */
static int init(void);
static int allocate(prte_job_t *jdata, pmix_list_t *nodes);
static int finalize(void);
static void modify(prte_pmix_server_req_t *req);
static void deallocate(prte_job_t *jdata, prte_app_context_t *app);

/*
 * Global variable
 */
prte_ras_base_module_t prte_ras_flux_module = {
    .init = init,
    .allocate = allocate,
    .deallocate = deallocate,
    .modify = modify,
    .finalize = finalize
};

/* init the module */
static int init(void)
{
    return PRTE_SUCCESS;
}


static int parse_json_payload(json_t *root,  pmix_list_t *prte_nodelist)
{
    int i, version, ret = PRTE_SUCCESS;
    int num_hosts;
    char *error_str = NULL;
    json_t *R_lite = NULL;
    json_t *nodelist = NULL;
    json_t *scheduling = NULL;
    json_t *properties = NULL;
    json_error_t error;
    char **node_array = NULL;

    /*
     * unpack the json
    */
    if (json_unpack_ex (root, &error, 0,
                        "{s:i s?O s:{s:o s:o s?o}}",
                        "version", &version,
                        "scheduling", &scheduling,
                        "execution",
                          "R_lite", &R_lite,
                          "nodelist", &nodelist,
                          "properties", &properties) < 0) {

        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate: error unpacking json object",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    /*
     * we only know how to parse versions 1 of resource.R
     */
    if (version != 1) {
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate: unexpected resource.R version %d expected 1",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), version));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    /*
     *  get the number of nodes and node list for this flux job
     */

    json_t *j_exec = json_object_get(root, "execution");
    if (!json_is_object(j_exec)) {
        error_str = "'execution' is missing or not an object";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate: %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    json_t *j_nodelist = json_object_get(j_exec, "nodelist");
    if (!json_is_array(j_nodelist)) {
        error_str = "'nodelist' is missing or not an array";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate: %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    /*
     * only support single nodelist entry in the json
     */
    size_t nodelist_len = json_array_size(j_nodelist);
    if (1 != nodelist_len) {
        error_str = "got more than one string in the nodelist";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_SUPPORTED;
        goto err;
    }

    json_t *j_node = json_array_get(j_nodelist,0);
    if (!json_is_string(j_node)) {
        error_str = "nodelist element is not a string";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    if (0 != prte_ras_flux_hostlist_count(json_string_value(j_node), &num_hosts)) {
        error_str = "problem encountered parsing number of nodes";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    node_array = (char **)malloc((num_hosts) * sizeof(char *));
    if (NULL == node_array) {
        ret = PRTE_ERR_OUT_OF_RESOURCE;
        goto err;
    }

    if (0 != prte_ras_flux_process_hostlist(json_string_value(j_node), node_array, &num_hosts)) {
        error_str = "problem encountered parsing node list";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    /*
     * get number of "slots" per node from the core idset
     */

    json_t *j_rlite = json_object_get(j_exec, "R_lite");
    if (!json_is_array(j_rlite)) {
        error_str = "'R_lite' is missing or not an array";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    size_t rlite_len = json_array_size(j_rlite);
    if (1 != rlite_len) {
        error_str = "got more than one string in R_lite";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_SUPPORTED;
        goto err;
    }

    json_t *j_entry = json_array_get(j_rlite, 0);
    if (!json_is_object(j_entry)) {
        error_str = "R_lite element is not an object";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    json_t *j_children = json_object_get(j_entry, "children");
    if (!json_is_object(j_children)) {
        error_str = "'children' missing or not an object";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    json_t *j_core = json_object_get(j_children, "core");
    if (!json_is_string(j_core)) {
        error_str = "'children.core' missing or not a string";
        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:  %s",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), error_str));
        ret = PRTE_ERR_NOT_AVAILABLE;
        goto err;
    }

    const char *core = json_string_value(j_core);
    int slots_per_node =  prte_ras_flux_idset_count(core);

    for (i = 0; i < num_hosts; ++i) {
        prte_node_t *node;

        PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                             "%s ras:flux:allocate:discover: adding node %s (%d slots)",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), node_array[i], slots_per_node));

        node = PMIX_NEW(prte_node_t);
        if (NULL == node) {
            ret = PRTE_ERR_OUT_OF_RESOURCE;
            goto err;
        }
        node->name = strdup(node_array[i]);
        node->state = PRTE_NODE_STATE_UP;
        node->slots_inuse = 0;
        node->slots_max = 0;
        node->slots = slots_per_node;
        pmix_list_append(prte_nodelist, &node->super);
        /*
         * the entries returned from prte_ras_flux_process_hostlist need to be freed
         */
        free(node_array[i]);
        node_array[i] = NULL;
    }

err:
    if (NULL != node_array) {
        free(node_array);
    }
    return ret;
}

/**
 * Discover available (pre-allocated) nodes and report
 * them back to the caller.
 *
 */
static int allocate(prte_job_t *jdata, pmix_list_t *nodes)
{
    int ret = PRTE_SUCCESS;
    flux_t *h = NULL;
    flux_future_t *f = NULL;
    flux_error_t flux_error;
    json_error_t json_err;
    char *return_str=NULL;
    const char *flux_job_id=NULL;

    if ((jdata == NULL) || (nodes == NULL)) {
        return PRTE_ERR_BAD_PARAM;
    }

    h = flux_open_ex(NULL, 0, &flux_error);
    if(NULL == h) {
        pmix_show_help("help-ras-flux.txt", "flux-broker-not-found", 1, flux_error.text);
        ret = PRTE_ERR_NOT_FOUND;
        goto err;
    }

    /*
     * first get job id attribute from local broker 
     */

    flux_job_id = flux_attr_get (h, "jobid");
    if (NULL == flux_job_id) {
        ret = PRTE_ERR_NOT_FOUND;
        goto err;
    }

    PMIX_OUTPUT_VERBOSE((10, prte_ras_base_framework.framework_output,
                         "%s ras:flux:allocate: flux job id is %s", PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), flux_job_id));

    /*
     * not sure what good this does but here goes
     */
    prte_job_ident = strdup(flux_job_id);

    /*
     * Lookup the "resource.R" KVS key
     * see https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/spec_20.html
     */

    f = flux_kvs_lookup(h, NULL, 0, "resource.R");
    if (NULL == f) {
        int errno_l = errno;
        pmix_show_help("help-ras-flux.txt", "flux-kvs-lookup-failure", 1, strerror(errno_l));
        ret = PRTE_ERR_NOT_FOUND;
        goto err;
    }

    if(flux_kvs_lookup_get (f, (const char **)&return_str) < 0){
        int errno_l = errno;
        pmix_show_help("help-ras-flux.txt", "flux-kvs-lookup-get-failure", 1, strerror(errno_l));
        return PRTE_ERR_NOT_FOUND;
        goto err;
    }

    PMIX_OUTPUT_VERBOSE((10, prte_ras_base_framework.framework_output,
                         "%s ras:flux:allocate: flux_kvs_lookup_get returned %s", PRTE_NAME_PRINT(PRTE_PROC_MY_NAME),return_str));

    /*
     *  Parse the returned JSON payload
     */
    json_t *root = json_loads(return_str, JSON_DECODE_ANY, &json_err);
    if (NULL == root) {
        pmix_show_help("help-ras-flux.txt", "flux-json-parse-failure", 1, json_err.text);
        ret = PRTE_ERR_UNPACK_FAILURE;
        goto err;
    }

    ret = parse_json_payload(root, nodes);

err:
    if (NULL != root) {
        json_decref(root);
    }
    if (NULL != f) {
        flux_future_destroy(f);
        f = NULL;
    }
    if (NULL != h) {
        flux_close(h);
        h = NULL;
    }

    return ret;
}

/*
 * This method is not currently available using Flux
 */
static void deallocate(prte_job_t *jdata, prte_app_context_t *app)
{
    PRTE_HIDE_UNUSED_PARAMS(jdata, app);
    PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                         "%s ras:flux:deallocate: not implemented",
                         PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
}

/*
 * This method is not currently available using Flux
 */
static void modify(prte_pmix_server_req_t *req)
{
    PRTE_HIDE_UNUSED_PARAMS(req);
    PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                         "%s ras:flux:deallocate: not implemented",
                         PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
}


/*
 * There's really nothing to do here
 */
static int finalize(void)
{
    PMIX_OUTPUT_VERBOSE((1, prte_ras_base_framework.framework_output,
                         "%s ras:flux:finalize: success (nothing to do)",
                         PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
    return PRTE_SUCCESS;
}
