#include "../parser/parser.h"
#include <stdio.h>
#include <stdlib.h>

int interpret_tree(AST *tree)
{
  if (!tree)
    return fprintf(stderr, "Fatal: unexpected nil\n"), -1;
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
	return interpret_tree(tree->rchild->lchild) + interpret_tree(tree->rchild->rchild->lchild);
      case '-':
	return interpret_tree(tree->rchild->lchild) - interpret_tree(tree->rchild->rchild->lchild);
      case '*':
	return interpret_tree(tree->rchild->lchild) * interpret_tree(tree->rchild->rchild->lchild);
      case '/':
        return interpret_tree(tree->rchild->lchild) / interpret_tree(tree->rchild->rchild->lchild);
      default:
	return fprintf(stderr, "Fatal: unknown operator: '%s'\n", tree->value);
    }
    default:
      return tree->rchild ? fprintf(stderr, "Fatal: operator expected: '%s'\n", tree->value), -1 : interpret_tree(tree->lchild);
    }
  case CONST_NUM:
    return atoi(tree->value);
  default:
    return fprintf(stderr, "Fatal: expression expected. got: '%s'", tree->value), -1;
  }
}

int interpret(AST *program)
{
  if (!program || !program->lchild || !program->lchild->lchild)
    return -1;

  int value = interpret_tree(program->lchild->lchild);
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
