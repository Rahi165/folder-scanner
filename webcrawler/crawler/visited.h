#ifndef VISITED_H
#define VISITED_H

/*
 * visited.h — thread-safe hash set for tracking visited URLs.
 *
 * Implementation: open-addressing with FNV-1a hashing and linear probing.
 * A single mutex serialises all operations so that check-and-insert is atomic.
 * The table grows automatically when the load factor exceeds VISITED_LOAD_THRESHOLD.
 */

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

/* Initial table size and growth trigger. Both values must stay consistent:
 * capacity must always be a power of 2 so that (hash & (capacity-1)) works. */
#define VISITED_INIT_CAPACITY  65536U  /* initial slot count — power of 2 */
#define VISITED_LOAD_THRESHOLD 0.70    /* resize when count/capacity > 0.70 */

/* Sentinel stored in keys[] to mark a slot as deleted (tombstone).
 * Any pointer value that can never be a valid strdup'd string works;
 * (char *)1 is below any real heap address on all supported platforms. */
#define VISITED_TOMBSTONE ((char *)1)

typedef struct {
    char     **keys;     /* NULL = empty, VISITED_TOMBSTONE = deleted, else strdup'd URL */
    uint32_t  *hashes;   /* FNV-1a hash cached per slot to speed up resize              */
    size_t     capacity; /* total slot count — always a power of 2                      */
    size_t     count;    /* number of live (non-tombstone) entries                      */
    pthread_mutex_t mutex;
} visited_set_t;

/*
 * visited_init — initialise *v with initial_capacity slots (must be a power of 2).
 * Returns 0 on success, -1 on allocation failure.
 */
int visited_init(visited_set_t *v, size_t initial_capacity);

/*
 * visited_destroy — free all keys and internal arrays, then destroy the mutex.
 * Does not free the visited_set_t struct itself.
 * The set must not be used after this call.
 */
void visited_destroy(visited_set_t *v);

/*
 * visited_check_and_insert — atomically test and insert url.
 *
 * Returns 1 if url was absent and has now been inserted (caller should crawl it).
 * Returns 0 if url was already present (caller should skip it).
 * Returns -1 on OOM; the set is left unchanged and the URL is treated as visited.
 */
int visited_check_and_insert(visited_set_t *v, const char *url);

/*
 * visited_count — return the current number of live entries.
 * Acquires the mutex, so the result is exact at the moment of the call.
 */
size_t visited_count(visited_set_t *v);

#endif /* VISITED_H */