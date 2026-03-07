#include "AST.h"
#include <malloc.h>
#include <string.h>

AST *N_AST(AST_T type, int value_len, const char *value, AST *lchild, AST *rchild)
{
  AST *result = malloc(sizeof(AST));
  if (!result)
    return NULL;
  *result = (AST){type, value_len, memcpy(malloc(value_len), value, value_len), lchild, rchild};
  return result;
}

void D_AST(AST *node)
{
  if (!node)
    return;
  
  if (node->value && node->value_len)
    free(node->value);
  
  D_AST(node->lchild);
  D_AST(node->rchild);

  *node = (AST){};
  free(node);
}
