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


int64_t null(int64_t v0){
  int64_t v1, __swp_v1;
  int64_t v2, __swp_v2;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v1 = 0;
        v2 = v0 == v1;
        __next_BB = 1;
        break;
      case 1:
        return v2;
        break;
    }
  }
  return 0;
}

int64_t find_balance(int64_t v0, int64_t v1){
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
  int64_t v12, __swp_v12;
  int64_t v13, __swp_v13;
  int64_t v14, __swp_v14;
  int64_t v15, __swp_v15;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v3 = null(v0);
        v4 = !!v3;
        if (v4) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v2;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v5 = 0;
        __swp_v2 = v5;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 4:
        v6 = car(v0);
        v7 = car(v6);
        v8 = v7 == v1;
        v9 = !!v8;
        if (v9) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v10 = car(v0);
        v11 = cdr(v10);
        __swp_v2 = v11;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 6:
        v12 = 1;
        v13 = !!v12;
        if (v13) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        v14 = cdr(v0);
        v15 = find_balance(v14, v1);
        __swp_v2 = v15;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 8:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t update_balance(int64_t v0, int64_t v1, int64_t v2){
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int64_t v6, __swp_v6;
  int64_t v7, __swp_v7;
  int64_t v8, __swp_v8;
  int64_t v9, __swp_v9;
  int64_t v10, __swp_v10;
  int64_t v11, __swp_v11;
  int64_t v12, __swp_v12;
  int64_t v13, __swp_v13;
  int64_t v14, __swp_v14;
  int64_t v15, __swp_v15;
  int64_t v16, __swp_v16;
  int64_t v17, __swp_v17;
  int64_t v18, __swp_v18;
  int64_t v19, __swp_v19;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v4 = null(v0);
        v5 = !!v4;
        if (v5) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v3;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v6 = 0;
        __swp_v3 = v6;
        v3 = __swp_v3;
        __next_BB = 2;
        break;
      case 4:
        v7 = car(v0);
        v8 = car(v7);
        v9 = v8 == v1;
        v10 = !!v9;
        if (v10) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v11 = cons(v1, v2);
        v12 = cdr(v0);
        v13 = cons(v11, v12);
        __swp_v3 = v13;
        v3 = __swp_v3;
        __next_BB = 2;
        break;
      case 6:
        v14 = 1;
        v15 = !!v14;
        if (v15) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        v16 = car(v0);
        v17 = cdr(v0);
        v18 = update_balance(v17, v1, v2);
        v19 = cons(v16, v18);
        __swp_v3 = v19;
        v3 = __swp_v3;
        __next_BB = 2;
        break;
      case 8:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t deposit(int64_t v0, int64_t v1, int64_t v2){
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v3 = find_balance(v0, v1);
        v4 = v3 + v2;
        v5 = update_balance(v0, v1, v4);
        __next_BB = 1;
        break;
      case 1:
        return v5;
        break;
    }
  }
  return 0;
}

