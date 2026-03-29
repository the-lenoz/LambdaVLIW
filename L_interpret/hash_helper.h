#ifndef HASH_HELPER_D
#define HASH_HELPER_D

#include "../htable/htable.h"
#include <stdlib.h>
#include <string.h>

static size_t hash(const void *k)
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

static int str_eq(const void *a, const void *b)
{
  return !strcmp((const char *)a, (const char *)b);
}

static void free_str(HTable *ht, void *k, void *v) { free(k); }
static void *k_dup(const void *k) { return strdup((const char *)k); }
static void *v_dup(const void *v) { return (void *)(size_t)v; }

#endif // HASH_HELPER_D
