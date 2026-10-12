#include "tokenizer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int starts_with_ignore_case(const char *s, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

static int append_token(token_list_t *list, const char *word) {
    char *copy;

    if (list == NULL || word == NULL) {
        return 0;
    }

    if (list->size == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 32 : list->capacity * 2;
        char **new_tokens = realloc(list->tokens, new_capacity * sizeof(char *));
        if (new_tokens == NULL) {
            return 0;
        }
        list->tokens = new_tokens;
        list->capacity = new_capacity;
    }

    copy = strdup(word);
    if (copy == NULL) {
        return 0;
    }

    list->tokens[list->size++] = copy;
    return 1;
}

static char decode_entity(const char *s, size_t *consumed) {
    if (strncmp(s, "&amp;", 5) == 0) {
        *consumed = 5;
        return '&';
    }
    if (strncmp(s, "&lt;", 4) == 0) {
        *consumed = 4;
        return '<';
    }
    if (strncmp(s, "&gt;", 4) == 0) {
        *consumed = 4;
        return '>';
    }
    if (strncmp(s, "&nbsp;", 6) == 0) {
        *consumed = 6;
        return ' ';
    }

    *consumed = 1;
    return *s;
}

static void emit_word(token_list_t *list, char *buffer, size_t *length) {
    if (*length < TOKEN_MIN_LEN || *length >= TOKEN_MAX_LEN) {
        *length = 0;
        return;
    }

    buffer[*length] = '\0';

    if (!is_stopword(buffer)) {
        append_token(list, buffer);
    }

    *length = 0;
}

int is_stopword(const char *word) {
    static const char *stopwords[] = {
        "a", "an", "and", "are", "as", "at", "be", "but", "by",
        "for", "from", "had", "has", "have", "he", "her", "his",
        "i", "if", "in", "is", "it", "its", "of", "on", "or",
        "she", "that", "the", "their", "them", "they", "this",
        "to", "was", "we", "were", "will", "with", "you"
    };

    size_t count = sizeof(stopwords) / sizeof(stopwords[0]);
    size_t i;

    if (word == NULL) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (strcmp(word, stopwords[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

token_list_t *tokenize_html(const char *html, size_t len) {
    token_list_t *list;
    char current[TOKEN_MAX_LEN];
    size_t word_len = 0;
    size_t i = 0;
    int inside_tag = 0;
    int inside_script = 0;
    int inside_style = 0;

    if (html == NULL) {
        return NULL;
    }

    list = calloc(1, sizeof(token_list_t));
    if (list == NULL) {
        return NULL;
    }

    while (i < len) {
        if (!inside_tag && html[i] == '<') {
            inside_tag = 1;

            if (i + 7 < len && starts_with_ignore_case(html + i, "<script")) {
                inside_script = 1;
            } else if (i + 6 < len && starts_with_ignore_case(html + i, "<style")) {
                inside_style = 1;
            } else if (inside_script && i + 9 < len && starts_with_ignore_case(html + i, "</script")) {
                inside_script = 0;
            } else if (inside_style && i + 8 < len && starts_with_ignore_case(html + i, "</style")) {
                inside_style = 0;
            }

            emit_word(list, current, &word_len);
            i++;
            continue;
        }

        if (inside_tag) {
            if (html[i] == '>') {
                inside_tag = 0;
            }
            i++;
            continue;
        }

        if (inside_script || inside_style) {
            i++;
            continue;
        }

        if (html[i] == '&') {
            size_t consumed = 0;
            char decoded = decode_entity(html + i, &consumed);

            if (isalpha((unsigned char)decoded)) {
                if (word_len < TOKEN_MAX_LEN - 1) {
                    current[word_len++] = (char)tolower((unsigned char)decoded);
                }
            } else {
                emit_word(list, current, &word_len);
            }

            i += consumed;
            continue;
        }

        if (isalpha((unsigned char)html[i])) {
            if (word_len < TOKEN_MAX_LEN - 1) {
                current[word_len++] = (char)tolower((unsigned char)html[i]);
            }
        } else {
            emit_word(list, current, &word_len);
        }

        i++;
    }

    emit_word(list, current, &word_len);
    return list;
}

void token_list_free(token_list_t *list) {
    size_t i;

    if (list == NULL) {
        return;
    }

    for (i = 0; i < list->size; i++) {
        free(list->tokens[i]);
    }

    free(list->tokens);
    free(list);
}