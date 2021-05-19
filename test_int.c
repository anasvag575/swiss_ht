/////////////////////////////////
// Header comment place holder //
/////////////////////////////////

#include "flat_sparse_hashtable.h"
#include "node_sparse_hashtable.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct small_4
{
    int trash;
} small_4;

typedef struct small_8
{
    int trash;
    int trash2;
} small_8;

typedef struct medium_20
{
    int trash;
    int trash2[4];
} medium_20;

typedef struct medium_32
{
    int trash;
    int trash2[7];
} medium_32;

typedef struct large_64
{
    int trash;
    int trash2[15];
} large_64;

typedef struct large_128
{
    int trash;
    int trash2[31];
} large_128;

typedef struct key_16byte
{
    long int key;
    long int trash;
} key_16byte;

typedef struct key_64byte
{
    long int key;
    long int trash[7];
} key_64byte;

/* Type of key - The test input */
typedef enum key_type
{
    INTEGER_4BYTE = sizeof(int),
    LONG_8BYTE = sizeof(long int),
    KEY_16BYTE = sizeof(key_16byte),
    KEY_64_BYTE = sizeof(key_64byte),
} key_type;

/* Type of payload */
typedef enum payload_type
{
    SMALL_4 = sizeof(small_4),
    SMALL_8 = sizeof(small_8),
    MEDIUM_20 = sizeof(medium_20),
    MEDIUM_32 = sizeof(medium_32),
    LARGE_64 = sizeof(large_64),
    LARGE_128 = sizeof(large_128),
} payload_type;

/* Type of data - The test input */
typedef enum seq_type
{
    SEQUENTIAL,
    SEMI_SEQUENTIAL,
    RANDOM,
    SEMI_RANDOM,
} seq_type;

/* Store for the results for comparing later */
typedef struct insert_results
{
    double insertion_time;
    double success_search;
    double fail_search;

} insert_results;

/* These can be extended for each testcase */
#define NUM_OF_KEYSIZES 4
#define NUM_OF_PAYLOADS 6
#define TEST_TYPES      8
#define NS_TIME         ((double)1e9)

const int payload_sizes[] = { SMALL_4, SMALL_8, MEDIUM_20, MEDIUM_32, LARGE_64, LARGE_128 };
const int key_sizes[] = { INTEGER_4BYTE, LONG_8BYTE, KEY_16BYTE, KEY_64_BYTE };

/* Generic shuffler */
static void shuffle_array(void *array, size_t arr_size, size_t num_shufl, size_t elsize)
{
    char tmp[elsize];
    char *arr = array;
    for(size_t i = 0; i < num_shufl - 1; i++)
    {
        int j = i + rand() / (RAND_MAX / (arr_size - i) + 1);
        memcpy(tmp, arr + j * elsize, elsize);
        memcpy(arr + j * elsize, arr + i * elsize, elsize);
        memcpy(arr + i * elsize, tmp, elsize);
    }
}

/* Print the type of testcases we are at */
static void print_testcase_type(int reserved, seq_type seq, payload_type payload)
{
    printf("Running testcase for case: {");

    /* Check type of data we insert */
    switch(seq)
    {
    case SEQUENTIAL:
        printf("Sequential |");
        break;
    case SEMI_SEQUENTIAL:
        printf("Semi/Sequential |");
        break;
    case RANDOM:
        printf("Random |");
        break;
    case SEMI_RANDOM:
        printf("Semi/Random |");
        break;
    }

    /* Size of the payload */
    size_t size = payload;
    printf(" %ld-byte payload with ", size);

    /* Type of payload */
    if(reserved)
        printf("reserve}\n");
    else
        printf("no reserve}\n");
}

/* Generate the entries vector - What is contained inside is
 * Dont care so its fine (Only size matters <:)> )*/
static void *generate_testcase(int test_size, payload_type payload)
{
    void *ret = calloc(test_size, (size_t)payload);

    if(!ret)
    {
        printf("Generation of test vector failed - OOM\nExiting...\n");
        exit(1);
    }

    return ret;
}

