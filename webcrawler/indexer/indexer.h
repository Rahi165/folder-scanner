#ifndef INDEX_H
#define INDEX_H

/* In-memory inverted index and document map.
 * Loads previous index if present and flushes atomically.
 * On-disk: docs.tsv, dict.tsv, postings.bin.
 */

#include <stdint.h>
#include <stddef.h>
#include "indexer/tokenizer.h"

#define INDEX_FORMAT_VERSION 1          // index file format version //
#define INDEX_INIT_CAPACITY (1u << 17)  // initial hash table size   //
#define INDEX_LOAD_THRESHOLD 0.65       // resize when load exceeded //

typedef struct {
    uint32_t *docids;   // sorted doc IDs for this term //
    uint32_t  count;    // number of doc IDs stored     //
    uint32_t  capacity; // allocated size of docids[]   //
} postings_t;

typedef struct {
    char      *term;    // term string (NULL = empty) //
    postings_t postings;   // postings list for the term   //
} index_entry_t;

typedef struct {
    index_entry_t *table;     // hash table array //
    size_t         capacity;     // number of slots //
    size_t         count;      // number of used entries //
} inverted_index_t;

typedef struct {
    uint32_t  docid;     // numeric document ID //
    char     *url;       // original URL                //
    char     *filepath;  // path to stored file         //
    uint16_t  depth;     // crawl depth                 //
} doc_entry_t;

typedef struct {
    doc_entry_t *entries; // dynamic array of docs      //
    size_t       count;   // number of docs             //
    size_t       capacity;// allocated size             //
} doc_map_t;

typedef void (*indexer_log_fn)(const char *msg); // logging callback //

typedef struct {
    double load_factor;     // max load before resize    //
    size_t initial_capacity;// starting hash size        //
    int    enable_stemming; // tokenizer option          //
} indexer_config_t;

typedef struct {
    inverted_index_t inv;   // main inverted index       //
    doc_map_t        docs;  // document metadata         //
    char            *out_dir; // output directory        //
    indexer_log_fn   logger;  // optional logger         //
} indexer_ctx_t;

// Initialize indexer with defaults; loads existing index if present. //
int indexer_init(indexer_ctx_t *ctx, const char *out_dir);

// Initialize indexer with custom configuration. //
int indexer_init_with_config(indexer_ctx_t *ctx,
                             const char *out_dir,
                             const indexer_config_t *cfg);

// Set a logging callback for debug/info messages. //
void indexer_set_logger(indexer_ctx_t *ctx, indexer_log_fn fn);

// Add a document and all its tokens into the index. //
void indexer_add_document(indexer_ctx_t *ctx,
                          uint32_t docid,
                          uint16_t depth,
                          const char *url,
                          const char *filepath,
                          token_list_t *tokens);

// Find or insert a term in the hash table. //
index_entry_t *index_lookup_or_insert(inverted_index_t *inv,
                                      const char *term);

// Append a docid to a postings list (grows if needed). //
void postings_add(postings_t *p, uint32_t docid);

// Retrieve postings list for a term (NULL if missing). //
const postings_t *indexer_get_postings(indexer_ctx_t *ctx,
                                       const char *term);

// Iterate over all terms in the index. //
typedef void (*index_iter_fn)(const index_entry_t *e, void *ud);
void indexer_iterate_terms(indexer_ctx_t *ctx,
                           index_iter_fn fn,
                           void *userdata);

// Return approximate memory usage of the entire index. //
size_t indexer_memory_usage(const indexer_ctx_t *ctx);

// Write docs.tsv, dict.tsv, and postings.bin to disk. //
int indexer_flush(indexer_ctx_t *ctx);

// Free all allocated memory (not ctx itself). //
void indexer_destroy(indexer_ctx_t *ctx);

#endif /* INDEX_H */
