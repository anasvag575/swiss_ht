/////////////////////////////////
// Header comment place holder //
/////////////////////////////////

/* Library inclusions */
#include "node_sparse_hashtable.h"

/* Dev level inclusions*/
#include "sparse_hashtable_common.h"

/* Debug Limiters */
//#define DEBUG_HASH_INSERT
//#define DEBUG_HASH_SEARCH
//#define DEBUG_HASH_DELETE

/******************************************** Private Structures/Defines ********************************************/

/* **** node_pair_t ****
 *
 * A bucket in the hashtable, used only for bookeeping
 * and organization purposes.
 */
typedef struct node_hashtable_pair
{
    void *key;
    void *entry;
} node_pair_t;

/* **** hashtable_struct ****
 *
 * The manager structure for a Swiss Hashtable. The struct is
 * typedef on the header file and remains private to the user, with only
 * access to header file.
 * This kinds of table holds references and not the entries themselves.
 */
struct node_hashtable_struct
{
    /* Size of the hashtable */
    size_t hashtable_sz;
    size_t group_num;

    /* The table that holds entries+keys */
    node_pair_t *table;

    /* The bitmap used for quick access */
    uint8_t *bitmap;

    /* Total entries in the map */
    size_t entries;

    /* Used for hashing - Randomizes hash for each run */
    size_t hash_seed;

    /* Iterator sub-structure */
    hashtable_iter_t iterator;

    /* Function pointers for the main operations */
    void (*destruct)(void *entry, void *key);
    int (*comp)(const void *key1, const void *key2);
    size_t (*hash)(const void *key);
};

/* Probing technique - Either one or the other */
#define SPARSE_LIN_PROBE
//#define SPARSE_QUAD_PROBE

/* Hash function related - If security is needed use Crypto */
#define USE_DEFAULT_HASH
//#define USE_CRYPTO_HASH

/**************************** Private function Prototypes ******************************/

/* Utility sub-routines */
static inline size_t _ht_node_hasher(const char *key, size_t hash_sz, uint64_t seed);

/* Sub-routine for iteration */
static int _ht_iter_valid_group(node_hashtable_t *hashtable, size_t *start_group, uint16_t *final_group_mask, short int direction);

/* Sub-routines for the main operations of the hashtable */
static void *_ht_node_search(node_hashtable_t *hashtable, const void *key);
static void *_ht_node_insert(node_hashtable_t *hashtable, void *key, void *entry, char flag);
static int _ht_node_delete(node_hashtable_t *hashtable, const void *key);
static int _ht_node_resize(node_hashtable_t *hashtable, size_t new_sz);

/************************************ Internal Routines ************************************/

/* Hasher wrapper used for the hashing of the keys */
static inline size_t _ht_node_hasher(const char *key, size_t hash_sz, uint64_t seed)
{
#if !defined(USE_CRYPTO_HASH)
    /* Default hasher */
    return hash_CityHash64WithSeed(key, hash_sz, seed);
#else
    uint64_t sd[2] = {seed, seed};

    /* Secure hasher - Cryptographic function */
    return siphash24(key, hash_sz, (const char*) sd);
#endif
}

/* Finds the next valid group in the hashtable - Forward or backward (1 or -1) */
static int _ht_iter_valid_group(node_hashtable_t *hashtable, size_t *start_group, uint16_t *final_group_mask, short int direction)
{

    while((*start_group) < hashtable->group_num)
    {
        uint8_t *bitmap_pos = &hashtable->bitmap[(*start_group) * GROUP_SIZE];
        uint16_t valid_entries_mask = ~(_ht_and_mask(bitmap_pos, HIGH_BIT_MASK));

        /* Break condition */
        if(valid_entries_mask)
        {
            *final_group_mask = valid_entries_mask;
            return ITER_VALID;
        }

        /* Probe to the next group */
        (*start_group) += direction;
    }

    return ITER_NOT_VALID;
}