/* Initializes the keys array */
static void initialize_keys_array(int test_size, void *arr, key_type key_type)
{
    /* Just choose the key type */
    int *int_table = (int *)arr;
    long int *long_table = (long int *)arr;
    key_16byte *custom_table = (key_16byte *)arr;
    key_64byte *custom_table2 = (key_64byte *)arr;

    for(int i = 0; i < test_size; i++)
    {
        switch(key_type)
        {
        case INTEGER_4BYTE:
        {
            int_table[i] = i;
            break;
        }
        case LONG_8BYTE:
        {
            long_table[i] = i;
            break;
        }
        case KEY_16BYTE:
        {
            custom_table[i].key = i;
            custom_table[i].trash = 0;
            break;
        }
        case KEY_64_BYTE:
        {
            custom_table2[i].key = i;
            custom_table2[i].trash[0] = 0;
            custom_table2[i].trash[2] = 0;
            break;
        }
        default:
            break;
        }
    }
}

/* Generate the fail keys array - For now just reverses and makes neg */
static void generate_fail_keys(int test_size, void *arr, key_type key_type)
{
    /* Just choose the key type */
    int *int_table = (int *)arr;
    long int *long_table = (long int *)arr;
    key_16byte *custom_table = (key_16byte *)arr;
    key_64byte *custom_table2 = (key_64byte *)arr;

    /* Just reverse each number - pretty simple and for
     * large cases we will have a good result*/
    for(int i = 0; i < test_size; i++)
    {
        switch(key_type)
        {
        case INTEGER_4BYTE:
        {
            if(int_table[i])
                int_table[i] = -int_table[i];
            else
                int_table[i] = -1;
            break;
        }
        case LONG_8BYTE:
        {
            if(long_table[i])
                long_table[i] = -long_table[i];
            else
                long_table[i] = -1;
            break;
        }
        case KEY_16BYTE:
        {
            if(custom_table[i].key)
                custom_table[i].key = -custom_table[i].key;
            else
                custom_table[i].key = -1;
            break;
        }
        case KEY_64_BYTE:
        {
            if(custom_table2[i].key)
                custom_table2[i].key = -custom_table2[i].key;
            else
                custom_table2[i].key = -1;
            break;
        }
        default:
            break;
        }
    }
}

/* Generate the fail keys array - For now just reverses and makes neg */
void *generate_key_array(int test_size, seq_type seq, key_type key_type)
{
    void *test_arr = malloc(test_size * key_type);

    if(!test_arr)
    {
        printf("Generation of test key vector failed - OOM\nExiting...\n");
        exit(1);
    }

    /* Populate the array */
    initialize_keys_array(test_size, test_arr, key_type);

    /* In case we need to - Permutate the data */
    switch(seq)
    {
    case SEMI_SEQUENTIAL:
    {
        shuffle_array(test_arr, test_size, test_size / 10, key_type);
        break;
    }
    case SEMI_RANDOM:
    {
        shuffle_array(test_arr, test_size, test_size / 5, key_type);
        break;
    }
    case RANDOM:
    {
        shuffle_array(test_arr, test_size, test_size, key_type);
        break;
    }
    default:
        break;
    }

    return test_arr;
}

/* Test insertion in hot table - Flags -> (Reserve/(Or not), Random/Sequential,
 * Order of insertion, Payload size)*/
