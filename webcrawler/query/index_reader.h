#ifndef INDEX_READER_H
#define INDEX_READER_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char *word;
    uint64_t postings_offset;
    uint32_t document_count;
} dictionary_entry_t;

typedef struct {
    dictionary_entry_t *items;
    size_t size;
} dictionary_t;

typedef struct {
    uint32_t docid;
    char *url;
    char *path;
    uint16_t depth;
} document_entry_t;

typedef struct {
    document_entry_t *items;
    size_t size;
} document_map_t;

int dictionary_load(dictionary_t *dict,const char *path);
void dictionary_free(dictionary_t *dict);
int dictionary_find(const dictionary_t *dict,const char *word);

int document_map_load(document_map_t *map,const char *path);
void document_map_free(document_map_t *map);
const char *document_url_for_id(const document_map_t *map,uint32_t docid);

uint32_t *read_postings_list(FILE *file,uint64_t offset,uint32_t *count_out);

uint32_t *and_intersect_lists(uint32_t **lists,
                              uint32_t *lengths,
                              int list_count,
                              uint32_t *answer_count);
#endif
