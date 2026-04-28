#include "htable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_HT_CAP 64

struct _hte
{
  char *k;
  void *val;
  _htelem *next;
};

struct _htable
{
  size_t capacity;
  size_t size;

  _htelem **elems;
};

static size_t str_hash(const void *k)
{
  const unsigned char *str = (const unsigned char *)k;
  size_t hash = 5381;
  int c;

  while ((c = *str++))
  {
    // hash = hash * 33 + c
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

static void *str_k_dup(const void *k) { return k ? strdup(k) : NULL; }

static inline size_t _ht_getindex(HTable *ht, const void *k)
{
  return str_hash(k) % ht->capacity;
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
  for (_htelem **bucket = ht->elems; (void *)bucket < (void *)(ht->elems) + new_cap; ++bucket)
    for (_htelem **e = bucket; *e; e = _move_insert_before(e, &(ht->elems[_ht_getindex(ht, (*e)->k)])))
      ;
}

HTable *ht_init(size_t init_cap)
{
  HTable *ht = malloc(sizeof(HTable));
  *ht = (HTable){
      init_cap ? init_cap : DEFAULT_HT_CAP, 0, calloc(init_cap ? init_cap : DEFAULT_HT_CAP, sizeof(_htelem))};
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
        free(c->k);
        free(c);
      }
    }
    free(ht->elems);
  }
  free(ht);
}

int ht_set(HTable *ht, const char *k, void *v)
{
  if (!ht)
    return -1;
  size_t h = _ht_getindex(ht, k);
  _htelem *e;
  for (e = ht->elems[h]; e; e = e->next)
  {
    if (!strcmp(k, e->k))
    {
      free(e->k);
      e->k = str_k_dup(k);
      e->val = v;
      return 1; // replaced duplicate key
    }
  }
  _ht_rehash(ht);
  h = _ht_getindex(ht, k);
  e = malloc(sizeof(_htelem));
  *e = (_htelem){str_k_dup(k), v, ht->elems[h]};
  ht->elems[h] = e;
  ht->size++;
  return 0;
}

int ht_get(HTable *ht, const char *k, void **dest)
{
  if (!ht)
    return 0;
  size_t h = str_hash(k) % ht->capacity;

  for (_htelem *e = ht->elems[h]; e; e = e->next)
    if (!strcmp(e->k, k))
      return *dest = e->val, 1;
  return 0;
}
