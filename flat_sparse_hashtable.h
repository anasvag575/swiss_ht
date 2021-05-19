#ifndef __FLAT_SPARSE_HASHTABLE_H
#define __FLAT_SPARSE_HASHTABLE_H

#include <stddef.h>
#include <stdlib.h>

/* Define for external use */
typedef struct flat_hashtable_struct flat_hashtable_t;

/* Iterator <key,entry> return values */
typedef struct flat_hashtable_tuple_struct
{
    void *key;
    void *entry;
} flat_hashtable_tuple_t;

// clang-format off

/* Error codes meaning along with numbering - These are common
 * in every call of this library, that has either a direct or
 * indirect (by reference) return of an error (type of integer).
 *
 *       Name            || Value ||      Explanation
 * ******************************************************************************************
 * HASH_OK               ||   0   || Operation completed successfully
 * HASH_WRONG_ARGUMENT   ||   2   || Wrong input by the user (0 for integers - NULL for refs)
 * HASH_CREATE_MEM_ALLOC ||   3   || Memory allocation failed in the creation of a new table
 * HASH_REHASH_MEM_ALLOC ||   4   || Memory allocation failed in the rehashing operation
 * HASH_ENTRY_NOT_EXISTS ||   5   || Entry does not exist in the table (during delete operation)
 * ******************************************************************************************
 *
 * In case the hashtable is going to be used with only one native type key
 * (int , long.. ) or there is a need for faster operations. Check this below.
 *
 * Problem for now is that only one type of key can be used after
 * the definition.
 * A MACRO based approach would solve this, since the library written in
 * a pair of MACRO wrappers would unroll and define the proper types.
 *
 * PROS
 * - MACRO expanding would generate faster code and better memory management
 *    -> Compiler could optimize stores and moves along with comparisons
 *    -> Simplifies lots of the generalization we are using currently (memcpys and comp)
 * - It would actually require less lines of code to implement.
 * - Specialization could happen for different types (different hashers).
 *
 * CONS
 * - We generate as much code as per the different <key,entry> types and
 * different structures. This might increase binary size a lot for those who care.
 * - Macros are generally harder to debug/use and expand upon.
 *
 * Some other implementations:
 *
 * khash ->    https://github.com/attractivechaos/klib/blob/master/khash.h
 * A well tested and exceptional library for lightweight hashtables.
 * These are the fastest thing around that can compete with C++ templates,
 * also, benchmarks exist where they are compared against other implementations
 * and C++ existing libraries.
 *
 * ctl/ust ->  https://github.com/glouw/ctl/blob/master/ctl/ust.h
 * Also, a pretty good library, it is the equivalent of the STL in C++
 * but for C, so it contains other interesting containers too.
 *
 * So for our case you can overide the general case and use one type of key to
 * have a faster implementation.
 *
 * !!! From the definitions below specify only ONE value !!!
 *
 * Use HT_FLAT_xx where the xx is some base type of the following:
 *  - INT         <=> HT_FLAT_INT_KEY
 *  - LONG INT    <=> HT_FLAT_LONG_INT_KEY
 *  - UINT32      <=> HT_FLAT_UINT32_KEY
 *  - UINT64      <=> HT_FLAT_UINT64_KEY
 */

// Define like that here //
//#define HT_FLAT_INT_KEY

// clang-format on

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
 * Main constructor routine that creates a hashtable. The hashtable inserts
 * and stores the entries internally by coppying, so to uniquely differentiate
 * 2 entries they have to be copiable (at least to do it correctly)
 *
 * The size of the entries and the keys is considered if we consider them as
 * statically allocated, i.e :
 *    -> Consider mystruct {int a, double b} -> sizeof(mystruct) = 12
 *    -> Consider mystruct {int a, char *str} -> sizeof(mystruct) = 8
 * And inside the hashtable the entry will be the str pointer and not the string itself.
 *
 * Hashtable may not be the exact size as the user requested and may allocate extra memory
 * for internal reasons.
 */
