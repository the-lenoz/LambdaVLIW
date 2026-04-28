#include "../SSA/SSA.h"
#include "../SSA/SSA_dump_graphviz.h"
#include "../SSA/SSA_to_L_tri_call.h"
#include "../htable/htable.h"
#include "../parser/parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TMP_VAR_NAME_LEN 31

typedef struct _vm_list
{
  char *var_name;
  SSAValName SSA_name;
  struct _vm_list *next;
} VarMappingList;

static VarMappingList *add_var(VarMappingList *head, const char *name, SSAValName SSA_name)
{
  VarMappingList *new_head = malloc(sizeof(VarMappingList));
  *new_head = (VarMappingList){
      strdup(name),
      SSA_name,
      head};
  return new_head;
}

static int AST_list_len(AST *l)
{
  if (!l || !CAR(l))
    return 0;
  return 1 + AST_list_len(CDR(l));
}

static void destroy_var_mapping_untill(VarMappingList *start, VarMappingList *end)
{
  if (!start || start == end)
    return;
  VarMappingList *next = start->next;
  free(start->var_name);
  free(start);
  return destroy_var_mapping_untill(next, end);
}

static SSAValName get_var(VarMappingList *mapping, const char *name)
{
  if (!mapping)
    return SSA_INVALID_VAL;
  if (!strcmp(name, mapping->var_name))
    return mapping->SSA_name;
  return get_var(mapping->next, name);
}

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

SSAValName compile_expr(AST *expr, VarMappingList *vars, HTable *funcs,
                        SSAModule *module, SSAFuncName fn, SSABasicBlockName *active_BB)
{
  if (!expr)
    return SSA_INVALID_VAL;
  if (expr->type == CONST_NUM)
  {
    SSAValName const_val_name = emit_const_assign(module, fn, *active_BB, atoi(expr->value));
    return const_val_name;
  }
  else if (expr->type == NAME)
  {
    SSAValName val_name = get_var(vars, expr->value);
    if (val_name == SSA_INVALID_VAL)
      return fprintf(stderr, "Fatal: usage of undevined variable '%s'.\n", expr->value), SSA_INVALID_VAL;
    return val_name;
  }
  else if (expr->type == CONST_STR)
    return fprintf(stderr, "Fatal: invalid program structure.\n"), SSA_INVALID_VAL;

  // else SEXP
  STR_MATCH(GET_OP(expr))
  STR_CASE("let")
  VarMappingList *parent_namespace = vars;
  for (AST *decl_list = SECOND(expr); decl_list; decl_list = CDR(decl_list))
  {
    AST *decl = CAR(decl_list);
    const char *var_name = FIRST(decl)->value;
    AST *var_value = SECOND(decl);
    SSAValName var_SSA_name = compile_expr(var_value, vars, funcs, module, fn, active_BB);

    vars = add_var(vars, var_name, var_SSA_name);
  }
  SSAValName result = compile_expr(THIRD(expr), vars, funcs, module, fn, active_BB);
  destroy_var_mapping_untill(vars, parent_namespace);
  return result;
  STR_CASE("cond")
  SSABasicBlockName result_BB = new_BB(module, fn, 0, 0);
  PhiList *result_list = NULL;
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
    PhiList_append(&result_list, (PhiPair){*active_BB, true_result});
    *active_BB = next_BB;
  }
  emit_goto(module, fn, *active_BB, result_BB);
  PhiList_append(&result_list, (PhiPair){*active_BB, SSA_VAL_VOID}); /*If there's no true case in cond*/
  *active_BB = result_BB;
  return emit_phi_assign(module, fn, *active_BB, result_list);

  STR_DEFAULT
  ArgList *args = NULL;
  for (AST *arg_list = CDR(expr); arg_list; arg_list = CDR(arg_list))
  {
    ArgList_append(&args, compile_expr(CAR(arg_list), vars, funcs, module, fn, active_BB));
  }
  int64_t calee;
  if (!ht_get(funcs, GET_OP(expr), (void **)&calee))
    return fprintf(stderr, "Fatal: usage of undefined function '%s'.\n", GET_OP(expr)), SSA_INVALID_VAL;
  return emit_call_assign(module, fn, *active_BB, (SSAFuncName)calee, args, is_constexpr(GET_OP(expr)));
  STR_MATCH_END
}

SSAFuncName build_function(AST *definition, SSAModule *module, HTable *funcs)
{
  if (!definition || !module || !funcs)
    return SSA_INVALID_FUNC;
  const char *name = SECOND(definition)->value;
  AST *arg_list = THIRD(definition);
  AST *body = THIRD(CDR(definition)); // FOURTH

  SSAFuncName func = new_func(module, name, AST_list_len(arg_list), 1);
  ht_set(funcs, name, (void *)(size_t)func);

  SSABasicBlockName entry_block, exit_block, active_block;
  entry_block = new_BB(module, func, 1, 0);
  exit_block = new_BB(module, func, 0, 1);
  active_block = entry_block;
  VarMappingList *locals = NULL;
  for (int i = 0; arg_list && CAR(arg_list); arg_list = CDR(arg_list), ++i)
    locals = add_var(locals, CAR(arg_list)->value, get_arg_val_name(module, func, i));
  SSAValName result = compile_expr(body, locals, funcs, module, func, &active_block);
  emit_goto(module, func, active_block, exit_block);
  emit_return(module, func, exit_block, result);

  return func;
}

SSAModule *build_program(AST *program)
{
  if (!program)
    return fprintf(stderr, "Fatal: no program.\n"), NULL;

  HTable *funcs;
  funcs = ht_init(0);

  SSAModule *module = new_module();

#define DECLARE_BIN_FUNC_STUB(name) ht_set(funcs, name, (void *)(size_t)new_func(module, name, 2, 0))
  DECLARE_BIN_FUNC_STUB("+");
  DECLARE_BIN_FUNC_STUB("-");
  DECLARE_BIN_FUNC_STUB("*");
  DECLARE_BIN_FUNC_STUB("/");
  DECLARE_BIN_FUNC_STUB(">");
  DECLARE_BIN_FUNC_STUB("<");
  DECLARE_BIN_FUNC_STUB("=");
#undef DECLARE_BIN_FUNC_STUB

  for (AST *code_blocks = CAR(program); code_blocks; code_blocks = CDR(code_blocks))
  {
    AST *stmt = CAR(code_blocks);
    if (stmt->type != SEXP || strcmp(GET_OP(stmt), "defun"))
    {
      ht_destroy(funcs);
      return fprintf(stderr, "Fatal: only defun is supported in global namespace, got '%s'.\n",
                     GET_OP(stmt)), NULL;
    }
    build_function(stmt, module, funcs);
  }

  ht_destroy(funcs);

  return module;
}

int compile_file(const char *path, const char *out_path)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", path), -1;

  AST *program = parse_stream(fp, 1); // close inside

  SSAModule *module = build_program(program);
  D_AST(program);

  fp = fopen(out_path, "w");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", out_path), -1;

  SSA_to_L_tri_call_module(module, fp);
#ifndef NDEBUG
  FILE *dump_fp = fopen("/tmp/dump.dot", "w");
  SSA_dump_func_graphviz(module, 0, dump_fp);
  fclose(dump_fp);
#endif
  fclose(fp);

  destroy_module(module);

  return 0;
}

int main(int argc, const char **argv)
{
  if (argc != 3)
    return fprintf(stderr, "Usage: %s input_file output_file\n", argv[0]), -1;

  return compile_file(argv[1], argv[2]);
}