insert_results test_simple_insert(int test_size, int reserved, seq_type seq, payload_type payload, key_type key_type, int print_flag)
{
    /* We choose the size of the hashtable - Relatively simple only during
     * construction */
    const int hashtable_size = (reserved) ? (1.4 * test_size) : (4);
    int op_error_code; /* Ignored in order to count only operations' cost */

    int fail_insertions = 0;
    int success_searhes = 0;
    int fail_searches = 0;

    /* Measurements */
    clock_t start, end;
    insert_results avg_time;

    if(print_flag)
    {
        printf("\n*************** Insertion of %d-byte keys ***************\n", key_type);
        print_testcase_type(reserved, seq, payload);
    }
    flat_hashtable_t *hashtable = ht_flat_create(hashtable_size, (size_t)payload, key_type, &op_error_code);

    /* Generate the <keys,entries> pairs */
    char *test_arr = generate_key_array(test_size, seq, key_type);
    char *test_entries = generate_testcase(test_size, payload);

    start = clock();
    for(int i = 0; i < test_size; i++)
    {
        char *cur_entry = test_entries + (i * payload);
        char *cur_key = test_arr + (i * key_type);
        if(ht_flat_insert(hashtable, cur_key, cur_entry, &op_error_code))
            fail_insertions++;
    }
    end = clock() - start;
    avg_time.insertion_time = ((double)end / CLOCKS_PER_SEC) / test_size;
    if(print_flag)
        printf("Avg Time took for insert is %f ns\n", avg_time.insertion_time * 1e9);

    /***********************/

    start = clock();
    for(int i = 0; i < test_size; i++)
    {
        char *cur_key = test_arr + (i * key_type);
        if(ht_flat_search(hashtable, cur_key))
            success_searhes++;
    }
    end = clock() - start;
    avg_time.success_search = ((double)end / CLOCKS_PER_SEC) / test_size;
    if(print_flag)
        printf("Avg Time took for search is %f ns\n", avg_time.success_search * 1e9);

    /***********************/

    generate_fail_keys(test_size, test_arr, key_type);

    start = clock();
    for(int i = 0; i < test_size; i++)
    {
        char *cur_key = test_arr + (i * key_type);
        if(!ht_flat_search(hashtable, cur_key))
            fail_searches++;
    }
    end = clock() - start;
    avg_time.fail_search = ((double)end / CLOCKS_PER_SEC) / test_size;
    if(print_flag)
        printf("Avg Time took for failed search is %f ns\n", avg_time.fail_search * 1e9);

    /* Assertions that everything went well */
    if(fail_insertions || (success_searhes != test_size) || (fail_searches != test_size))
    {
        printf("Fail insertions %d and search %d and fail %d\n", fail_insertions, success_searhes, fail_searches);
        printf("Testing went wrong !\nExiting...\n");
        exit(1);
    }

    /* Free */
    ht_flat_free(hashtable);
    free(test_arr);
    free(test_entries);

    return avg_time;
}

/* Test delete in hot table - Flags -> (Reserve/(Or not), Random/Sequential,
 * Order of insertion, Payload size)
 * Deletes are made on the whole table so resizes included (basically a clear)
 */
insert_results test_simple_delete(int test_size, int reserved, seq_type seq, payload_type payload, key_type key_type, int print_flag)
{
    /* We choose the size of the hashtable - Relatively simple only during
     * construction */
    const int hashtable_size = (reserved) ? (1.4 * test_size) : (4);
    int op_error_code; /* Ignored in order to count only operations' cost */

    int fail_insertions = 0;
    int success_deletes = 0;

    /* Measurements */
    clock_t start, end;
    insert_results avg_time;

    if(print_flag)
    {
        printf("\n*************** Insert/Delete of %d-byte keys ***************\n", key_type);
        print_testcase_type(reserved, seq, payload);
    }
    flat_hashtable_t *hashtable = ht_flat_create(hashtable_size, (size_t)payload, key_type, &op_error_code);

    /* Generate the <keys,entries> pairs */
    char *test_arr = generate_key_array(test_size, seq, key_type);
    char *test_entries = generate_testcase(test_size, payload);

    start = clock();
    for(int i = 0; i < test_size; i++)
    {
        char *cur_entry = test_entries + (i * payload);
        char *cur_key = test_arr + (i * key_type);
        if(ht_flat_insert(hashtable, cur_key, cur_entry, &op_error_code))
            fail_insertions++;
    }
    end = clock() - start;
    avg_time.insertion_time = ((double)end / CLOCKS_PER_SEC) / test_size;
    if(print_flag)
        printf("Avg Time took for insert is %f ns\n", avg_time.insertion_time * 1e9);

    /***********************/

    start = clock();
    for(int i = 0; i < test_size; i++)
    {
        char *cur_key = test_arr + (i * key_type);
        if(!ht_flat_delete(hashtable, cur_key))
            success_deletes++;
    }
    end = clock() - start;
    avg_time.success_search = ((double)end / CLOCKS_PER_SEC) / test_size;
    if(print_flag)
        printf("Avg Time took for deletes is %f ns\n", avg_time.success_search * 1e9);

    /* Assertions that everything went well */
    if(fail_insertions || (success_deletes != test_size))
    {
        printf("Fail insertions %d and success deletes %d\n", fail_insertions, success_deletes);
        printf("Testing went wrong !\nExiting...\n");
        exit(1);
    }

    /* Free */
    ht_flat_free(hashtable);
    free(test_entries);
    free(test_arr);

    return avg_time;
}

