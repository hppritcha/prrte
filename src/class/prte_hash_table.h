/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2014-2015 Hewlett-Packard Development Company, LP.
 *                         All rights reserved.
 * Copyright (c) 2014-2015 Mellanox Technologies, Inc.
 *                         All rights reserved.
 * Copyright (c) 2014-2019 Intel, Inc.  All rights reserved.
 * Copyright (c) 2014-2016 Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2020      Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2021      Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

/** @file
 *
 *  A hash table that may be indexed with either fixed length
 *  (e.g. uint32_t/uint64_t) or arbitrary size binary key
 *  values. However, only one key type may be used in a given table
 *  concurrently.
 */

#ifndef PRTE_HASH_TABLE_H
#define PRTE_HASH_TABLE_H

#include "prte_config.h"

#include <stdint.h>
#include "src/include/types.h"
#include "src/class/prte_list.h"

BEGIN_C_DECLS

PRTE_EXPORT PRTE_CLASS_DECLARATION(prte_hash_table_t);

struct prte_hash_table_t
{
    prte_object_t        super;          /**< subclass of prte_object_t */
    struct prte_hash_element_t * ht_table;       /**< table of elements (opaque to users) */
    size_t               ht_capacity;    /**< allocated size (capacity) of table */
    size_t               ht_size;        /**< number of extant entries */
    size_t               ht_growth_trigger; /**< size hits this and table is grown  */
    int                  ht_density_numer, ht_density_denom; /**< max allowed density of table */
    int                  ht_growth_numer, ht_growth_denom;   /**< growth factor when grown  */
    const struct prte_hash_type_methods_t * ht_type_methods;
};
typedef struct prte_hash_table_t prte_hash_table_t;



/**
 *  Initializes the table size, must be called before using
 *  the table.
 *
 *  @param   table   The input hash table (IN).
 *  @param   size    The size of the table, which will be rounded up
 *                   (if required) to the next highest power of two (IN).
 *  @return  PRTE error code.
 *
 */

PRTE_EXPORT int prte_hash_table_init(prte_hash_table_t* ht, size_t table_size);

/* this could be the new init if people wanted a more general API */
PRTE_EXPORT int prte_hash_table_init2(prte_hash_table_t* ht, size_t estimated_max_size,
                                        int density_numer, int density_denom,
                                        int growth_numer, int growth_denom);

/**
 *  Returns the number of elements currently stored in the table.
 *
 *  @param   table   The input hash table (IN).
 *  @return  The number of elements in the table.
 *
 */

static inline size_t prte_hash_table_get_size(prte_hash_table_t *ht)
{
    return ht->ht_size;
}

/**
 *  Remove all elements from the table.
 *
 *  @param   table   The input hash table (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_remove_all(prte_hash_table_t *ht);

/**
 *  Retrieve value via uint32_t key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   ptr     The value associated with the key
 *  @return  integer return code:
 *           - PRTE_SUCCESS       if key was found
 *           - PRTE_ERR_NOT_FOUND if key was not found
 *           - PRTE_ERROR         other error
 *
 */

PRTE_EXPORT int prte_hash_table_get_value_uint32(prte_hash_table_t* table, uint32_t key,
                                                   void** ptr);

/**
 *  Set value based on uint32_t key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   value   The value to be associated with the key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_set_value_uint32(prte_hash_table_t* table, uint32_t key, void* value);

/**
 *  Remove value based on uint32_t key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_remove_value_uint32(prte_hash_table_t* table, uint32_t key);

/**
 *  Retrieve value via uint64_t key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   ptr     The value associated with the key
 *  @return  integer return code:
 *           - PRTE_SUCCESS       if key was found
 *           - PRTE_ERR_NOT_FOUND if key was not found
 *           - PRTE_ERROR         other error
 *
 */

PRTE_EXPORT int prte_hash_table_get_value_uint64(prte_hash_table_t *table, uint64_t key,
                                                   void **ptr);

/**
 *  Set value based on uint64_t key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   value   The value to be associated with the key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_set_value_uint64(prte_hash_table_t *table, uint64_t key, void* value);

/**
 *  Remove value based on uint64_t key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_remove_value_uint64(prte_hash_table_t *table, uint64_t key);

/**
 *  Retrieve value via arbitrary length binary key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   ptr     The value associated with the key
 *  @return  integer return code:
 *           - PRTE_SUCCESS       if key was found
 *           - PRTE_ERR_NOT_FOUND if key was not found
 *           - PRTE_ERROR         other error
 *
 */

PRTE_EXPORT int prte_hash_table_get_value_ptr(prte_hash_table_t *table, const void* key,
                                                size_t keylen, void **ptr);

