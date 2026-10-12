#include "crawler/visited.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Hash function
 * ----------------------------------------------------------------------- */

/* FNV-1a 32-bit hash over the bytes of s. */
static uint32_t fnv1a_32(const char *s)
{
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        h = (h ^ (uint32_t)*p) * 16777619u;
    return h;
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------------- */

/*
 * visited_init — allocate the hash table arrays and initialise the mutex.
 * Returns 0 on success, -1 on allocation failure.
 */
int visited_init(visited_set_t *v, size_t initial_capacity)
{
    v->keys   = calloc(initial_capacity, sizeof(char *));
    v->hashes = calloc(initial_capacity, sizeof(uint32_t));
    if (!v->keys || !v->hashes) {
        free(v->keys);
        free(v->hashes);
        return -1;
    }
    v->capacity = initial_capacity;
    v->count    = 0;
    pthread_mutex_init(&v->mutex, NULL);
    return 0;
}

/*
 * visited_destroy — free all stored keys and the table arrays, then
 * destroy the mutex. The set must not be used after this call.
 */
void visited_destroy(visited_set_t *v)
{
    pthread_mutex_lock(&v->mutex);
    for (size_t i = 0; i < v->capacity; i++) {
        if (v->keys[i] && v->keys[i] != VISITED_TOMBSTONE)
            free(v->keys[i]);
    }
    free(v->keys);
    free(v->hashes);
    v->keys   = NULL;
    v->hashes = NULL;
    pthread_mutex_unlock(&v->mutex);
    pthread_mutex_destroy(&v->mutex);
}

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

/*
 * _resize — rehash all live entries into a new table of new_cap slots.
 * Tombstones are dropped during the rehash.
 * Must be called with v->mutex held. Returns 0 on success, -1 on OOM.
 */
static int _resize(visited_set_t *v, size_t new_cap)
{
    char     **new_keys   = calloc(new_cap, sizeof(char *));
    uint32_t  *new_hashes = calloc(new_cap, sizeof(uint32_t));
    if (!new_keys || !new_hashes) {
        free(new_keys);
        free(new_hashes);
        return -1;
    }

    /* Re-insert every live entry into the new table. */
    for (size_t i = 0; i < v->capacity; i++) {
        char *k = v->keys[i];
        if (!k || k == VISITED_TOMBSTONE) continue;

        uint32_t h    = v->hashes[i];
        size_t   slot = h & (new_cap - 1);
        while (new_keys[slot])
            slot = (slot + 1) & (new_cap - 1);
        new_keys[slot]   = k;
        new_hashes[slot] = h;
    }

    free(v->keys);
    free(v->hashes);
    v->keys     = new_keys;
    v->hashes   = new_hashes;
    v->capacity = new_cap;
    return 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/*
 * visited_check_and_insert — atomically test whether url is in the set
 * and, if not, insert it.
 *
 * Returns:
 *   1  — url was not present and has been inserted (newly seen)
 *   0  — url was already in the set
 *  -1  — OOM while duplicating the URL string (treated as already visited)
 */
int visited_check_and_insert(visited_set_t *v, const char *url)
{
    pthread_mutex_lock(&v->mutex);

    uint32_t h    = fnv1a_32(url);
    size_t   mask = v->capacity - 1;
    size_t   slot = h & mask;
    size_t   first_tomb = (size_t)-1; /* index of the first tombstone seen */

    /* Linear probe: scan for a matching key or an empty slot. */
    for (size_t i = 0; i < v->capacity; i++) {
        size_t idx = (slot + i) & mask;
        char  *k   = v->keys[idx];

        if (!k) {
            /* Empty slot — url is not in the set; insert it. */
            size_t ins = (first_tomb != (size_t)-1) ? first_tomb : idx;
            char  *copy = strdup(url);
            if (!copy) {
                pthread_mutex_unlock(&v->mutex);
                return -1;
            }
            v->keys[ins]   = copy;
            v->hashes[ins] = h;
            v->count++;

            /* Grow the table if the load factor exceeds the threshold. */
            if ((double)v->count / (double)v->capacity > VISITED_LOAD_THRESHOLD)
                _resize(v, v->capacity * 2);

            pthread_mutex_unlock(&v->mutex);
            return 1;
        }

        if (k == VISITED_TOMBSTONE) {
            /* Record the first tombstone for possible reuse on insert. */
            if (first_tomb == (size_t)-1) first_tomb = idx;
            continue;
        }

        if (v->hashes[idx] == h && strcmp(k, url) == 0) {
            /* Found a matching entry — url was already visited. */
            pthread_mutex_unlock(&v->mutex);
            return 0;
        }
    }

    /* Probe exhausted without finding a slot (resize should prevent this). */
    pthread_mutex_unlock(&v->mutex);
    return 0;
}

/*
 * visited_count — return the number of URLs currently in the set.
 */
size_t visited_count(visited_set_t *v)
{
    pthread_mutex_lock(&v->mutex);
    size_t c = v->count;
    pthread_mutex_unlock(&v->mutex);
    return c;
}