/* Testing of iterators just to see if they work properly */
void test_iterators(int print_flag)
{
    /* We choose the size of the hashtable - Relatively simple only during
     * construction */

    /* The average "small" entries case */
    int test_size = 1000000;
    seq_type seq = SEQUENTIAL;
    payload_type payload = SMALL_4;
    key_type key_type = INTEGER_4BYTE;
    const int hashtable_size = 1.4 * test_size;

    /* Proper state is 0 - HASH_OK */
    int op_error_code = 0;

    if(print_flag)
        printf("\n*************** Testing iterators ***************\n");

    flat_hashtable_t *hashtable = ht_flat_create(hashtable_size, (size_t)payload, key_type, &op_error_code);

    /* Generate the <keys,entries> pairs */
    char *test_arr = generate_key_array(test_size, seq, key_type);
    char *test_entries = generate_testcase(test_size, payload);

    /* First load the hashtable */
    for(int i = 0; i < test_size; i++)
    {
        char *cur_entry = test_entries + (i * payload);
        char *cur_key = test_arr + (i * key_type);
        if(ht_flat_insert(hashtable, cur_key, cur_entry, &op_error_code) || op_error_code)
        {
            printf("Critical failure during insertion[iterator test].Exiting...\n");
            exit(1);
        }
    }

    /***************************** FORWARD ITERATOR *******************************/

    /* Iterate over the values since now they are hot in cache already */
    flat_hashtable_tuple_t iter = ht_flat_start_it(hashtable);
    int *last_key = NULL;
    int iter_num = 0;

    /* While entries still exist */
    clock_t start = clock();
    while(iter.key)
    {
        last_key = iter.key;
        iter_num++;
        iter = ht_flat_next_it(hashtable);
    }
    clock_t end = clock() - start;
    double avg_time = (double)end / CLOCKS_PER_SEC;
    avg_time = (avg_time / test_size) * NS_TIME;

    if(iter_num != test_size)
    {
        printf("Iterator not working properly -> %d | Exiting...\n", iter_num);
        exit(1);
    }
    else if(print_flag)
    {
        printf("Last item in forward %d (Also disabling optimizer)\n", *last_key);
        printf("Avg time for forward iteration: %f ns\n", avg_time);
    }
    /***************************** BACKWARD ITERATOR *******************************/

    /* Iterate over the values since now they are hot in cache already */
    iter = ht_flat_end_it(hashtable);
    last_key = NULL;
    iter_num = 0;

    /* While entries still exist */
    start = clock();
    while(iter.key)
    {
        last_key = iter.key;
        iter_num++;
        iter = ht_flat_prev_it(hashtable);
    }
    end = clock() - start;
    avg_time = (double)end / CLOCKS_PER_SEC;
    avg_time = (avg_time / test_size) * NS_TIME;

    if(iter_num != test_size)
    {
        printf("Iterator not working properly -> %d | Exiting...\n", iter_num);
        exit(1);
    }
    else if(print_flag)
    {
        printf("Last item in backward %d (Also disabling optimizer)\n", *last_key);
        printf("Avg time for backward iteration: %f ns\n", avg_time);
    }
    /***************************** ASSERT PROPER FUNCTIONALITY **********************************/

    /* End -> Using next (We should iterate over the last group ONLY < 16) */
    iter = ht_flat_end_it(hashtable);
    iter_num = 0;

    /* While entries still exist */
    while(iter.key)
    {
        last_key = iter.key;
        iter_num++;
        iter = ht_flat_next_it(hashtable);
    }

    /* Means we moved to other groups */
    if(iter_num > 16)
    {
        printf("End iterator move outside expected limits using next. Exiting...\n");
        exit(1);
    }
    else if(print_flag)
    {
        printf("Iterator end to next worked fine %d\n", iter_num);
    }

    /* Start -> Using prev (We should iterate over the first group ONLY < 16) */
    iter = ht_flat_end_it(hashtable);
    iter_num = 0;

    /* While entries still exist */
    while(iter.key)
    {
        last_key = iter.key;
        iter_num++;
        iter = ht_flat_next_it(hashtable);
    }

    /* Means we moved to other groups */
    if(iter_num > 16)
    {
        printf("Start iterator move outside expected limits using prev. Exiting...\n");
        exit(1);
    }
    else if(print_flag)
    {
        printf("Iterator start to prev worked fine %d\n", iter_num);
    }

    /* Free the hashtable and the test table */
    ht_flat_free(hashtable);
    free(test_arr);
    free(test_entries);
}