/* The main lookup sub-routine */
static void *_ht_node_search(node_hashtable_t *hashtable, const void *key)
{
    const size_t group_mask = hashtable->group_num - 1; /* Now this becomes a mask */
    node_pair_t *table = hashtable->table;

    /* Find the hash and the metadata for the given key */
    const size_t hash = _ht_node_hasher(key, hashtable->hash(key), hashtable->hash_seed);
    const uint8_t bitmap_ctrl = hash & GROUP_H2_MASK;

    /* Simple power of 2 modding to find the group index */
    size_t group_idx = (hash >> GROUP_H1_SHIFT) & group_mask;
#ifndef SPARSE_LIN_PROBE
    size_t probe_step = 1;
#endif

    while(1)
    {
        /* Create an index for the matches */
        size_t i = group_idx * GROUP_SIZE;

        /* Create the new masks */
        uint8_t *bitmap_pos = &hashtable->bitmap[i];
        uint16_t eq_mask = _ht_eq_mask(bitmap_pos, bitmap_ctrl);
        uint16_t empty_mask = _ht_eq_mask(bitmap_pos, ENTRY_EMPTY);

        /* As long as there are matches */
        while(eq_mask)
        {
            size_t pos = _get_first_set_bit_pos(eq_mask);

            /* Found an empty position */
            if(HT_LIKELY(hashtable->comp(table[i + pos].key, key)))
                return table[i + pos].entry;

            /* Unset this entry */
            eq_mask ^= 1 << pos;
        }

        /* Search stop condition */
        if(HT_LIKELY(empty_mask))
            return NULL;

/* Probe */
#ifdef SPARSE_LIN_PROBE
        group_idx = (group_idx + 1) & group_mask;
#else
        group_idx = (group_idx + probe_step) & group_mask;
#endif
    }

    return NULL; /* Suppress compiler warnings */
}

/* The main insertion sub-routine. Performs different kinds of insertion based on action */
static void *_ht_node_insert(node_hashtable_t *hashtable, void *key, void *entry, char flag)
{
    /* Resizing = NO_SEARCH | Insert = SEARCH_NO_REPLACE */
    if(flag != NO_SEARCH)
    {
        void *ret = _ht_node_search(hashtable, key);
        if(ret)
            return ret;
    }

    /* Restart the search - Constants */
    const size_t group_mask = hashtable->group_num - 1; /* Now this is a bitmask */
    const size_t hash = _ht_node_hasher(key, hashtable->hash(key), hashtable->hash_seed);

    /* Initial conditions */
    node_pair_t *table = hashtable->table;
    size_t group_idx = (hash >> GROUP_H1_SHIFT) & group_mask;
#ifndef SPARSE_LIN_PROBE
    size_t probe_step = 1;
#endif

    while(1)
    {
        /* Create the new masks */
        uint8_t *bitmap_pos = &hashtable->bitmap[group_idx * GROUP_SIZE];
        uint16_t empty_or_del_mask = _ht_and_mask(bitmap_pos, HIGH_BIT_MASK);

        /* Found empty spot */
        if(empty_or_del_mask)
        {
            size_t pos = (group_idx * GROUP_SIZE) + _get_first_set_bit_pos(empty_or_del_mask);

            /* Insertion in the hashtable */
            table[pos].key = key;
            table[pos].entry = entry;
            hashtable->bitmap[pos] = hash & (GROUP_H2_MASK);

            /* Fix the bitmap and toggle the first bit afterwards */
            hashtable->entries++;

            return NULL;
        }

/* Probe */
#ifdef SPARSE_LIN_PROBE
        group_idx = (group_idx + 1) & group_mask;
#else
        group_idx = (group_idx + probe_step) & group_mask;
#endif
    }

    return NULL; /* Suppress compiler warnings */
}

