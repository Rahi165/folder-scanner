#include "crawler/parse.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>

/* -----------------------------------------------------------------------
 * link_list_t internals
 * ----------------------------------------------------------------------- */

/* Allocate a new, empty link list with a small initial capacity. */
static link_list_t *link_list_new(void)
{
    link_list_t *ll = malloc(sizeof(*ll));
    if (!ll) return NULL;

    ll->count    = 0;
    ll->capacity = 16;
    ll->urls     = malloc((size_t)ll->capacity * sizeof(char *));
    if (!ll->urls) { free(ll); return NULL; }

    return ll;
}

/* Append url to ll, doubling capacity as needed. Frees url on OOM. */
static void link_list_append(link_list_t *ll, char *url)
{
    if (ll->count == ll->capacity) {
        int    new_cap = ll->capacity * 2;
        char **tmp     = realloc(ll->urls, (size_t)new_cap * sizeof(char *));
        if (!tmp) { free(url); return; }
        ll->urls     = tmp;
        ll->capacity = new_cap;
    }
    ll->urls[ll->count++] = url;
}

/* Free all URLs stored in ll, then ll itself. No-op on NULL. */
void link_list_free(link_list_t *ll)
{
    if (!ll) return;
    for (int i = 0; i < ll->count; i++)
        free(ll->urls[i]);
    free(ll->urls);
    free(ll);
}

/* -----------------------------------------------------------------------
 * URL helpers
 * ----------------------------------------------------------------------- */

/* Return 1 if url begins with http:// or https:// (case-insensitive). */
int url_is_http(const char *url)
{
    if (!url) return 0;
    if (strncasecmp(url, "http://",  7) == 0) return 1;
    if (strncasecmp(url, "https://", 8) == 0) return 1;
    return 0;
}

/*
 * Resolve raw_url against base_url, then normalise the result:
 *   - scheme and host lowercased
 *   - fragment stripped
 *   - default ports (80/443) removed
 *
 * Non-http(s) URLs and resolution failures return NULL.
 * Caller owns the returned string.
 */
char *url_normalize(const char *raw_url, const char *base_url)
{
    if (!raw_url || raw_url[0] == '\0') return NULL;

    /*
     * Reject non-http schemes (javascript:, mailto:, data:, tel:, …).
     * A colon appearing before the first slash signals a scheme prefix.
     */
    const char *colon = strchr(raw_url, ':');
    const char *slash = strchr(raw_url, '/');
    if (colon && (!slash || colon < slash)) {
        if (!url_is_http(raw_url)) return NULL;
    }

    /* Resolve the (possibly relative) reference against the base URL. */
    xmlChar *base = xmlCharStrdup(base_url ? base_url : "");
    xmlChar *href = xmlCharStrdup(raw_url);
    xmlChar *abs  = xmlBuildURI(href, base);
    xmlFree(href);
    xmlFree(base);

    if (!abs) return NULL;

    if (!url_is_http((char *)abs)) {
        xmlFree(abs);
        return NULL;
    }

    /* Parse into components for normalisation. */
    xmlURIPtr uri = xmlParseURI((char *)abs);
    xmlFree(abs);
    if (!uri) return NULL;

    /* Lowercase scheme and host. */
    if (uri->scheme)
        for (char *p = uri->scheme; *p; p++)
            *p = (char)tolower((unsigned char)*p);

    if (uri->server)
        for (char *p = uri->server; *p; p++)
            *p = (char)tolower((unsigned char)*p);

    /* Drop the fragment — it is irrelevant for crawling. */
    if (uri->fragment) {
        xmlFree(uri->fragment);
        uri->fragment = NULL;
    }

    /* Remove well-known default ports to canonicalise the URL. */
    if (uri->port != 0) {
        if ((strcmp(uri->scheme, "http")  == 0 && uri->port == 80) ||
            (strcmp(uri->scheme, "https") == 0 && uri->port == 443))
            uri->port = 0;
    }

    /* Serialise back to a string and return a plain-char copy. */
    char *serialised = (char *)xmlSaveUri(uri);
    xmlFreeURI(uri);
    if (!serialised) return NULL;

    char *out = strdup(serialised);
    xmlFree(serialised);
    return out;
}

/* -----------------------------------------------------------------------
 * DOM traversal
 * ----------------------------------------------------------------------- */

/*
 * Recursively walk the libxml2 node tree rooted at node. For every <a>
 * element found, resolve and normalise its href against base_url and
 * append the result to out.
 */
static void walk_nodes(xmlNode *node, const char *base_url, link_list_t *out)
{
    for (xmlNode *cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcasecmp(cur->name, (const xmlChar *)"a") == 0) {

            xmlChar *href = xmlGetProp(cur, (const xmlChar *)"href");
            if (href) {
                char *norm = url_normalize((char *)href, base_url);
                if (norm) link_list_append(out, norm);
                xmlFree(href);
            }
        }

        if (cur->children)
            walk_nodes(cur->children, base_url, out);
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/*
 * Parse html (html_len bytes) as an HTML document and extract every
 * hyperlink, resolved and normalised relative to base_url.
 *
 * Returns a heap-allocated link_list_t (possibly empty) on success, or
 * NULL on allocation failure. Caller must call link_list_free() on the
 * result.
 */
link_list_t *parse_extract_links(const char *html, size_t html_len,
                                  const char *base_url)
{
    link_list_t *ll = link_list_new();
    if (!ll) return NULL;

    if (!html || html_len == 0) return ll;

    int options = HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR |
                  HTML_PARSE_RECOVER   | HTML_PARSE_NONET;

    htmlDocPtr doc = htmlReadMemory(html, (int)html_len,
                                    base_url, "UTF-8", options);
    if (!doc) return ll;

    xmlNode *root = xmlDocGetRootElement(doc);
    if (root) walk_nodes(root, base_url, ll);

    xmlFreeDoc(doc);
    return ll;
}