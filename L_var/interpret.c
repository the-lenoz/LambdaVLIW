#include "../htable/htable.h"
#include "../parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
void *v_dup(const void *v) { return (void *)(size_t)v; }

int interpret_tree(AST *tree, HTable *var_tab)
{
  if (!tree)
    return fprintf(stderr, "Fatal: unexpected nil\n"), -1;
  ssize_t value;
  switch (tree->type)
  {
  case SEXP:
    switch (tree->lchild->type)
    {
    case NAME:
      if (!tree->rchild->lchild || !tree->rchild->rchild || tree->rchild->rchild->rchild)
        return fprintf(stderr, "Fatal: only binary operators supported. got '%s'\n", tree->value), -1;
      switch (*tree->lchild->value)
      {
      case '+':
        return interpret_tree(tree->rchild->lchild, var_tab) + interpret_tree(tree->rchild->rchild->lchild, var_tab);
      case '-':
        return interpret_tree(tree->rchild->lchild, var_tab) - interpret_tree(tree->rchild->rchild->lchild, var_tab);
      case '*':
        return interpret_tree(tree->rchild->lchild, var_tab) * interpret_tree(tree->rchild->rchild->lchild, var_tab);
      case '/':
        return interpret_tree(tree->rchild->lchild, var_tab) / interpret_tree(tree->rchild->rchild->lchild, var_tab);
      case 'l':
        if (!strcmp(tree->lchild->value, "let"))
        {
          ht_set(var_tab, tree->rchild->lchild->lchild->value,
                 (void *)(ssize_t)interpret_tree(tree->rchild->lchild->rchild->lchild, var_tab));
          return interpret_tree(tree->rchild->rchild->lchild, var_tab);
        }
      default:
        return fprintf(stderr, "Fatal: unknown operator: '%s'\n", tree->value);
      }
    default:
      return tree->rchild ? fprintf(stderr, "Fatal: operator expected: '%s'\n", tree->value), -1 : interpret_tree(tree->lchild, var_tab);
    }
  case CONST_NUM:
    return atoi(tree->value);
  case NAME:
    if (!ht_get(var_tab, tree->value, (void **)&value))
      return fprintf(stderr, "Fatal: unknown id: '%s'\n", tree->value);
    return value;
  default:
    return fprintf(stderr, "Fatal: expression expected. got: '%s'\n", tree->value), -1;
  }
}

int interpret(AST *program)
{
  if (!program || !program->lchild || !program->lchild->lchild)
    return -1;

  HTable *var_tab = ht_init(strhash, str_eq, k_dup, v_dup, free_str, 0);
  int value = interpret_tree(program->lchild->lchild, var_tab);
  ht_destroy(var_tab);
  printf("Result: %d\n", value);
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
