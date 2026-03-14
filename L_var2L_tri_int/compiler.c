#include "../parser/parser.h"
#include <stdio.h>
#include <string.h>

#define MAX_TMP_VAR_NAME_LEN 31

int compile_expr(AST *expr, const char *result_name, FILE *out_fp, int *var_name_c)
{
  const char *op_c, *l_arg, *r_arg;
  char l_arg_val[MAX_TMP_VAR_NAME_LEN + 1];
  char r_arg_val[MAX_TMP_VAR_NAME_LEN + 1];
  switch (expr->type)
  {
  case SEXP:
    if (!strcmp(expr->lchild->value, "let"))
    {
      compile_expr(expr->rchild->lchild->rchild->lchild, expr->rchild->lchild->lchild->value, out_fp, var_name_c);
      compile_expr(expr->rchild->rchild->lchild, result_name, out_fp, var_name_c);
    }
    else
    {
      if ((*expr->lchild->value == '+' || *expr->lchild->value == '-' ||
           *expr->lchild->value == '*' || *expr->lchild->value == '/'))
      {
        if (expr->rchild->lchild->type == CONST_NUM || expr->rchild->lchild->type == NAME)
          l_arg = expr->rchild->lchild->value;
        else
        {
          l_arg = l_arg_val;
          memset(l_arg_val, 0, MAX_TMP_VAR_NAME_LEN + 1);
          snprintf(l_arg_val, MAX_TMP_VAR_NAME_LEN, "__tmp_%d", (*var_name_c)++);
          compile_expr(expr->rchild->lchild, l_arg, out_fp, var_name_c);
        }
        if (expr->rchild->rchild->lchild->type == CONST_NUM || expr->rchild->rchild->lchild->type == NAME)
          r_arg = expr->rchild->rchild->lchild->value;
        else
        {
          r_arg = r_arg_val;
          memset(r_arg_val, 0, MAX_TMP_VAR_NAME_LEN + 1);
          snprintf(r_arg_val, MAX_TMP_VAR_NAME_LEN, "__tmp_%d", (*var_name_c)++);
          compile_expr(expr->rchild->rchild->lchild, r_arg, out_fp, var_name_c);
        }

        fprintf(out_fp, "(let %s (%s %s %s))\n", result_name,
                expr->lchild->value, l_arg, r_arg);
      }
      else
      {
        return fprintf(stderr, "Fatal: unsupported operator: '%s'\n", expr->value), -1;
      }
    }

    break;
  default:
    return fprintf(stderr, "Fatal: compilation flow shouldn't be here. '%s'\n", expr->value), -1;
  }
  return 0;
}

int compile_program(AST *program, FILE *out_fp)
{
  if (!program || !out_fp)
    return fprintf(stderr, "Fatal: no program or file_ptr.\n"), -1;

  int var_name_c = 0;

  putc('(', out_fp);

  int status = compile_expr(program->lchild->lchild, "res", out_fp, &var_name_c);

  putc(')', out_fp);
  return status;
}

int compile_file(const char *path, const char *out_path)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", path), -1;

  AST *program = parse_stream(fp, 1); // close inside

  fp = fopen(out_path, "w");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", out_path), -1;

  int status = compile_program(program, fp);

  fclose(fp);
  D_AST(program);

  if (status != 0)
    return status;

  return 0;
}

int main(int argc, const char **argv)
{
  if (argc != 3)
    return fprintf(stderr, "Usage: %s input_file output_file\n", argv[0]), -1;

  return compile_file(argv[1], argv[2]);
}