/**
 *  Set value based on arbitrary length binary key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   value   The value to be associated with the key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_set_value_ptr(prte_hash_table_t *table, const void* key, size_t keylen, void* value);

/**
 *  Remove value based on arbitrary length binary key.
 *
 *  @param   table   The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_hash_table_remove_value_ptr(prte_hash_table_t *table, const void* key, size_t keylen);


/** The following functions are only for allowing iterating through
    the hash table. The calls return along with a key, a pointer to
    the hash node with the current key, so that subsequent calls do
    not have to traverse all over again to the key (although it may
    just be a simple thing - to go to the array element and then
    traverse through the individual list). But lets take out this
    inefficiency too. This is similar to having an STL iterator in
    functionality */

/**
 *  Get the first 32 bit key from the hash table, which can be used later to
 *  get the next key
 *  @param  table   The hash table pointer (IN)
 *  @param  key     The first key (OUT)
 *  @param  value   The value corresponding to this key (OUT)
 *  @param  node    The pointer to the hash table internal node which stores
 *                  the key-value pair (this is required for subsequent calls
 *                  to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_hash_table_get_first_key_uint32(prte_hash_table_t *table, uint32_t *key,
                                        void **value, void **node);


/**
 *  Get the next 32 bit key from the hash table, knowing the current key
 *  @param  table    The hash table pointer (IN)
 *  @param  key      The key (OUT)
 *  @param  value    The value corresponding to this key (OUT)
 *  @param  in_node  The node pointer from previous call to either get_first
                     or get_next (IN)
 *  @param  out_node The pointer to the hash table internal node which stores
 *                   the key-value pair (this is required for subsequent calls
 *                   to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_hash_table_get_next_key_uint32(prte_hash_table_t *table, uint32_t *key,
                                       void **value, void *in_node,
                                       void **out_node);


/**
 *  Get the first 64 key from the hash table, which can be used later to
 *  get the next key
 *  @param  table   The hash table pointer (IN)
 *  @param  key     The first key (OUT)
 *  @param  value   The value corresponding to this key (OUT)
 *  @param  node    The pointer to the hash table internal node which stores
 *                  the key-value pair (this is required for subsequent calls
 *                  to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_hash_table_get_first_key_uint64(prte_hash_table_t *table, uint64_t *key,
                                       void **value, void **node);


/**
 *  Get the next 64 bit key from the hash table, knowing the current key
 *  @param  table    The hash table pointer (IN)
 *  @param  key      The key (OUT)
 *  @param  value    The value corresponding to this key (OUT)
 *  @param  in_node  The node pointer from previous call to either get_first
                     or get_next (IN)
 *  @param  out_node The pointer to the hash table internal node which stores
 *                   the key-value pair (this is required for subsequent calls
 *                   to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_hash_table_get_next_key_uint64(prte_hash_table_t *table, uint64_t *key,
                                       void **value, void *in_node,
                                       void **out_node);


/**
 *  Get the first ptr bit key from the hash table, which can be used later to
 *  get the next key
 *  @param  table    The hash table pointer (IN)
 *  @param  key      The first key (OUT)
 *  @param  key_size The first key size (OUT)
 *  @param  value    The value corresponding to this key (OUT)
 *  @param  node     The pointer to the hash table internal node which stores
 *                   the key-value pair (this is required for subsequent calls
 *                   to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_hash_table_get_first_key_ptr(prte_hash_table_t *table, void* *key,
                                        size_t *key_size, void **value, void **node);


/**
 *  Get the next ptr bit key from the hash table, knowing the current key
 *  @param  table    The hash table pointer (IN)
 *  @param  key      The key (OUT)
 *  @param  key_size The key size (OUT)
 *  @param  value    The value corresponding to this key (OUT)
 *  @param  in_node  The node pointer from previous call to either get_first
                     or get_next (IN)
 *  @param  out_node The pointer to the hash table internal node which stores
 *                   the key-value pair (this is required for subsequent calls
 *                   to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_hash_table_get_next_key_ptr(prte_hash_table_t *table, void* *key,
                                       size_t *key_size, void **value,
                                       void *in_node, void **out_node);



PRTE_EXPORT PRTE_CLASS_DECLARATION(prte_proc_table_t);

struct prte_proc_table_t
{
    prte_hash_table_t    super;          /**< subclass of prte_object_t */
    size_t               pt_size;        /**< number of extant entries */
    size_t               vpids_size;
    // FIXME
    // Begin KLUDGE!!  So ompi/debuggers/ompi_common_dll.c doesn't complain
    size_t              pt_table_size;  /**< size of table */
    // End KLUDGE
};
typedef struct prte_proc_table_t prte_proc_table_t;



