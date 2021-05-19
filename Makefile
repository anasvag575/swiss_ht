# Compilation Flags #
CC = gcc
DFLAGS = -g
WFLAGS = -Wall -Wno-pointer-arith -pedantic-errors
OFLAGS = -O3 -march=native
OFLAGS_SEE = -msse -msse2
CFLAGS = $(DFLAGS) $(WFLAGS) $(OFLAGS) $(OFLAGS_SEE) $(DEBUG_CFLAGS)

#Debug for sanitizer
#DEBUG_CFLAGS = -fsanitize=address
#DEBUG_LFLAGS = -fsanitize=address -static-libasan

#Empty for release
DEBUG_CFLAGS = 
DEBUG_LFLAGS = 

# Linker Flags #
LFLAGS = -rdynamic $(DEBUG_LFLAGS)

# Compilation Objects #
OBJS2 = flat_sparse_hashtable.o test_int.o node_sparse_hashtable.o
OBJS1 = flat_sparse_hashtable.o test_str.o node_sparse_hashtable.o

# Program's Binary Name #
BINARYNAME2 = test_int
BINARYNAME1 = test_str

# Final Targets #
all: test_int test_str

test_int: $(OBJS2)
	$(CC) $(OBJS2) $(LFLAGS) -o $(BINARYNAME2) $(OBJFLAGS)

test_str: $(OBJS1)
	$(CC) $(OBJS1) $(LFLAGS) -o $(BINARYNAME1) $(OBJFLAGS)

test_int.o: test_int.c
	$(CC) $(CFLAGS) -c $< -o $@

test_str.o: test_str.c
	$(CC) $(CFLAGS) -c $< -o $@

flat_sparse_hashtable.o: flat_sparse_hashtable.c flat_sparse_hashtable.h
	$(CC) $(CFLAGS) -c $< -o $@

node_sparse_hashtable.o: node_sparse_hashtable.c node_sparse_hashtable.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean Objects and Created Files #
clean-all: clean clean-out
clean:
	rm -vf $(BINARYNAME2) $(BINARYNAME1) $(OBJS1) $(OBJS2)

