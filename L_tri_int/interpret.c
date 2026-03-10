#include "../parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VARS_NUM 1024

int evaluate(AST *expr, int vars_array_size, const int *vars_array)
{
  if (!expr || expr->type != SEXP)
    return fprintf(stderr, "Fatal: S-exp expected, got nil\n"), -1;

  int l_arg, r_arg;
  if (*expr->rchild->lchild->value >= '0' && *expr->rchild->lchild->value <= '9')
    l_arg = atoi(expr->rchild->lchild->value);
  else
  {
    if (strncmp(expr->rchild->lchild->value, "var", 3))
      return fprintf(stderr, "Fatal: 'var' expected, got: '%s'\n", expr->rchild->lchild->value), -1;
    int l_var_num = atoi(expr->rchild->lchild->value + 3);
    if (l_var_num >= vars_array_size)
      return fprintf(stderr, "Fatal: undefined variable '%s'\n", expr->rchild->lchild->value), -1;
    l_arg = vars_array[l_var_num];
  }
  if (*expr->rchild->rchild->lchild->value >= '0' && *expr->rchild->rchild->lchild->value <= '9')
    r_arg = atoi(expr->rchild->rchild->lchild->value);
  else
  {
    if (strncmp(expr->rchild->rchild->lchild->value, "var", 3))
      return fprintf(stderr, "Fatal: 'var' expected, got: '%s'\n", expr->rchild->rchild->lchild->value), -1;
    int r_var_num = atoi(expr->rchild->rchild->lchild->value + 3);
    if (r_var_num >= vars_array_size)
      return fprintf(stderr, "Fatal: undefined variable '%s'\n", expr->rchild->rchild->lchild->value), -1;
    r_arg = vars_array[r_var_num];
  }  

  int result;
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
  int result = 0;
  int vars_num = DEFAULT_VARS_NUM;
  int *vars = calloc(vars_num, sizeof(int));

  if (!tree || tree->type != SEXP)
    return fprintf(stderr, "Fatal: unexpected expression type\n"), -1;

  AST *instr = tree;
  int first = 1;
  int var_num;
  while (instr)
  {
    if (strcmp(instr->lchild->lchild->value, "let"))
      return fprintf(stderr, "Fatal: 'let' block expected. got: '%s'\n", instr->lchild->lchild->value), -1;

    if (!strcmp(instr->lchild->rchild->lchild->value, "res"))
    {
      result = evaluate(instr->lchild->rchild->rchild->lchild, vars_num, vars);
      break;
    }
    if (strncmp(instr->lchild->rchild->lchild->value, "var", 3))
      return fprintf(stderr, "Fatal: 'var' expected. got: '%s'\n", instr->lchild->rchild->value), -1;
    var_num = atoi(instr->lchild->rchild->lchild->value + 3); // "varXX" strip "var" str
    if (var_num >= vars_num)
    {
      vars_num = var_num * 2;
      vars = realloc(vars, vars_num * sizeof(int));
    }
    vars[var_num] = evaluate(instr->lchild->rchild->rchild->lchild, vars_num, vars);

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
  free(vars);
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