int64_t withdraw(int64_t v0, int64_t v1, int64_t v2){
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int64_t v6, __swp_v6;
  int64_t v7, __swp_v7;
  int64_t v8, __swp_v8;
  int64_t v9, __swp_v9;
  int64_t v10, __swp_v10;
  int64_t v11, __swp_v11;
  int64_t v12, __swp_v12;
  int64_t v13, __swp_v13;
  int64_t v14, __swp_v14;
  int64_t v15, __swp_v15;
  int64_t v16, __swp_v16;
  int64_t v17, __swp_v17;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v4 = find_balance(v0, v1);
        v5 = v4 > v2;
        v6 = !!v5;
        if (v6) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v3;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v7 = find_balance(v0, v1);
        v8 = v7 - v2;
        v9 = update_balance(v0, v1, v8);
        __swp_v3 = v9;
        v3 = __swp_v3;
        __next_BB = 2;
        break;
      case 4:
        v10 = find_balance(v0, v1);
        v11 = v10 == v2;
        v12 = !!v11;
        if (v12) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v13 = find_balance(v0, v1);
        v14 = v13 - v2;
        v15 = update_balance(v0, v1, v14);
        __swp_v3 = v15;
        v3 = __swp_v3;
        __next_BB = 2;
        break;
      case 6:
        v16 = 1;
        v17 = !!v16;
        if (v17) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        __swp_v3 = v0;
        v3 = __swp_v3;
        __next_BB = 2;
        break;
      case 8:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t transfer(int64_t v0, int64_t v1, int64_t v2, int64_t v3){
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int64_t v6, __swp_v6;
  int64_t v7, __swp_v7;
  int64_t v8, __swp_v8;
  int64_t v9, __swp_v9;
  int64_t v10, __swp_v10;
  int64_t v11, __swp_v11;
  int64_t v12, __swp_v12;
  int64_t v13, __swp_v13;
  int64_t v14, __swp_v14;
  int64_t v15, __swp_v15;
  int64_t v16, __swp_v16;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v5 = find_balance(v0, v1);
        v6 = v5 > v3;
        v7 = !!v6;
        if (v7) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v4;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v8 = withdraw(v0, v1, v3);
        v9 = deposit(v8, v2, v3);
        __swp_v4 = v9;
        v4 = __swp_v4;
        __next_BB = 2;
        break;
      case 4:
        v10 = find_balance(v0, v1);
        v11 = v10 == v3;
        v12 = !!v11;
        if (v12) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v13 = withdraw(v0, v1, v3);
        v14 = deposit(v13, v2, v3);
        __swp_v4 = v14;
        v4 = __swp_v4;
        __next_BB = 2;
        break;
      case 6:
        v15 = 1;
        v16 = !!v15;
        if (v16) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        __swp_v4 = v0;
        v4 = __swp_v4;
        __next_BB = 2;
        break;
      case 8:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t add_account(int64_t v0, int64_t v1, int64_t v2){
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v3 = cons(v1, v2);
        v4 = cons(v3, v0);
        __next_BB = 1;
        break;
      case 1:
        return v4;
        break;
    }
  }
  return 0;
}

