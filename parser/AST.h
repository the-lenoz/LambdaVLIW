#ifndef AST_D
#define AST_D

#include <stdio.h>

#define CAR(x) ((x)->lchild)
#define CDR(x) ((x)->rchild)
#define FIRST(x) CAR(x)
#define SECOND(x) CAR(CDR(x))
#define THIRD(x) CAR(CDR(CDR(x)))


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

void D_AST(AST *node);

void print_AST(AST *node, FILE *fp);

#endif // AST_D