/* The main delete sub-routine */
static int _ht_node_delete(node_hashtable_t *hashtable, const void *key)
{
    /* Constants */
    const size_t group_mask = hashtable->group_num - 1; /* Now this becomes a mask */
    const size_t hash = _ht_node_hasher(key, hashtable->hash(key), hashtable->hash_seed);
    const uint8_t bitmap_ctrl = hash & GROUP_H2_MASK;

    /* Starting group index */
    node_pair_t *table = hashtable->table;
    size_t group_idx = (hash >> GROUP_H1_SHIFT) & group_mask;
#ifndef SPARSE_LIN_PROBE
    size_t probe_step = 1;
#endif

    /* Simplifies stuff by having an eternal loop */
    while(1)
    {
        /* Index for the <key, entries> table */
        size_t i = group_idx * GROUP_SIZE;

        /* Create the new masks */
        uint8_t *bitmap_pos = &hashtable->bitmap[i];
        uint16_t eq_mask = _ht_eq_mask(bitmap_pos, bitmap_ctrl);
        uint16_t empty_mask = _ht_eq_mask(bitmap_pos, ENTRY_EMPTY);

        while(eq_mask)
        {
            /* Found an empty position */
            size_t pos = _get_first_set_bit_pos(eq_mask);

            if(hashtable->comp(table[i + pos].key, key))
            {
                /* Put a tombstone only if there no empty entries in the group */
                hashtable->bitmap[i + pos] = (empty_mask) ? ENTRY_EMPTY : ENTRY_DELETED;
                hashtable->destruct(table[i + pos].entry, table[i + pos].key);
                hashtable->entries--;
                return HASH_OK;
            }

            /* Unset this entry */
            eq_mask ^= 1 << pos;
        }

        /* Stop condition */
        if(empty_mask)
            return HASH_ENTRY_NOT_EXISTS;

/* Probe */
#ifdef SPARSE_LIN_PROBE
        group_idx = (group_idx + 1) & group_mask;
#else
        group_idx = (group_idx + probe_step) & group_mask;
#endif
    }
}

/* The main resize sub-routine. Performs up and down resizes. */
static int _ht_node_resize(node_hashtable_t *hashtable, size_t new_sz)
{
    /* Constants */
    const size_t num_of_groups = hashtable->hashtable_sz >> GROUP_SIZE_SHIFT;

    /* New tables */
    node_pair_t *new_table = calloc(new_sz, sizeof(node_pair_t));
    uint8_t *new_bitmap = aligned_alloc(BITMAP_FORCE_ALLIGN, new_sz * sizeof(uint8_t));

    if(!new_table || !new_bitmap)
    {
        free(new_table);
        free(new_bitmap);
        return HASH_REHASH_MEM_ALLOC;
    }

    /* Set every entry as empty in the new bitmap */
    memset(new_bitmap, ENTRY_EMPTY, new_sz * sizeof(uint8_t));

    /* Setup variables - Old table parameters */
    node_pair_t *old_table = hashtable->table;
    uint8_t *old_bitmap = hashtable->bitmap;

    /* Update the new_table parameters */
    hashtable->bitmap = new_bitmap;
    hashtable->table = new_table;
    hashtable->hashtable_sz = new_sz;
    hashtable->iterator.iter_state = ITER_NOT_VALID;
    hashtable->group_num = new_sz >> GROUP_SIZE_SHIFT;
    hashtable->entries = 0;

    /* Iterate over the old table */
    for(size_t i = 0; i < num_of_groups; i++)
    {
        /* Find empty_or_deleted -> Invert for the valid entries */
        uint16_t valid_entries_mask = ~(_ht_and_mask(&old_bitmap[i * GROUP_SIZE], HIGH_BIT_MASK));
        size_t cur_idx = i * GROUP_SIZE;

        /* This iteration step is trivial - No entries in this group */
        if(!valid_entries_mask)
            continue;

        /* Extract every <key, entry> pair and insert in the new table */
        while(valid_entries_mask)
        {
            size_t pos = _get_first_set_bit_pos(valid_entries_mask);
            size_t tmp = cur_idx + pos;

            /* Insert safely - Each key is unique */
            _ht_node_insert(hashtable, old_table[tmp].key, old_table[tmp].entry, NO_SEARCH);

            /* Unset this entry */
            valid_entries_mask ^= 1 << pos;
        }
    }

    /* Free the old tables */
    free(old_bitmap);
    free(old_table);

    return HASH_OK;
}

/************************************ Main Routines for Node ************************************/

