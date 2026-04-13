#include "AST.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

AST *N_AST(AST_T type, int value_len, const char *value, AST *lchild, AST *rchild)
{
  AST *result = malloc(sizeof(AST));
  if (!result)
    return NULL;
  *result = (AST){type, value_len, value ? strndup(value, value_len) : NULL, lchild, rchild};
  return result;
}

AST *copy_AST(const AST *tree)
{
  if (!tree)
    return NULL;
  return N_AST(tree->type, tree->value_len, tree->value, copy_AST(tree->lchild), copy_AST(tree->rchild));
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

int list_len(AST *list)
{
  if (!list)
    return 0;
  return 1 + list_len(CDR(list));
}

static void print_node(AST *node, FILE *fp, int depth)
{
  for (int i = 0; i < depth; ++i)
    putc(' ', fp);
  if (!node)
  {
    fprintf(fp, "nil");
    return;
  }
  putc('(', fp);
  switch (node->type)
  {
  case PROGRAM:
    fprintf(fp, "program");
    break;
  case CODE_BLOCK:
    fprintf(fp, "code_block");
    break;
  case SEXP:
    fprintf(fp, "S_exp");
    break;
  case LIST:
    fprintf(fp, "list");
    break;
  case NAME:
    fprintf(fp, "name");
    break;
  case CONST_NUM:
    fprintf(fp, "const_num");
    break;
  case CONST_STR:
    fprintf(fp, "const_str");
    break;
  default:
    fprintf(fp, "unknown");
    break;
  }
  putc('\n', fp);
  print_node(node->lchild, fp, depth + 2);
  putc('\n', fp);
  print_node(node->rchild, fp, depth + 2);
  putc(')', fp);
}

void print_AST(AST *node, FILE *fp)
{
  print_node(node, fp, 0);
  putc('\n', fp);
}

void dump_node(AST *node, FILE *fp, int indent, int depth)
{
  for (int i = 0; i < depth && indent; ++i)
    putc(' ', fp);
  if (!node)
  {
    fprintf(fp, "nil");
    return;
  }
  switch (node->type)
  {
  case PROGRAM:
    dump_node(node->lchild, fp, 0, 0);
    return;
  case CODE_BLOCK:
    dump_node(node->lchild, fp, 0, 0);
    if (node->rchild)
    {
      putc('\n', fp);
      dump_node(node->rchild, fp, 0, 0);
    }
    return;
  case SEXP:
    putc('(', fp);
    for (AST *arg = node; arg; arg = CDR(arg))
    {
      dump_node(CAR(arg), fp, arg != node, depth + 2);
      if (CDR(arg))
        putc('\n', fp);
    }
    putc(')', fp);
    return;
  case CONST_NUM:
  case CONST_STR:
  case NAME:
    fputs(node->value, fp);
    return;
  default:
    return;
  }
}

void dump_AST(AST *node, FILE *fp)
{
  dump_node(node, fp, 0, 0);
  putc('\n', fp);
}
