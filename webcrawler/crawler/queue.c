#include "crawler/queue.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int advance_slot(int index, int capacity)
{
    return (index + 1) % capacity;
}

static void clear_entry(queue_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }

    entry->url = NULL;
    entry->depth = 0;
}

static void free_buffer_contents(url_queue_t *q)
{
    int i;

    for (i = 0; i < q->count; i++) {
        int pos = (q->front + i) % q->capacity;
        free(q->entries[pos].url);
        clear_entry(&q->entries[pos]);
    }
}

int url_queue_init(url_queue_t *q, int capacity)
{
    if (q == NULL) {
        return -1;
    }

    if (capacity <= 0) {
        capacity = QUEUE_DEFAULT_CAPACITY;
    }

    q->entries = (queue_entry_t *)calloc((size_t)capacity, sizeof(queue_entry_t));
    if (q->entries == NULL) {
        return -1;
    }

    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->capacity = capacity;
    q->peak_size = 0;
    q->shutting_down = 0;

    if (pthread_mutex_init(&q->lock, NULL) != 0) {
        free(q->entries);
        q->entries = NULL;
        return -1;
    }

    if (pthread_cond_init(&q->can_push, NULL) != 0) {
        pthread_mutex_destroy(&q->lock);
        free(q->entries);
        q->entries = NULL;
        return -1;
    }

    if (pthread_cond_init(&q->can_pop, NULL) != 0) {
        pthread_cond_destroy(&q->can_push);
        pthread_mutex_destroy(&q->lock);
        free(q->entries);
        q->entries = NULL;
        return -1;
    }

    return 0;
}

void url_queue_destroy(url_queue_t *q)
{
    if (q == NULL) {
        return;
    }

    pthread_mutex_lock(&q->lock);

    free_buffer_contents(q);
    free(q->entries);

    q->entries = NULL;
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->capacity = 0;
    q->peak_size = 0;
    q->shutting_down = 1;

    pthread_mutex_unlock(&q->lock);

    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->can_push);
    pthread_cond_destroy(&q->can_pop);
}

int url_queue_push(url_queue_t *q, const char *url, int depth)
{
    char *url_copy;

    if (q == NULL || url == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count == q->capacity && !q->shutting_down) {
        pthread_cond_wait(&q->can_push, &q->lock);
    }

    if (q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    url_copy = strdup(url);
    if (url_copy == NULL) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    q->entries[q->rear].url = url_copy;
    q->entries[q->rear].depth = depth;
    q->rear = advance_slot(q->rear, q->capacity);
    q->count++;

    if (q->count > q->peak_size) {
        q->peak_size = q->count;
    }

    pthread_cond_signal(&q->can_pop);
    pthread_mutex_unlock(&q->lock);

    return 0;
}

int url_queue_pop(url_queue_t *q, char **url_out, int *depth_out)
{
    if (q == NULL || url_out == NULL || depth_out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->shutting_down) {
        pthread_cond_wait(&q->can_pop, &q->lock);
    }

    if (q->count == 0 && q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    *url_out = q->entries[q->front].url;
    *depth_out = q->entries[q->front].depth;

    clear_entry(&q->entries[q->front]);
    q->front = advance_slot(q->front, q->capacity);
    q->count--;

    pthread_cond_signal(&q->can_push);
    pthread_mutex_unlock(&q->lock);

    return 0;
}

void url_queue_shutdown(url_queue_t *q)
{
    if (q == NULL) {
        return;
    }

    pthread_mutex_lock(&q->lock);
    q->shutting_down = 1;
    pthread_cond_broadcast(&q->can_push);
    pthread_cond_broadcast(&q->can_pop);
    pthread_mutex_unlock(&q->lock);
}

int url_queue_size(url_queue_t *q)
{
    int current;

    if (q == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->lock);
    current = q->count;
    pthread_mutex_unlock(&q->lock);

    return current;
}

int url_queue_max_depth(url_queue_t *q)
{
    int highest;

    if (q == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->lock);
    highest = q->peak_size;
    pthread_mutex_unlock(&q->lock);

    return highest;
}