/* We know the size of the results */
void analyze_results(insert_results res[TEST_TYPES][NUM_OF_KEYSIZES][NUM_OF_PAYLOADS], int is_delete)
{
    /* TODO Check which is the average best and worst testcase across the board*/

    /* TODO Plot results of small int keys with different payloads */

    /* TODO Plot for large keys with different payloads */

    /* TODO Plot for different sizes ? */

    /* TODO Do it for each different test type */

    //    for(int i = 0; i < TEST_TYPES; i++)
    //    {
    //        /* Accumulate all keysizes together */
    //        for(int j = 0; j < NUM_OF_KEYSIZES; j++)
    //        {
    //            double avg_ins_inc = 0;
    //            double avg_suc_search_inc = 0;
    //            double avg_fail_search_inc = 0;
    //            double avg_sz_incr = 0;
    //
    //            for(int k = 0; k < NUM_OF_PAYLOADS - 1; k++)
    //            {
    //                avg_ins_inc += (res[i][j][k+1].insertion_time - res[i][j][k].insertion_time) * NS_TIME;
    //                avg_suc_search_inc += (res[i][j][k+1].success_search - res[i][j][k].success_search) * NS_TIME;
    //                avg_fail_search_inc += (res[i][j][k+1].fail_search - res[i][j][k].fail_search) * NS_TIME;
    //                avg_sz_incr +=  payload_sizes[k+1] - payload_sizes[k];
    //            }
    //
    //            /* Average the results and check */
    //            avg_ins_inc = avg_ins_inc/(NUM_OF_PAYLOADS - 1);
    //            avg_suc_search_inc = avg_suc_search_inc/(NUM_OF_PAYLOADS - 1);
    //            avg_fail_search_inc = avg_fail_search_inc/(NUM_OF_PAYLOADS - 1);
    //            avg_sz_incr = avg_sz_incr/(NUM_OF_PAYLOADS - 1);
    //
    //            printf("For keysize %d(bytes):\n", key_sizes[j]);
    //            printf("\tAverages: Insert: %f - Success search: %f - Fail search: %f - Size increase: %f\n",
    //                                avg_ins_inc, avg_suc_search_inc, avg_fail_search_inc, avg_sz_incr);
    //        }
    //    }

    /* Total averages to check the trend */
    double total_avg_time_ins = 0;
    double total_avg_suc_search = 0;
    double total_avg_fail_search = 0;

    /* Find total avgs for everything */
    for(int i = 0; i < TEST_TYPES; i++)
    {
        for(int j = 0; j < NUM_OF_KEYSIZES; j++)
        {
            for(int k = 0; k < NUM_OF_PAYLOADS; k++)
            {
                total_avg_time_ins += res[i][j][k].insertion_time * NS_TIME;
                total_avg_suc_search += res[i][j][k].success_search * NS_TIME;
                total_avg_fail_search += res[i][j][k].fail_search * NS_TIME;
            }
        }
    }

    total_avg_time_ins = total_avg_time_ins / (TEST_TYPES * NUM_OF_KEYSIZES * NUM_OF_PAYLOADS);
    total_avg_suc_search = total_avg_suc_search / (TEST_TYPES * NUM_OF_KEYSIZES * NUM_OF_PAYLOADS);
    total_avg_fail_search = total_avg_fail_search / (TEST_TYPES * NUM_OF_KEYSIZES * NUM_OF_PAYLOADS);

    /* Means we analyze insert */
    if(!is_delete)
    {
        printf("*******Total Averages (in ns)*******\nInsert: %f - Success search: %f - Fail search: %f\n",
               total_avg_time_ins,
               total_avg_suc_search,
               total_avg_fail_search);
    }
    else
    {
        printf("*******Total Averages (in ns)*******\nInsert: %f - Delete: %f\n", total_avg_time_ins, total_avg_suc_search);
    }
}

