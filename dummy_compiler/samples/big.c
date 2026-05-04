#include <stdio.h>
#include <stdint.h>


int64_t print_int(int64_t n) {return printf("%ld\n", n), n;}

int64_t main(int64_t v0, int64_t v1){
  int64_t v2, __swp_v2;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v2 = print_int(v0);
        __next_BB = 1;
        break;
      case 1:
        return v2;
        break;
    }
  }
  return 0;
}