flat_hashtable_t *ht_flat_create(size_t hashtable_sz, size_t entry_sz, size_t key_sz, int *error_code);

/* **** ht_flats_free ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 * @ Return value: None
 * @ Description:
 *
 * Main destructor routine that frees the hashtable entries and the hashtable itself.
 */
void ht_flat_free(flat_hashtable_t *hashtable);

/* **** ht_flat_search ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key             : The key to be searched
 * @ Return value:
 *        - void *entry                 : Pointer to entry or NULL
 * @ Description:
 *
 * Main lookup routine that searches for an entry in the hashtable in the form of
 * <key, entry>.
 *
 * In failure it returns NULL, in success it returns a pointer to the entry
 * inside the hashtable.
 * User is responsible in case of wrong write/read on the entry after retrieval.
 *
 * Return may not be valid after insertion or delete.
 */
void *ht_flat_search(flat_hashtable_t *hashtable, const void *key);

/* **** ht_flat_insert ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key             : The key to be inserted
 *        - const void *entry           : The entry associated with the key
 *        - int *error_code             : Error code for the status of the operation
 * @ Return value:
 *        - void *old_entry             : The old entry (NULL if it was empty)
 * @ Description:
 *
 * Main insertion routine that inserts an entry in the hashtable in the form of
 * <key, entry>.
 *
 * This routine will insert an entry only if the key does not exist in the table
 * where the routine return NULL for success, otherwise it returns the existing entry.
 * Error code should be checked (in case or rehashing failure).
 */
void *ht_flat_insert(flat_hashtable_t *hashtable, const void *key, const void *entry, int *error_code);

/* **** ht_flat_emplace ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key             : The key to be inserted
 *        - const void *entry           : The entry associated with the key
 * @ Return value:
 *        - void error_code             : Error code for the status of the operation
 * @ Description:
 *
 * Similar to insert().
 *
 * Main difference is that if the key already exists it replaces with the entry
 * in the table (or creates a new entry like insert()).
 * Error code should be checked (in case or rehashing failure).
 * */
int ht_flat_emplace(flat_hashtable_t *hashtable, const void *key, const void *entry);

/* **** ht_flat_delete ****
 * @ Input arguments:
 *        - flat_hashtable_t *hashtable : The hashtable structure manager
 *        - const void *key             : The key to be inserted
 * @ Return value:
 *        - int  status                 : Status of the operation
 * @ Description:
 *
 * Main insertion routine that deletes an entry from the hashtable.
 * */
int ht_flat_delete(flat_hashtable_t *hashtable, const void *key);

/* ------------------------ Utilities ------------------------ */

size_t ht_flat_get_entries(flat_hashtable_t *hashtable);
size_t ht_flat_get_capacity(flat_hashtable_t *hashtable);
double ht_flat_get_load_factor(flat_hashtable_t *hashtable);
void ht_flat_print_mem_usage(flat_hashtable_t *hashtable);

/* ------------------------ Iterators ------------------------ */

/* Given the specified hashtable start/end iterator routines
 * will find the first/last group in the hashtable and set the
 * tuple with the first entry in it.
 *
 * Next/Prev Iterate forward/backwards over the
 * GROUPS.
 *
 * When the return value is set to NULL then iteration stops, we
 * reached the end (either before the start of the table or
 * after the end of the table).
 *
 * Iterators may not be valid after insertion or delete.
 * */
flat_hashtable_tuple_t ht_flat_start_it(flat_hashtable_t *hashtable);
flat_hashtable_tuple_t ht_flat_next_it(flat_hashtable_t *hashtable);
flat_hashtable_tuple_t ht_flat_prev_it(flat_hashtable_t *hashtable);
flat_hashtable_tuple_t ht_flat_end_it(flat_hashtable_t *hashtable);

#endif   // __FLAT_SPARSE_HASHTABLE_H //
