#include "query/index_reader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int compare_dictionary_entries(const void *left,const void *right){
    const dictionary_entry_t *a=left;
    const dictionary_entry_t *b=right;
    return strcmp(a->word,b->word);
}

int dictionary_load(dictionary_t *dict,const char *path){
    FILE *file;
    char *line=NULL;
    size_t line_size=0;
    ssize_t read_count;
    size_t capacity=128;

    if(!dict||!path) return -1;
    dict->items=NULL; dict->size=0;

    file=fopen(path,"r");
    if(!file){ perror(path); return -1; }

    dict->items=malloc(capacity*sizeof(dictionary_entry_t));
    if(!dict->items){ fclose(file); return -1; }

    while((read_count=getline(&line,&line_size,file))!=-1){
        char *save=NULL;
        char *term,*off,*df;
        dictionary_entry_t *entry;

        if(read_count>0 && line[read_count-1]=='\n')
            line[read_count-1]='\0';

        term=strtok_r(line,"\t",&save);
        off=strtok_r(NULL,"\t",&save);
        df=strtok_r(NULL,"\t",&save);

        if(!term||!off||!df) continue;

        if(dict->size==capacity){
            capacity*=2;
            void *tmp=realloc(dict->items,
                    capacity*sizeof(dictionary_entry_t));
            if(!tmp){
                free(line); fclose(file);
                dictionary_free(dict);
                return -1;
            }
            dict->items=tmp;
        }

        entry=&dict->items[dict->size];
        entry->word=strdup(term);
        entry->postings_offset=strtoull(off,NULL,10);
        entry->document_count=(uint32_t)strtoul(df,NULL,10);

        if(entry->word) dict->size++;
    }

    free(line);
    fclose(file);

    qsort(dict->items,dict->size,sizeof(dictionary_entry_t),
          compare_dictionary_entries);
    return 0;
}

void dictionary_free(dictionary_t *dict){
    if(!dict||!dict->items) return;
    for(size_t i=0;i<dict->size;i++)
        free(dict->items[i].word);
    free(dict->items);
}

int dictionary_find(const dictionary_t *dict,const char *word){
    dictionary_entry_t key;
    dictionary_entry_t *found;

    if(!dict||!word||!dict->items) return -1;

    key.word=(char*)word;
    key.postings_offset=0;
    key.document_count=0;

    found=bsearch(&key,
                  dict->items,
                  dict->size,
                  sizeof(dictionary_entry_t),
                  compare_dictionary_entries);

    return found ? (int)(found-dict->items) : -1;
}

static int compare_docs(const void *a,const void *b){
    uint32_t x=((document_entry_t*)a)->docid;
    uint32_t y=((document_entry_t*)b)->docid;
    return (x>y)-(x<y);
}

int document_map_load(document_map_t *map,const char *path){
    FILE *file;
    char *line=NULL;
    size_t line_size=0;
    ssize_t read_count;
    size_t capacity=128;

    if(!map||!path) return -1;
    map->items=NULL; map->size=0;

    file=fopen(path,"r");
    if(!file){ perror(path); return -1; }

    map->items=malloc(capacity*sizeof(document_entry_t));
    if(!map->items){ fclose(file); return -1; }

    while((read_count=getline(&line,&line_size,file))!=-1){
        char *save=NULL,*id,*url,*fp,*depth;
        document_entry_t *e;

        if(read_count>0 && line[read_count-1]=='\n')
            line[read_count-1]='\0';

        id=strtok_r(line,"\t",&save);
        url=strtok_r(NULL,"\t",&save);
        fp=strtok_r(NULL,"\t",&save);
        depth=strtok_r(NULL,"\t",&save);

        if(!id||!url) continue;

        if(map->size==capacity){
            capacity*=2;
            void *tmp=realloc(map->items,
                    capacity*sizeof(document_entry_t));
            if(!tmp){
                free(line); fclose(file);
                document_map_free(map);
                return -1;
            }
            map->items=tmp;
        }

        e=&map->items[map->size++];
        e->docid=(uint32_t)strtoul(id,NULL,10);
        e->url=strdup(url);
        e->path=strdup(fp?fp:"");
        e->depth=depth?(uint16_t)strtoul(depth,NULL,10):0;
    }

    free(line);
    fclose(file);

    qsort(map->items,map->size,sizeof(document_entry_t),compare_docs);
    return 0;
}

void document_map_free(document_map_t *map){
    if(!map||!map->items) return;
    for(size_t i=0;i<map->size;i++){
        free(map->items[i].url);
        free(map->items[i].path);
    }
    free(map->items);
}

const char *document_url_for_id(const document_map_t *map,uint32_t docid){
    document_entry_t key={docid,NULL,NULL,0};
    document_entry_t *found;

    if(!map||!map->items) return NULL;

    found=bsearch(&key,map->items,map->size,
                  sizeof(document_entry_t),compare_docs);

    return found ? found->url : NULL;
}

uint32_t *read_postings_list(FILE *file,uint64_t offset,uint32_t *count_out){
    uint32_t count;
    uint32_t *items;

    *count_out=0;
    if(fseeko(file,(off_t)offset,SEEK_SET)!=0) return NULL;
    if(fread(&count,sizeof(uint32_t),1,file)!=1) return NULL;
    if(count==0) return NULL;

    items=malloc(count*sizeof(uint32_t));
    if(!items) return NULL;

    if(fread(items,sizeof(uint32_t),count,file)!=count){
        free(items);
        return NULL;
    }

    *count_out=count;
    return items;
}

static uint32_t *intersect_two(
    const uint32_t *a,uint32_t an,
    const uint32_t *b,uint32_t bn,
    uint32_t *outn)
{
    uint32_t *r=malloc(an*sizeof(uint32_t));
    uint32_t i=0,j=0,k=0;
    if(!r){ *outn=0; return NULL; }

    while(i<an && j<bn){
        if(a[i]==b[j]) r[k++]=a[i],i++,j++;
        else if(a[i]<b[j]) i++;
        else j++;
    }

    *outn=k;
    if(!k){ free(r); return NULL; }
    return r;
}

uint32_t *and_intersect_lists(uint32_t **lists,
                              uint32_t *lengths,
                              int list_count,
                              uint32_t *answer_count)
{
    if(!answer_count){ return NULL; }
    *answer_count=0;
    if(list_count<=0) return NULL;

    uint32_t current_len=lengths[0];
    uint32_t *current=malloc(current_len*sizeof(uint32_t));
    memcpy(current,lists[0],current_len*sizeof(uint32_t));

    for(int i=1;i<list_count;i++){
        uint32_t next_len=0;
        uint32_t *next=
            intersect_two(current,current_len,
                          lists[i],lengths[i],
                          &next_len);
        free(current);
        current=next;
        current_len=next_len;

        if(!current) return NULL;
    }

    *answer_count=current_len;
    return current;
}
