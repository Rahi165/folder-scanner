#ifndef TOKENIZER_H
#define TOKENIZER_H

/*
 * tokenizer.h
 *
 * Converts HTML into a cleaned list of words for indexing.
 * Main behavior:
 *   - removes HTML tags
 *   - skips script/style sections
 *   - decodes a few common HTML entities
 *   - lowercases tokens
 *   - filters by token length
 *   - removes common stopwords
 */

#include <stddef.h>

#define TOKEN_MIN_LEN 2
#define TOKEN_MAX_LEN 128

typedef struct {
    char **tokens;
    size_t size;
    size_t capacity;
} token_list_t;

/*
 * Parse an HTML buffer and return a heap-allocated token list.
 * Caller must free the result with token_list_free().
 * Returns NULL on allocation failure.
 */
token_list_t *tokenize_html(const char *html, size_t len);

/* Returns 1 if the word is a stopword, 0 otherwise. */
int is_stopword(const char *word);

/* Free the token list and all token strings inside it. */
void token_list_free(token_list_t *list);

#endif /* TOKENIZER_H */