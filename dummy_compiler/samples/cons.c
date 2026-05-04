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


int64_t range(int64_t v0, int64_t v1){
  int64_t v2, __swp_v2;
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int64_t v6, __swp_v6;
  int64_t v7, __swp_v7;
  int64_t v8, __swp_v8;
  int64_t v9, __swp_v9;
  int64_t v10, __swp_v10;
  int64_t v11, __swp_v11;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v2 = v0 == v1;
        if (v2) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v11;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v3 = 0;
        __swp_v11 = v3;
        v11 = __swp_v11;
        __next_BB = 2;
        break;
      case 4:
        v4 = v0 > v1;
        if (v4) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v5 = 0;
        __swp_v11 = v5;
        v11 = __swp_v11;
        __next_BB = 2;
        break;
      case 6:
        v6 = 1;
        if (v6) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        v7 = 1;
        v8 = v0 + v7;
        v9 = range(v8, v1);
        v10 = cons(v0, v9);
        __swp_v11 = v10;
        v11 = __swp_v11;
        __next_BB = 2;
        break;
      case 8:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t print_list(int64_t v0){
  int64_t v1, __swp_v1;
  int64_t v2, __swp_v2;
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int64_t v6, __swp_v6;
  int64_t v7, __swp_v7;
  int64_t v8, __swp_v8;
  int64_t v9, __swp_v9;
  int64_t v10, __swp_v10;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v1 = 0;
        v2 = v0 == v1;
        if (v2) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v10;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v3 = 0;
        __swp_v10 = v3;
        v10 = __swp_v10;
        __next_BB = 2;
        break;
      case 4:
        v4 = 1;
        if (v4) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v5 = car(v0);
        v6 = print_int(v5);
        v7 = cdr(v0);
        v8 = print_list(v7);
        v9 = 0;
        __swp_v10 = v9;
        v10 = __swp_v10;
        __next_BB = 2;
        break;
      case 6:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t main(int64_t v0, int64_t v1){
  int64_t v2, __swp_v2;
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v2 = 0;
        v3 = 100;
        v4 = range(v2, v3);
        v5 = print_list(v4);
        __next_BB = 1;
        break;
      case 1:
        __gc_cleanup();
        return v5;
        break;
    }
  }
  return 0;
}