node_hashtable_t *ht_node_create(size_t hashtable_sz,
                                 int (*comp)(const void *, const void *),
                                 void (*destruct)(void *, void *),
                                 size_t (*hash)(const void *), int *error_code)
{
    /* Set the error code */
    *error_code = HASH_OK;

    /* Check input by the user - Necessary inputs */
    if(!hashtable_sz || !comp || !destruct || !hash)
    {
        *error_code = HASH_WRONG_ARGUMENT;
        return NULL;
    }

    /* Fix the size for the hashtable.
     * Minimum is 32 entries (2 groups) since alligned alloc requires multiples of
     * allignment (address sanitizer report ?) of 32. Not much of a difference 32 is a pretty good
     * starting size.But this is actually a problem with the standard (there are 3 versions of
     * what to do in this situation) so just allocate more, eh.
     * TODO -> Maybe this could be also moved to a better C99 solution ?*/
    hashtable_sz = (hashtable_sz > (2 * GROUP_SIZE)) ? _get_next_power_of_two(hashtable_sz) : (2 * GROUP_SIZE);

    /* Try to allocate memory for the structure */
    node_hashtable_t *hashtable = malloc(sizeof(node_hashtable_t));

    if(!hashtable)
    {
        *error_code = HASH_CREATE_MEM_ALLOC;
        return NULL;
    }

    hashtable->table = calloc(hashtable_sz, sizeof(node_pair_t));
    hashtable->bitmap = aligned_alloc(BITMAP_FORCE_ALLIGN, hashtable_sz * sizeof(uint8_t));

    /* Final assertions */
    if(!hashtable->table || !hashtable->bitmap)
    {
        *error_code = HASH_CREATE_MEM_ALLOC;
        free(hashtable->table);
        free(hashtable->bitmap);
        free(hashtable);
        return NULL;
    }

    /* Initialize the bitmap - Set it to 0xFF everywhere */
    memset(hashtable->bitmap, ENTRY_EMPTY, hashtable_sz * sizeof(uint8_t));

    /* Seeding of the hashtable */
    srand(time(NULL));
    hashtable->hash_seed = rand();

    /* Main parameters of the hashtable - Initialize all the sizes and the seed */
    hashtable->hashtable_sz = hashtable_sz;
    hashtable->group_num = hashtable_sz >> GROUP_SIZE_SHIFT;
    hashtable->entries = 0;
    hashtable->iterator.iter_state = ITER_NOT_VALID;

    /* Function pointers */
    hashtable->destruct = destruct;
    hashtable->comp = comp;
    hashtable->hash = hash;

    return hashtable;
}

void *ht_node_search(node_hashtable_t *hashtable, const void *key)
{
    /* Check user input */
    if(HT_LIKELY(!key || !hashtable))
        return NULL;

    return _ht_node_search(hashtable, key);
}

void *ht_node_insert(node_hashtable_t *hashtable, void *key, void *entry, int *error_code)
{
    /* Check user input */
    if(HT_LIKELY(!key || !entry || !hashtable || !error_code))
    {
        *error_code = HASH_WRONG_ARGUMENT;
        return NULL;
    }

    /* Set error code */
    *error_code = HASH_OK;

    void *ret = _ht_node_insert(hashtable, key, entry, SEARCH_NO_REPLACE);

    /* Also check if there is a need for rehashing */
    if(hashtable->entries > UPPER_LIMIT(hashtable->hashtable_sz))
    {
        /* Inforn in case of rehashing error - Doubling is the resizing policy */
        *error_code = _ht_node_resize(hashtable, hashtable->hashtable_sz << 1);
    }

    return ret;
}

int ht_node_delete(node_hashtable_t *hashtable, const void *key)
{
    int ret = HASH_OK;

    /* Check user input */
    if(HT_LIKELY(!key || !hashtable))
        return HASH_WRONG_ARGUMENT;

    ret = _ht_node_delete(hashtable, key);

    /* Also check if there is a need for rehashing */
    if((hashtable->entries < LOWER_LIMIT(hashtable->hashtable_sz)) && hashtable->hashtable_sz != (2 * GROUP_SIZE))
    {
        /* Policy is size halving */
        ret = _ht_node_resize(hashtable, hashtable->hashtable_sz >> 1);
    }

    return ret;
}

void ht_node_free(node_hashtable_t *hashtable)
{
    /* Nothing to free, return */
    if(!hashtable)
        return;

    const size_t num_of_groups = hashtable->hashtable_sz >> GROUP_SIZE_SHIFT;
    node_pair_t *table = hashtable->table;

    /* Iterate over the table */
    for(size_t i = 0; i < num_of_groups; i++)
    {
        /* Find empty_or_deleted -> Invert for the valid entries */
        size_t cur_idx = i * GROUP_SIZE;
        uint16_t valid_entries_mask = ~(_ht_and_mask(&hashtable->bitmap[cur_idx], HIGH_BIT_MASK));

        /* This iteration step is trivial - No entries in this group */
        if(!valid_entries_mask)
            continue;

        /* Extract every <key, entry> pair and insert in the new table */
        while(valid_entries_mask)
        {
            size_t pos = _get_first_set_bit_pos(valid_entries_mask);
            hashtable->destruct(table[cur_idx + pos].entry, table[cur_idx + pos].key);

            /* Unset this entry */
            valid_entries_mask ^= 1 << pos;
        }
    }

    /* Free the entries table and the structure itself */
    free(hashtable->bitmap);
    free(hashtable->table);
    free(hashtable);
}

