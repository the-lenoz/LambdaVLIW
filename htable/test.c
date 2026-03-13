#include "htable.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

size_t hash(const void *i)
{
  return (size_t)i;
}
int is_eq(const void *a, const void *b) { return a == b; }
void ifree(HTable *ht, void *k, void *v) {}
void *kdup(const void *k) { return (void *)(const size_t)k; }
void *vdup(const void *v) { return (void *)123; }

int main(void)
{
  HTable *ht = ht_init(hash, is_eq, kdup, vdup, ifree, 0);

  size_t val = 0;
  int status = 0;

  status = ht_get(ht, (void *)1, (void **)&val);
  printf("status: %d, data: %zu\n", status, val);

  status = ht_set(ht, (void *)1, (void *)123);
  printf("status: %d\n", status);

  status = ht_get(ht, (void *)1, (void **)&val);
  printf("status: %d, data: %zu\n", status, val);

  ht_destroy(ht);
  
  return 0;
}