/**
 *  Initializes the table size, must be called before using
 *  the table.
 *
 *  @param   pt      The input hash table (IN).
 *  @param   jobids  The size of the jobids table, which will be rounded up
 *                   (if required) to the next highest power of two (IN).
 *  @param   vpids   The size of the vpids table, which will be rounded up
 *                   (if required) to the next highest power of two (IN).
 *  @return  PRTE error code.
 *
 */

PRTE_EXPORT int prte_proc_table_init(prte_proc_table_t* pt, size_t jobids, size_t vpids);

/**
 *  Remove all elements from the table.
 *
 *  @param   pt   The input hash table (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_proc_table_remove_all(prte_proc_table_t *pt);

/**
 *  Retrieve value via prte_process_name_t key.
 *
 *  @param   pt      The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   ptr     The value associated with the key
 *  @return  integer return code:
 *           - PRTE_SUCCESS       if key was found
 *           - PRTE_ERR_NOT_FOUND if key was not found
 *           - PRTE_ERROR         other error
 *
 */

PRTE_EXPORT int prte_proc_table_get_value(prte_proc_table_t* pt, prte_process_name_t key,
                                                   void** ptr);

/**
 *  Set value based on prte_process_name_t key.
 *
 *  @param   pt      The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @param   value   The value to be associated with the key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_proc_table_set_value(prte_proc_table_t* pt, prte_process_name_t key, void* value);

/**
 *  Remove value based on prte_process_name_t key.
 *
 *  @param   pt      The input hash table (IN).
 *  @param   key     The input key (IN).
 *  @return  PRTE return code.
 *
 */

PRTE_EXPORT int prte_proc_table_remove_value(prte_proc_table_t* pt, prte_process_name_t key);


/**
 *  Get the first prte_process_name_t key from the hash table, which can be used later to
 *  get the next key
 *  @param  pt      The hash table pointer (IN)
 *  @param  key     The first key (OUT)
 *  @param  value   The value corresponding to this key (OUT)
 *  @param  node1   The pointer to the first internal node which stores
 *                  the key-value pair (this is required for subsequent calls
 *                  to get_next_key) (OUT)
 *  @param  node2   The pointer to the second internal node which stores
 *                  the key-value pair (this is required for subsequent calls
 *                  to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_proc_table_get_first_key(prte_proc_table_t *pt, prte_process_name_t *key,
                                                void **value, void **node1, void **node2);


/**
 *  Get the next prte_process_name_t key from the hash table, knowing the current key
 *  @param  pt       The hash table pointer (IN)
 *  @param  key      The key (OUT)
 *  @param  value    The value corresponding to this key (OUT)
 *  @param  in_node1 The first node pointer from previous call to either get_first
                     or get_next (IN)
 *  @param  out_node1 The first pointer to the hash table internal node which stores
 *                   the key-value pair (this is required for subsequent calls
 *                   to get_next_key) (OUT)
 *  @param  in_node2 The second node pointer from previous call to either get_first
                     or get_next (IN)
 *  @param  out_node2 The second pointer to the hash table internal node which stores
 *                   the key-value pair (this is required for subsequent calls
 *                   to get_next_key) (OUT)
 *  @return PRTE error code
 *
 */

PRTE_EXPORT int prte_proc_table_get_next_key(prte_proc_table_t *pt, prte_process_name_t *key,
                                               void **value, void *in_node1, void **out_node1,
                                               void *in_node2, void **out_node2);

/**
 * Loop over a hash table.
 *
 * @param[in] key Key for each item
 * @param[in] type Type of key (ui32|ui64|ptr)
 * @param[in] value Storage for each item
 * @param[in] ht Hash table to iterate over
 *
 * This macro provides a simple way to loop over the items in an prte_hash_table_t. It
 * is not safe to call prte_hash_table_remove* from within the loop.
 *
 * Example Usage:
 *
 * uint64_t key;
 * void * value;
 * PRTE_HASH_TABLE_FOREACH(key, uint64, value, ht) {
 *    do_something(key, value);
 * }
 */
#define PRTE_HASH_TABLE_FOREACH(key, type, value, ht) \
  for (void *_nptr=NULL;                                   \
       PRTE_SUCCESS == prte_hash_table_get_next_key_##type(ht, &key, (void **)&value, _nptr, &_nptr);)

#define PRTE_HASH_TABLE_FOREACH_PTR(key, value, ht, body)        \
{                                    \
    size_t key_size_;                            \
    for (void *_nptr=NULL;                        \
         PRTE_SUCCESS == prte_hash_table_get_next_key_ptr (ht, &key, &key_size_, (void **)&value, _nptr, &_nptr);) \
    body                                \
}

END_C_DECLS

#endif  /* PRTE_HASH_TABLE_H */
