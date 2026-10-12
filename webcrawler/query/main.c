#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include "query/index_reader.h"

#define QUERY_MAX_TERMS 64

static void usage(const char *p){
    fprintf(stderr,
      "Usage: %s --index <dir> term1 [term2 ...]\n",p);
}

static void lowercase(char *s){
    while(*s){
        *s=(char)tolower((unsigned char)*s);
        s++;
    }
}

int main(int argc,char **argv){
    char *base=NULL;
    int c;

    static struct option opts[]={
      {"index",required_argument,NULL,'i'},
      {"help",no_argument,NULL,'h'},
      {0,0,0,0}
    };

    while((c=getopt_long(argc,argv,"i:h",opts,NULL))!=-1){
        if(c=='i') base=optarg;
        else{
            usage(argv[0]);
            return c=='h'?0:1;
        }
    }

    if(!base){
        usage(argv[0]);
        return 1;
    }

    int term_count=argc-optind;
    if(term_count<1){
        usage(argv[0]);
        return 1;
    }

    for(int i=optind;i<argc;i++)
        lowercase(argv[i]);

    char docs[4096],dict[4096],post[4096];

    snprintf(docs,sizeof docs,"%s/index/docs.tsv",base);
    snprintf(dict,sizeof dict,"%s/index/dict.tsv",base);
    snprintf(post,sizeof post,"%s/index/postings.bin",base);

    dictionary_t D;
    document_map_t M;

    if(dictionary_load(&D,dict)) return 1;
    if(document_map_load(&M,docs)) return 1;

    FILE *pf=fopen(post,"rb");
    if(!pf){
        perror(post);
        return 1;
    }

    uint32_t *lists[QUERY_MAX_TERMS];
    uint32_t lens[QUERY_MAX_TERMS];
    int loaded=0;

    for(int i=optind;i<argc;i++){
        int idx=dictionary_find(&D,argv[i]);

        if(idx<0){
            printf("No documents matched all terms.\n");
            return 0;
        }

        uint32_t n=0;
        uint32_t *L=
            read_postings_list(
                pf,
                D.items[idx].postings_offset,
                &n);

        if(!L){
            printf("No documents matched all terms.\n");
            return 0;
        }

        lists[loaded]=L;
        lens[loaded]=n;
        loaded++;
    }

    uint32_t hits=0;
    uint32_t *results=
        and_intersect_lists(
            lists,lens,loaded,&hits);

    for(int i=0;i<loaded;i++)
        free(lists[i]);

    fclose(pf);

    if(!results||hits==0){
        printf("No documents matched.\n");
    }else{
        printf("Found %" PRIu32 " matches:\n",hits);

        for(uint32_t i=0;i<hits;i++){
            printf("%" PRIu32 " %s\n",
                results[i],
                document_url_for_id(&M,results[i]));
        }
    }

    free(results);
    dictionary_free(&D);
    document_map_free(&M);
    return 0;
}