int64_t remove_account(int64_t v0, int64_t v1){
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
  int64_t v12, __swp_v12;
  int64_t v13, __swp_v13;
  int64_t v14, __swp_v14;
  int64_t v15, __swp_v15;
  int64_t v16, __swp_v16;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v3 = null(v0);
        v4 = !!v3;
        if (v4) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
        return v2;
        break;
      case 2:
        __next_BB = 1;
        break;
      case 3:
        v5 = 0;
        __swp_v2 = v5;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 4:
        v6 = car(v0);
        v7 = car(v6);
        v8 = v7 == v1;
        v9 = !!v8;
        if (v9) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v10 = cdr(v0);
        __swp_v2 = v10;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 6:
        v11 = 1;
        v12 = !!v11;
        if (v12) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        v13 = car(v0);
        v14 = cdr(v0);
        v15 = remove_account(v14, v1);
        v16 = cons(v13, v15);
        __swp_v2 = v16;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 8:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t run_logic(int64_t v0, int64_t v1){
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
  int64_t v12, __swp_v12;
  int64_t v13, __swp_v13;
  int64_t v14, __swp_v14;
  int64_t v15, __swp_v15;
  int64_t v16, __swp_v16;
  int64_t v17, __swp_v17;
  int64_t v18, __swp_v18;
  int64_t v19, __swp_v19;
  int64_t v20, __swp_v20;
  int64_t v21, __swp_v21;
  int64_t v22, __swp_v22;
  int64_t v23, __swp_v23;
  int64_t v24, __swp_v24;
  int64_t v25, __swp_v25;
  int64_t v26, __swp_v26;
  int64_t v27, __swp_v27;
  int64_t v28, __swp_v28;
  int64_t v29, __swp_v29;
  int64_t v30, __swp_v30;
  int64_t v31, __swp_v31;
  int64_t v32, __swp_v32;
  int64_t v33, __swp_v33;
  int64_t v34, __swp_v34;
  int64_t v35, __swp_v35;
  int64_t v36, __swp_v36;
  int64_t v37, __swp_v37;
  int64_t v38, __swp_v38;
  int64_t v39, __swp_v39;
  int64_t v40, __swp_v40;
  int64_t v41, __swp_v41;
  int64_t v42, __swp_v42;
  int64_t v43, __swp_v43;
  int64_t v44, __swp_v44;
  int64_t v45, __swp_v45;
  int64_t v46, __swp_v46;
  int64_t v47, __swp_v47;
  int64_t v48, __swp_v48;
  int64_t v49, __swp_v49;
  int64_t v50, __swp_v50;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v3 = 0;
        v4 = v1 == v3;
        v5 = !!v4;
        if (v5) {
          __next_BB = 3;
      } else {
          __next_BB = 4;
      }
        break;
      case 1:
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
        v6 = 1;
        v7 = v1 == v6;
        v8 = !!v7;
        if (v8) {
          __next_BB = 5;
      } else {
          __next_BB = 6;
      }
        break;
      case 5:
        v9 = 2;
        v10 = remove_account(v0, v9);
        v11 = 0;
        v12 = run_logic(v10, v11);
        __swp_v2 = v12;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 6:
        v13 = 2;
        v14 = v1 == v13;
        v15 = !!v14;
        if (v15) {
          __next_BB = 7;
      } else {
          __next_BB = 8;
      }
        break;
      case 7:
        v16 = 1;
        v17 = 2;
        v18 = 500;
        v19 = transfer(v0, v16, v17, v18);
        v20 = 1;
        v21 = run_logic(v19, v20);
        __swp_v2 = v21;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 8:
        v22 = 3;
        v23 = v1 == v22;
        v24 = !!v23;
        if (v24) {
          __next_BB = 9;
      } else {
          __next_BB = 10;
      }
        break;
      case 9:
        v25 = 2;
        v26 = 1000;
        v27 = deposit(v0, v25, v26);
        v28 = 2;
        v29 = run_logic(v27, v28);
        __swp_v2 = v29;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 10:
        v30 = 4;
        v31 = v1 == v30;
        v32 = !!v31;
        if (v32) {
          __next_BB = 11;
      } else {
          __next_BB = 12;
      }
        break;
      case 11:
        v33 = 2;
        v34 = 0;
        v35 = add_account(v0, v33, v34);
        v36 = 3;
        v37 = run_logic(v35, v36);
        __swp_v2 = v37;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 12:
        v38 = 5;
        v39 = v1 == v38;
        v40 = !!v39;
        if (v40) {
          __next_BB = 13;
      } else {
          __next_BB = 14;
      }
        break;
      case 13:
        v41 = 0;
        v42 = 1;
        v43 = 2000;
        v44 = add_account(v41, v42, v43);
        v45 = 4;
        v46 = run_logic(v44, v45);
        __swp_v2 = v46;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 14:
        v47 = 1;
        v48 = !!v47;
        if (v48) {
          __next_BB = 15;
      } else {
          __next_BB = 16;
      }
        break;
      case 15:
        v49 = 5;
        v50 = run_logic(v0, v49);
        __swp_v2 = v50;
        v2 = __swp_v2;
        __next_BB = 2;
        break;
      case 16:
        __next_BB = 2;
        break;
    }
  }
  return 0;
}

int64_t print_pair(int64_t v0){
  int64_t v1, __swp_v1;
  int64_t v2, __swp_v2;
  int64_t v3, __swp_v3;
  int64_t v4, __swp_v4;
  int64_t v5, __swp_v5;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v1 = car(v0);
        v2 = print_int(v1);
        v3 = cdr(v0);
        v4 = print_int(v3);
        v5 = 0;
        __next_BB = 1;
        break;
      case 1:
        return v5;
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
  int64_t v6, __swp_v6;
  int __next_BB = 0;
  while (1) {
    switch (__next_BB) {
      case 0:
        v2 = 0;
        v3 = 5;
        v4 = run_logic(v2, v3);
        v5 = car(v4);
        v6 = print_pair(v5);
        __next_BB = 1;
        break;
      case 1:
        __gc_cleanup();
        return v6;
        break;
    }
  }
  return 0;
}

