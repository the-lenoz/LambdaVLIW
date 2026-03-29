#include "../parser/parser.h"
#include "../htable/htable.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VARS_NUM 1024

size_t strhash(const void *k)
{
  const unsigned char *str = (const unsigned char *)k;
  size_t hash = 5381;
  int c;

  while ((c = *str++))
  {
    // hash = hash * 33 + c
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}
int str_eq(const void *a, const void *b) { return !strcmp(a, b); }

void free_str(HTable *ht, void *k, void *v) { free(k); }
void *k_dup(const void *k) { return strdup(k); }
void*v_dup(const void*v){return (void*)(size_t)v;}

int evaluate(AST *expr, HTable *var_tab)
{
  if (!expr || expr->type != SEXP)
    return fprintf(stderr, "Fatal: S-exp expected, got nil\n"), -1;

  ssize_t l_arg, r_arg;
  if (expr->rchild->lchild->type == CONST_NUM)
    l_arg = atoi(expr->rchild->lchild->value);
  else
  {
    if (!ht_get(var_tab, expr->rchild->lchild->value, (void**)&l_arg))
      return fprintf(stderr, "Fatal: undefined variable '%s'\n", expr->rchild->lchild->value), -1;
  }
  if (expr->rchild->rchild->lchild->type == CONST_NUM)
    r_arg = atoi(expr->rchild->rchild->lchild->value);
  else
  {
    if (!ht_get(var_tab, expr->rchild->rchild->lchild->value, (void**)&r_arg))
      return fprintf(stderr, "Fatal: undefined variable '%s'\n", expr->rchild->lchild->value), -1;
  }

  switch (*expr->lchild->value)
  {
  case '+':
    return l_arg + r_arg;
    break;
  case '-':
    return l_arg - r_arg;
    break;
  case '*':
    return l_arg * r_arg;
    break;
  case '/':
    return l_arg / r_arg;
    break;
  default:
    return fprintf(stderr, "Fatal: binary arithmetic operator expected, got: '%s'\n", expr->lchild->value), -1;
  }
}

int interpret_list(AST *tree)
{
  HTable *var_tab = ht_init(strhash, str_eq, k_dup, v_dup, free_str, 0);

  if (!tree || tree->type != SEXP)
    return fprintf(stderr, "Fatal: unexpected expression type\n"), -1;

  AST *instr = tree;
  int first = 1;
  int var_num;
  int ticks = 0;
  while (instr)
  {
    if (strcmp(instr->lchild->lchild->value, "let"))
      return fprintf(stderr, "Fatal: 'let' block expected. got: '%s'\n", instr->lchild->lchild->value), -1;

    ht_set(var_tab, instr->lchild->rchild->lchild->value, (void*)(ssize_t)evaluate(instr->lchild->rchild->rchild->lchild, var_tab));

    ++ticks;
    if (first)
    {
      instr = tree->rchild;
      first = 0;
    }
    else
    {
      instr = instr->rchild;
    }
  }
  ssize_t result;
  if (!ht_get(var_tab, "res", (void **)&result))
    return fprintf(stderr, "Fatal: no result in the program.\n"), -1;
  ht_destroy(var_tab);
  printf("Program finished in %d ticks\n", ticks);
  return result;
}

int interpret(AST *program)
{
  if (!program || !program->lchild || !program->lchild->lchild)
    return -1;

  int value = interpret_list(program->lchild->lchild);
  printf("Result: %d\n", value);
  D_AST(program);
  return 0;
}

int interpret_stream(FILE *fp)
{
  AST *program = parse_stream(fp, 0);

  if (!program->lchild)
    return fprintf(stderr, "Invalid syntax\n"), -1;

  return interpret(program);
}

int process_file(const char *filename)
{
  if (!filename)
    return -1;

  FILE *fp = fopen(filename, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", filename), -1;

  int status = interpret_stream(fp);

  fclose(fp);
  return status;
}

int main(int argc, char **argv)
{
  if (argc > 2)
    return fprintf(stderr, "Usage: %s source.lisp\n", argv[0]), -1;

  if (argc == 1)
    return interpret_stream(stdin);
  else
  {
    const char *filename = argv[1];
    return process_file(filename);
  }
}
