#ifndef AST_D
#define AST_D

#include <stdio.h>
#include <string.h>

#define CAR(x) ((x)->lchild)
#define CDR(x) ((x)->rchild)
#define FIRST(x) CAR(x)
#define SECOND(x) CAR(CDR(x))
#define THIRD(x) CAR(CDR(CDR(x)))

#define CONS(a, b) N_AST(SEXP, 0, NULL, a, b)

#define GET_OP(x) (CAR(x)->value)

#define DEFUN_GET_ID(x) (SECOND(x)->value)

#define STR_MATCH(val)                      \
  const char *_str_switch_internal = (val); \
  if (0)                                    \
  {
#define STR_CASE(pattern)                          \
  }                                                \
  else if (!strcmp(pattern, _str_switch_internal)) \
  {
#define STR_DEFAULT \
  }                 \
  else              \
  {
#define STR_MATCH_END }

typedef enum
{
  PROGRAM,
  CODE_BLOCK,
  SEXP,
  LIST,
  NAME,
  CONST_NUM,
  CONST_STR  
} AST_T;

typedef struct _ast
{
  AST_T type;
  
  int value_len;
  char *value;

  struct _ast *lchild;
  struct _ast *rchild;
} AST;


AST *N_AST(AST_T type, int value_len, const char *value, AST *lchild, AST *rchild);

AST *copy_AST(const AST *tree);

int list_len(AST *list);

void D_AST(AST *node);

void print_AST(AST *node, FILE *fp);
void dump_AST(AST *node, FILE *fp);

#endif // AST_D
