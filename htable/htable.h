#ifndef HTABLE_D
#define HTABLE_D

#include <stddef.h>
typedef struct _hte _htelem;

typedef struct _htable HTable;

typedef size_t (*hashfunc)(const void *key);
typedef int (*cmpfunc)(const void *a_key, const void *b_key);
typedef void (*freefunc)(HTable *ht, void *e_key, void *e_val);
typedef void* (*keydupfunc)(const void *key);
typedef void* (*valdupfunc)(const void *val);


HTable *ht_init(hashfunc hf, cmpfunc cf, keydupfunc kdf, valdupfunc vdf, freefunc ff, size_t init_cap);
void ht_destroy(HTable *ht);

int ht_set(HTable *ht, const void *k, void *v);
int ht_get(HTable *ht, const void *k, void**dest);


#endif // HTABLE_H
