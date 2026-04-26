#include "../SSA/SSA.h"
#include "../htable/htable.h"
#include "../parser/parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TMP_VAR_NAME_LEN 31

static int is_constexpr(const char *func)
{
  if (!func)
    return 0;
  switch (*func)
  {
  case '+':
  case '-':
  case '*':
  case '/':
  case '<':
  case '>':
  case '=':
    return 1;
  default:
    return 0;
  }
}

SSAValName compile_expr(AST *expr, HTable *vars, HTable *funcs, SSAModule *module, SSAFuncName fn, SSABasicBlockName *active_BB)
{
  if (expr->type != SEXP)
    return fprintf(stderr, "Fatal: invalid program structure.\n"), -1;

  STR_MATCH(GET_OP(expr))
  STR_CASE("let")
  for (AST *decl_list = SECOND(expr); decl_list; decl_list = CDR(decl_list))
  {
    AST *decl = CAR(decl_list);
    const char *var_name = FIRST(decl)->value;
    AST *var_value = SECOND(decl);
    SSAValName var_SSA_name = compile_expr(var_value, vars, funcs, module, fn, active_BB);
    ht_set(vars, var_name, (void *)(size_t)var_SSA_name);
  }
  return compile_expr(THIRD(expr), vars, funcs, module, fn, active_BB);
  STR_CASE("cond")
  SSABasicBlockName result_BB = new_BB(module, fn, 0, 0);
  PhiList *result_list = new_PhiList(module, fn, result_BB);
  for (AST *options_list = CDR(expr); options_list; options_list = CDR(options_list))
  {
    AST *option = CAR(options_list);
    AST *cond = FIRST(option);
    AST *then = SECOND(option);

    SSAValName condition = compile_expr(cond, vars, funcs, module, fn, active_BB);
    SSABasicBlockName true_BB = new_BB(module, fn, 0, 0);
    SSABasicBlockName next_BB = new_BB(module, fn, 0, 0);
    emit_cond_goto(module, fn, *active_BB, condition, true_BB, next_BB);
    *active_BB = true_BB;
    SSAValName true_result = compile_expr(then, vars, funcs, module, fn, active_BB);
    emit_goto(module, fn, *active_BB, result_BB);
    PhiList_append(result_list, (PhiPair){*active_BB, true_result});
    *active_BB = next_BB;
  }
  emit_goto(module, fn, *active_BB, result_BB);
  PhiList_append(result_list, (PhiPair){*active_BB, SSA_INVALID_VAL}); /*No true case in cond*/
  *active_BB = result_BB;
  return emit_phi_assign(module, fn, *active_BB, result_list);

  STR_DEFAULT
  SSABasicBlockName result_BB = new_BB(module, fn, 0, 0);
  ArgList *args = new_ArgList(module, fn, result_BB);
  for (AST *arg_list = CDR(expr); arg_list; arg_list = CDR(arg_list))
  {
    ArgList_append(args, compile_expr(CAR(arg_list), vars, funcs, module, fn, active_BB));
  }
  emit_goto(module, fn, *active_BB, result_BB);
  *active_BB = result_BB;
  SSAFuncName calee;
  if (!ht_get(funcs, GET_OP(expr), (void **)&calee))
    return fprintf(stderr, "Fatal: usage of undefined function '%s'.\n", GET_OP(expr)), SSA_INVALID_VAL;
  return emit_call_assign(module, fn, *active_BB, calee, args, is_constexpr(GET_OP(expr)));
  STR_MATCH_END
}

int compile_program(AST *program, FILE *out_fp)
{
  if (!program || !out_fp)
    return fprintf(stderr, "Fatal: no program or file_ptr.\n"), -1;

  int var_name_c = 0;

  putc('(', out_fp);

  putc(')', out_fp);
  return 0;
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