int main()
{
    /* Run the tests */
    int test_size = 100000;

    /* Compiler test - Clang will print both though :( */
#if defined(__clang__)
    printf("Using clang !!\n");
#elif defined(__GNUC__)
    printf("Using gcc !!\n");
#else
    printf("Unknown compiler!!\n");
#endif

/* Overloaded add for the results struct */
#define ADD_TO_RES_STR(x, y)                                                                                                                         \
    do                                                                                                                                               \
    {                                                                                                                                                \
        x.fail_search += y.fail_search;                                                                                                              \
        x.success_search += y.success_search;                                                                                                        \
        x.insertion_time += y.insertion_time;                                                                                                        \
    } while(0)

/* Overloaded divide for the results struct */
#define AVG_TO_RES_STR(x, div)                                                                                                                       \
    do                                                                                                                                               \
    {                                                                                                                                                \
        x.fail_search = x.fail_search / div;                                                                                                         \
        x.success_search = x.success_search / div;                                                                                                   \
        x.insertion_time = x.insertion_time / div;                                                                                                   \
    } while(0)

/* Use 1 when in need of live printing */
#define SMOOTH_RUNS 1

    /* Prints the results of each test while running */
    int print_flag = 1;

    /* Results table */
    insert_results res_tables[TEST_TYPES][NUM_OF_KEYSIZES][NUM_OF_PAYLOADS];
    insert_results temp;
    memset(res_tables, 0, NUM_OF_KEYSIZES * NUM_OF_PAYLOADS * TEST_TYPES * sizeof(insert_results));

    /* For integers 4-bytes on my machine */
    for(int j = 0; j < NUM_OF_KEYSIZES; j++) /* For different key_sizes */
    {
        for(int i = 0; i < NUM_OF_PAYLOADS; i++) /* For different payloads */
        {
            for(int k = 0; k < SMOOTH_RUNS; k++)
            {
                /* Sequential insertions */
                temp = test_simple_insert(test_size, 1, SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[0][j][i], temp);

                temp = test_simple_insert(test_size, 0, SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[1][j][i], temp);

                /* Random insertions */
                temp = test_simple_insert(test_size, 1, RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[2][j][i], temp);

                temp = test_simple_insert(test_size, 0, RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[3][j][i], temp);

                /* Semi-Random insertions */
                temp = test_simple_insert(test_size, 1, SEMI_RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[4][j][i], temp);

                temp = test_simple_insert(test_size, 0, SEMI_RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[5][j][i], temp);

                /* Semi-Sequential insertions */
                temp = test_simple_insert(test_size, 1, SEMI_SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[6][j][i], temp);

                temp = test_simple_insert(test_size, 0, SEMI_SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[7][j][i], temp);
            }

            /* Average the results */
            for(int k = 0; k < TEST_TYPES; k++)
            {
                AVG_TO_RES_STR(res_tables[k][j][i], SMOOTH_RUNS);
            }
        }
    }

    /* Performs a small analysis on the insert results */
    analyze_results(res_tables, 0);

    /* Enable/Disable for delete */
    print_flag = 0;

    /* Reinitialize */
    memset(res_tables, 0, NUM_OF_KEYSIZES * NUM_OF_PAYLOADS * TEST_TYPES * sizeof(insert_results));

    /* For integers 4-bytes on my machine */
    for(int j = 0; j < NUM_OF_KEYSIZES; j++) /* For different key_sizes */
    {
        for(int i = 0; i < NUM_OF_PAYLOADS; i++) /* For different payloads */
        {
            for(int k = 0; k < SMOOTH_RUNS; k++)
            {
                /* Sequential insertions */
                temp = test_simple_delete(test_size, 1, SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[0][j][i], temp);
                temp = test_simple_delete(test_size, 0, SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[1][j][i], temp);

                /* Random insertions */
                temp = test_simple_delete(test_size, 1, RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[2][j][i], temp);
                temp = test_simple_delete(test_size, 0, RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[3][j][i], temp);

                /* Semi-Random insertions */
                temp = test_simple_delete(test_size, 1, SEMI_RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[4][j][i], temp);
                temp = test_simple_delete(test_size, 0, SEMI_RANDOM, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[5][j][i], temp);

                /* Semi-Sequential insertions */
                temp = test_simple_delete(test_size, 1, SEMI_SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[6][j][i], temp);
                temp = test_simple_delete(test_size, 0, SEMI_SEQUENTIAL, payload_sizes[i], key_sizes[j], print_flag);
                ADD_TO_RES_STR(res_tables[7][j][i], temp);
            }

            /* Average the results */
            for(int k = 0; k < TEST_TYPES; k++)
            {
                AVG_TO_RES_STR(res_tables[k][j][i], SMOOTH_RUNS);
            }
        }
    }

    /* Performs a small analysis on the insert results */
    analyze_results(res_tables, 1);

    /* Testing iterators */
    test_iterators(1);

    return 1;
}
