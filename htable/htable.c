#include "htable.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_HT_CAP 64

struct _hte
{
  void *k;
  void *v;
  _htelem *next;
};

struct _htable
{
  hashfunc hf;
  cmpfunc cf;
  keydupfunc kdf;
  valdupfunc vdf;
  freefunc ff;

  size_t capacity;
  size_t size;

  _htelem **elems;
};

static inline size_t _ht_getindex(HTable*ht, const void *k)
{
  return ht->hf(k) % ht->capacity;
}    

static _htelem **_move_insert_before(_htelem **elem, _htelem **dst_elem)
{
  if (*elem == *dst_elem)
    return &(*elem)->next;

  _htelem *next = (*elem)->next;

  (*elem)->next = *dst_elem;
  *dst_elem = *elem;
  *elem = next;
  return elem;
}    

static void _ht_rehash(HTable *ht)
{
  if (!ht || ht->size * 10 < ht->capacity * 7)
    return;

  size_t new_cap = ht->capacity * 2 * sizeof(_htelem *);

  ht->elems = realloc(ht->elems, new_cap);
  memset(ht->elems + ht->capacity, 0, ht->capacity * sizeof(_htelem *));

  ht->capacity *= 2;
  for (_htelem **bucket = ht->elems; (void*)bucket < (void*)(ht->elems) + new_cap; ++bucket)
    for (_htelem **e = bucket; *e; e = _move_insert_before(e, &(ht->elems[_ht_getindex(ht, (*e)->k)])))
      ;
}

HTable *ht_init(hashfunc hf, cmpfunc cf, keydupfunc kdf, valdupfunc vdf, freefunc ff, size_t init_cap)
{
  HTable *ht = malloc(sizeof(HTable));
  *ht = (HTable){
      hf, cf, kdf, vdf, ff, init_cap ? init_cap : DEFAULT_HT_CAP, 0, calloc(init_cap ? init_cap : DEFAULT_HT_CAP, sizeof(_htelem))};
  return ht;
}
void ht_destroy(HTable *ht)
{
  if (!ht)
    return;

  if (ht->elems)
  {
    _htelem *c, *n = NULL;
    for (size_t i = 0; i < ht->capacity; ++i)
    {
      for (c = ht->elems[i]; c; c = n)
      {
        n = c->next;
        ht->ff(ht, c->k, c->v);
        free(c);
      }
    }
    free(ht->elems);
  }
  free(ht);
}

int ht_set(HTable *ht, const void *k, void *v)
{
  if (!ht)
    return -1;
  size_t h = _ht_getindex(ht, k);
  _htelem *e;
  for (e = ht->elems[h]; e; e = e->next)
  {
    if (ht->cf(k, e->k))
    {
      ht->ff(ht, e->k, e->v);
      e->k = ht->kdf(k);
      e->v = ht->vdf(v);
      return 1; // replaced duplicate key
    }
  }
  _ht_rehash(ht);
  h = _ht_getindex(ht, k);
  e = malloc(sizeof(_htelem));
  *e = (_htelem){ht->kdf(k), ht->vdf(v), ht->elems[h]};
  ht->elems[h] = e;
  ht->size++;
  return 0;
}

int ht_get(HTable *ht, const void *k, void **dest)
{
  if (!ht)
    return 0;
  size_t h = ht->hf(k) % ht->capacity;

  for (_htelem *e = ht->elems[h]; e; e = e->next)
    if (ht->cf(k, e->k))
      return *dest = e->v, 1;
  return 0;
}
