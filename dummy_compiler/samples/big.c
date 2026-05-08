#include <stdio.h>
#include <stdint.h>
#include <malloc.h>


int64_t print_int(int64_t n) {return printf("%ld\n", n), n;}


struct __gcdata {int64_t item; struct __gcdata *next;};
struct __pair {int64_t _car; int64_t _cdr;};
struct __gcdata *__GC = NULL;

void __gc_add(int64_t item) {  struct __gcdata *new_GC = malloc(sizeof(struct __gcdata));
  if (!new_GC) return;
  *new_GC = (struct __gcdata){item, __GC};
  __GC = new_GC;
}
void __gc_cleanup() {
  for (struct __gcdata *ptr = __GC, *next = ptr ? ptr->next : NULL; 
       ptr; ptr = next, next = ptr ? ptr->next : NULL) {
    free((void*)(ptr->item));
    free(ptr);
  }
}

int64_t cons(int64_t _car, int64_t _cdr) {
  int64_t result = (int64_t)malloc(sizeof(struct __pair));
  if (result) {
    *(struct __pair*)result = (struct __pair){_car, _cdr};
    __gc_add(result);
  }
  return result;
}
int64_t car(int64_t pair) {return pair ? ((struct __pair*)pair)->_car : pair;}
int64_t cdr(int64_t pair) {return pair ? ((struct __pair*)pair)->_cdr : pair;}


int64_t main(int64_t v0, int64_t v1){
  int64_t v2, __swp_v2;
  int64_t v3, __swp_v3;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        break;
      case 1:
        __gc_cleanup();
        return v2;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        __swp_v2 = v0;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 4:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