node_hashtable_tuple_t ht_node_start_it(node_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    node_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };
    hashtable->iterator.iter_state = ITER_NOT_VALID;

    if(hashtable->entries)
    {
        /* Get the first valid group - Always returns valid */
        hashtable->iterator.cur_group = 0;
        hashtable->iterator.cur_group_mask = 0;
        hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, 1);

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = (hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos;
        ret_iter.key = hashtable->table[idx].key;
        ret_iter.entry = hashtable->table[idx].entry;
    }

    return ret_iter;
}

node_hashtable_tuple_t ht_node_next_it(node_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    node_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };

    if(hashtable->iterator.iter_state == ITER_VALID)
    {
        /* We are still in the same group */
        if(!hashtable->iterator.cur_group_mask)
        {
            /* Move forward */
            hashtable->iterator.cur_group++;

            /* Get the first valid group */
            hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, 1);

            /* We reached the end */
            if(hashtable->iterator.iter_state == ITER_NOT_VALID)
            {
                return ret_iter;
            }
        }

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = (hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos;
        ret_iter.key = hashtable->table[idx].key;
        ret_iter.entry = hashtable->table[idx].entry;
    }

    return ret_iter;
}

node_hashtable_tuple_t ht_node_prev_it(node_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    node_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };

    if(hashtable->iterator.iter_state == ITER_VALID)
    {
        /* We are still in the same group */
        if(!hashtable->iterator.cur_group_mask)
        {
            /* We reached the end */
            if(!hashtable->iterator.cur_group)
            {
                return ret_iter;
            }

            /* Go to the previous group */
            hashtable->iterator.cur_group--;

            /* Get the first valid group */
            hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, -1);
        }

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = (hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos;
        ret_iter.key = hashtable->table[idx].key;
        ret_iter.entry = hashtable->table[idx].entry;
    }

    return ret_iter;
}

node_hashtable_tuple_t ht_node_end_it(node_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    node_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };
    hashtable->iterator.iter_state = ITER_NOT_VALID;

    if(hashtable->entries)
    {
        /* Get the last valid group - Always returns valid */
        hashtable->iterator.cur_group = hashtable->group_num - 1;
        hashtable->iterator.cur_group_mask = 0;
        hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, -1);

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = (hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos;
        ret_iter.key = hashtable->table[idx].key;
        ret_iter.entry = hashtable->table[idx].entry;
    }

    return ret_iter;
}

size_t ht_node_get_entries(node_hashtable_t *hashtable)
{
    return hashtable->entries;
}

size_t ht_node_get_capacity(node_hashtable_t *hashtable)
{
    return hashtable->hashtable_sz;
}

double ht_node_get_load_factor(node_hashtable_t *hashtable)
{
    return ((double)hashtable->entries) / hashtable->hashtable_sz;
}

void ht_node_print_mem_usage(node_hashtable_t *hashtable)
{
    size_t bitmap_sz = hashtable->hashtable_sz;
    size_t hashtable_sz = hashtable->hashtable_sz * sizeof(node_pair_t);
    size_t manager_structure_mem = sizeof(node_hashtable_t);

    size_t valid_entries = hashtable->entries * sizeof(node_pair_t);
    size_t valid_bitmap = hashtable->entries;

    size_t total_memory = bitmap_sz + hashtable_sz + manager_structure_mem;
    size_t used_memory = valid_entries + valid_bitmap + manager_structure_mem;

    printf("******SPARSE_NODE_STAT INFO******\n");
    printf("Entries: %ld - Capacity: %ld\n", hashtable->entries, hashtable->hashtable_sz);
    printf("Total mem used (bytes): %ld\n", total_memory);
    printf("Effective mem used(bytes): %ld\n", used_memory);
    printf("Memory util (bytes): %f\n", (double)used_memory / total_memory);
}
