#ifndef __NODE_SPARSE_HASHTABLE_H
#define __NODE_SPARSE_HASHTABLE_H

#include <stddef.h>
#include <stdlib.h>

/* Structure manager */
typedef struct node_hashtable_struct node_hashtable_t;

/* Iterator <key,entry> return values */
typedef struct node_hashtable_tuple_struct
{
    void *key, *entry;
} node_hashtable_tuple_t;

/* ------------------------ Main routines ------------------------ */

/* **** ht_flat_create ****
 * @ Input arguments:
 *        - size_t hashtable_size      : The initial hashtable size
 *        - size_t entry_sz            : The size of the entry
 *        - size_t hashtable_size      : The size of the key
 *        - int error_code             : The error code, in case of failure
 * @ Return value:
 *        - flat_hashtable_t *hashtable     : The hashtable structure manager
 * @ Description:
 *
 * Main constructor routine that creates a hashtable. The hashtable stores
 * the references (pointers) to the key and the values, so the user has
 * to allocate them externally.
 *
 * Due to that, they have to provide the function pointers that manipulate
 * the data of the of the keys-entries:
 *  - Comparator has to return 0 when there is not a match between keys and
 * any other number when there is.
 *  - Hasher returns the length of the key in order to calculate the hash.
 *  - Destruct is the the destructor routine and user provides
 *  entry and key in order to be freed.
 *
 * Hashtable may not be the exact size as the user requested and may allocate extra memory
 * for internal reasons.
 */
node_hashtable_t *ht_node_create(size_t hashtable_sz,
                                 int (*comp)(const void *, const void *),
                                 void (*destruct)(void *, void *),
                                 size_t (*hash)(const void *), int *error_code);

/* **** ht_flats_free ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 * @ Return value: None
 * @ Description:
 *
 * Main destructor routine that frees the hashtable entries and the hashtable itself.
 */
void ht_node_free(node_hashtable_t *hashtable);

/* **** ht_flat_search ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key        : The key to be searched
 * @ Return value:
 *        - void *entry            : Pointer to entry or NULL
 * @ Description:
 *
 * Main lookup routine that searches for an entry in the hashtable in the form of
 * <key, entry>.
 *
 * In failure it returns NULL, in success it returns a pointer to the entry
 * inside the hashtable.
 * User is responsible in case of wrong write/read on the entry.
 *
 */
void *ht_node_search(node_hashtable_t *hashtable, const void *key);

/* **** ht_flat_insert ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key        : The key to be inserted
 *        - const void *entry      : The entry associated with the key
 *        - int *error_code        : Error code for the status of the operation
 * @ Return value:
 *        - void *old_entry        : The old entry (NULL if it was empty)
 * @ Description:
 *
 * Main insertion routine that inserts an entry in the hashtable in the form of
 * <key, entry>.
 *
 * This routine will insert an entry only if the key does not exist in the table
 * where the routine return NULL for success, otherwise it returns the existing entry.
 * Error code should be checked (in case or rehashing failure).
 */
void *ht_node_insert(node_hashtable_t *hashtable, void *key, void *entry, int *error_code);

/* **** ht_flat_delete ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key        : The key to be inserted
 * @ Return value:
 *        - int  status            : Status of the operation
 * @ Description:
 *
 * Main insertion routine that deletes an entry from the hashtable.
 * */
int ht_node_delete(node_hashtable_t *hashtable, const void *key);

/* ------------------------ Utilities ------------------------ */
size_t ht_node_get_entries(node_hashtable_t *hashtable);
size_t ht_node_get_capacity(node_hashtable_t *hashtable);
double ht_node_get_load_factor(node_hashtable_t *hashtable);
void ht_node_print_mem_usage(node_hashtable_t *hashtable);

/* ------------------------ Iterators ------------------------ */

/* Given the specified hashtable start/end iterator routines
 * will find the first/last group in the hashtable and set the
 * tuple with the first entry in it.
 *
 * Next/Prev Iterate over the groups forward/backwards over the
 * GROUPS.
 *
 * When the return value is set to NULL then iteration stops, we
 * reached the end (either before the start of the table or
 * the after the end of the table).
 *
 * Iterators may not be valid after insertion or delete.
 * */
node_hashtable_tuple_t ht_node_start_it(node_hashtable_t *hashtable);
node_hashtable_tuple_t ht_node_next_it(node_hashtable_t *hashtable);
node_hashtable_tuple_t ht_node_prev_it(node_hashtable_t *hashtable);
node_hashtable_tuple_t ht_node_end_it(node_hashtable_t *hashtable);

#endif   // __NODE_SPARSE_HASHTABLE_H //
