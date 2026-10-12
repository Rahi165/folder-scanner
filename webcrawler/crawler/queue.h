#ifndef QUEUE_H
#define QUEUE_H

/*
 * queue.h
 *
 * Thread-safe bounded FIFO queue used as the crawler URL frontier.
 * The queue uses a circular buffer with:
 *   - one mutex
 *   - one condition variable for "not full"
 *   - one condition variable for "not empty"
 *
 * Behavior:
 *   - producers wait when the queue is full
 *   - consumers wait when the queue is empty
 *   - shutdown wakes all waiting threads
 */

#include <pthread.h>
#include <stddef.h>

#define QUEUE_DEFAULT_CAPACITY 1024

/* Single item stored in the URL frontier */
typedef struct {
    char *url;   /* dynamically allocated string */
    int depth;   /* crawl depth for this URL */
} queue_entry_t;

/* Bounded circular queue */
typedef struct {
    queue_entry_t *entries;
    int front;
    int rear;
    int count;
    int capacity;
    int peak_size;
    int shutting_down;

    pthread_mutex_t lock;
    pthread_cond_t can_push;
    pthread_cond_t can_pop;
} url_queue_t;

/* Initialize queue with the given capacity */
int url_queue_init(url_queue_t *q, int capacity);

/* Destroy queue and free any URLs still stored inside it */
void url_queue_destroy(url_queue_t *q);

/*
 * Push a new URL into the queue.
 * Returns 0 on success.
 * Returns -1 on failure or shutdown.
 */
int url_queue_push(url_queue_t *q, const char *url, int depth);

/*
 * Pop the next URL from the queue.
 * On success, caller receives ownership of *url_out and must free it.
 * Returns 0 on success.
 * Returns -1 if shutdown has happened and the queue is empty.
 */
int url_queue_pop(url_queue_t *q, char **url_out, int *depth_out);

/* Put the queue into shutdown mode and wake blocked threads */
void url_queue_shutdown(url_queue_t *q);

/* Return current number of items in queue */
int url_queue_size(url_queue_t *q);

/* Return the largest size the queue has reached */
int url_queue_max_depth(url_queue_t *q);

#endif