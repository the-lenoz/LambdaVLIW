#include <stdio.h>


int fib_recursive(int v0, int v1, int v2){
  int v3, __swp_v3;
  int v4, __swp_v4;
  int v5, __swp_v5;
  int v6, __swp_v6;
  int v7, __swp_v7;
  int v8, __swp_v8;
  int v9, __swp_v9;
  int v10, __swp_v10;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v3 = 0;
        v4 = v0 == v3;
        if (v4) {
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
        __swp_v10 = v1;
        v10 = __swp_v10;
        __next_BB = 2;
        break;
      case 4:
        v5 = 1;
        if (v5) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v6 = 1;
        v7 = v0 - v6;
        v8 = v1 + v2;
        v9 = fib_recursive(v7, v2, v8);
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

int calculate_fib(int v0){
  int v1, __swp_v1;
  int v2, __swp_v2;
  int v3, __swp_v3;
  int v4, __swp_v4;
  int v5, __swp_v5;
  int v6, __swp_v6;
  int v7, __swp_v7;
  int v8, __swp_v8;
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
        return v8;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v3 = 0;
        __swp_v8 = v3;
        v8 = __swp_v8;
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
        v5 = 0;
        v6 = 1;
        v7 = fib_recursive(v0, v5, v6);
        __swp_v8 = v7;
        v8 = __swp_v8;
        __next_BB = 2;
        break;
      case 6:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int calculate_fib_naive(int v0){
  int v1, __swp_v1;
  int v2, __swp_v2;
  int v3, __swp_v3;
  int v4, __swp_v4;
  int v5, __swp_v5;
  int v6, __swp_v6;
  int v7, __swp_v7;
  int v8, __swp_v8;
  int v9, __swp_v9;
  int v10, __swp_v10;
  int v11, __swp_v11;
  int v12, __swp_v12;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v1 = 3;
        v2 = v0 < v1;
        if (v2) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v12;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v3 = 1;
        __swp_v12 = v3;
        v12 = __swp_v12;
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
        v5 = 1;
        v6 = v0 - v5;
        v7 = calculate_fib_naive(v6);
        v8 = 2;
        v9 = v0 - v8;
        v10 = calculate_fib_naive(v9);
        v11 = v7 + v10;
        __swp_v12 = v11;
        v12 = __swp_v12;
        __next_BB = 2;
        break;
      case 6:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int sum_fib_range(int v0, int v1){
  int v2, __swp_v2;
  int v3, __swp_v3;
  int v4, __swp_v4;
  int v5, __swp_v5;
  int v6, __swp_v6;
  int v7, __swp_v7;
  int v8, __swp_v8;
  int v9, __swp_v9;
  int v10, __swp_v10;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v2 = 0;
        v3 = v0 == v2;
        if (v3) {
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
        __swp_v10 = v1;
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
        v5 = 1;
        v6 = v0 - v5;
        v7 = calculate_fib(v0);
        v8 = v1 + v7;
        v9 = sum_fib_range(v6, v8);
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

int main(){
  int v0, __swp_v0;
  int v1, __swp_v1;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v0 = 32;
        v1 = calculate_fib_naive(v0);
        __next_BB = 1;
        break;
      case 1:
        return v1;
        break;
    }
  }
  return 0;
}

