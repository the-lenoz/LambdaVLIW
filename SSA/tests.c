#include "SSA.h"
#include "SSA_dump_graphviz.h"
#include "SSA_to_L_tri_call.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond)                                                            \
  do                                                                                 \
  {                                                                                  \
    if (!(cond))                                                                     \
    {                                                                                \
      fprintf(stderr, "    assert failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
      return -1;                                                                     \
    }                                                                                \
  } while (0)

static FILE *open_dump_file(const char *case_name, const char *ext, char *resolved_path, size_t path_cap)
{
  const char *roots[] = {"samples", "SSA/samples"};

  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
  {
    if (snprintf(resolved_path, path_cap, "%s/%s.%s", roots[i], case_name, ext) >= (int)path_cap)
      continue;

    FILE *fp = fopen(resolved_path, "w");
    if (fp)
      return fp;
  }

  return NULL;
}

static int dump_test_case(const char *case_name, const SSAModule *module)
{
  char path[256];
  FILE *fp = open_dump_file(case_name, "trif", path, sizeof(path));

  if (!fp)
    return -1;
  if (SSA_to_L_tri_call_module(module, fp) < 0)
    return fclose(fp), -1;
  fclose(fp);
  printf("  dump: %s\n", path);

  fp = open_dump_file(case_name, "dot", path, sizeof(path));
  if (!fp)
    return -1;
  if (SSA_dump_module_graphviz(module, fp) < 0)
    return fclose(fp), -1;
  fclose(fp);
  printf("  dump: %s\n", path);

  return 0;
}

static int dump_cfg_info_test_case(const char *case_name, SSAModule *module)
{
  char path[256];
  FILE *fp = open_dump_file(case_name, "cfg.dot", path, sizeof(path));

  if (!fp)
    return -1;
  if (SSA_dump_module_cfg_info_graphviz(module, fp) < 0)
    return fclose(fp), -1;
  fclose(fp);
  printf("  dump: %s\n", path);

  return 0;
}

static int stream_contains(FILE *fp, const char *needle)
{
  char buffer[1024];

  if (!fp || !needle)
    return 0;

  rewind(fp);
  while (fgets(buffer, sizeof(buffer), fp))
    if (strstr(buffer, needle))
      return 1;

  return 0;
}

static int bb_list_count(const SSABasicBlockList *list)
{
  int count = 0;
  for (; list; list = list->next)
    ++count;
  return count;
}

static int bb_list_count_bb(const SSABasicBlockList *list, SSABasicBlockName bb)
{
  int count = 0;
  for (; list; list = list->next)
    if (list->BB == bb)
      ++count;
  return count;
}

static const CFGLoop *find_loop_by_header(const CFGStructureAnnotation *annotation,
                                          SSABasicBlockName header)
{
  if (!annotation)
    return NULL;

  for (int i = 0; i < annotation->loops_count; ++i)
    if (annotation->loops[i].header == header)
      return annotation->loops + i;
  return NULL;
}

static SSAConst make_i1(int value)
{
  SSAConst cnst = {0};
  cnst.i1_value = value;
  return cnst;
}

static SSAConst make_i32(int32_t value)
{
  SSAConst cnst = {0};
  cnst.i32_value = value;
  return cnst;
}

static SSAConst make_i64(int64_t value)
{
  SSAConst cnst = {0};
  cnst.i64_value = value;
  return cnst;
}

static const SSAInstr *get_instr(const SSAFunc *func, SSABasicBlockName bb, unsigned int index)
{
  if (!func || bb >= func->basic_blocks_count)
    return NULL;

  SSAInstrName instr = func->basic_blocks[bb].first_instr;
  for (unsigned int i = 0; instr != SSA_INVALID_INSTR; instr = instr->next, ++i)
    if (i == index)
      return instr;

  return NULL;
}

static const SSABlockTerminator *get_terminator(const SSAFunc *func, SSABasicBlockName bb)
{
  if (!func || bb >= func->basic_blocks_count)
    return NULL;

  const SSAInstr *instr = func->basic_blocks[bb].last_instr;
  if (!instr || instr->kind != SSA_INSTR_TERM)
    return NULL;

  return &instr->term;
}

static unsigned int get_instr_count(const SSAFunc *func, SSABasicBlockName bb)
{
  unsigned int count = 0;
  if (!func || bb >= func->basic_blocks_count)
    return 0;

  for (SSAInstrName instr = func->basic_blocks[bb].first_instr; instr != SSA_INVALID_INSTR; instr = instr->next)
    count += 1;

  return count;
}

static int count_phi_options(const PhiList *options)
{
  int count = 0;

  for (const PhiList *it = options; it; it = it->next)
    count += 1;
  return count;
}

static int tree_has_child(const BBTree *tree, SSABasicBlockName parent, SSABasicBlockName child)
{
  if (!tree || !tree->child_arr || !tree->sibling_arr)
    return 0;

  for (SSABasicBlockName it = tree->child_arr[parent]; it != SSA_INVALID_BB; it = tree->sibling_arr[it])
    if (it == child)
      return 1;

  return 0;
}

static int test_simple_call_and_return(void)
{
  SSAModule *module;
  SSAFuncName callee;
  SSAFuncName caller;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  SSAValName call_val;
  SSAFunc *caller_fn;
  const SSABlockTerminator *term;

  module = new_module();
  TEST_ASSERT(module != NULL);

  callee = new_func(module, "callee", SSA_i32, 0, NULL, 1);
  TEST_ASSERT(callee != SSA_INVALID_FUNC);

  caller = new_func(module, "main", SSA_i32, 0, NULL, 1);
  TEST_ASSERT(caller != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, caller);
  bb_exit = new_BB(module, caller);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, caller, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, caller, bb_exit) == 1);

  caller_fn = &module->functions[caller];
  TEST_ASSERT(caller_fn->entry_block == bb_entry);
  TEST_ASSERT(caller_fn->exit_block == bb_exit);

  call_val = emit_call_assign(module, caller, bb_entry, callee, NULL, 1, 0);
  TEST_ASSERT(call_val != SSA_INVALID_VAL);
  TEST_ASSERT(caller_fn->values_count == 1);
  TEST_ASSERT(caller_fn->values[call_val].kind == SSA_VALUE_CALL);
  TEST_ASSERT(caller_fn->values[call_val].type == SSA_i32);
  TEST_ASSERT(caller_fn->values[call_val].is_const == 1);
  TEST_ASSERT(caller_fn->values[call_val].expr.call.calee_name == callee);
  TEST_ASSERT(caller_fn->values[call_val].expr.call.args == NULL);
  TEST_ASSERT(caller_fn->values[call_val].parent_name == bb_entry);

  TEST_ASSERT(emit_goto(module, caller, bb_entry, bb_exit) == 0);
  TEST_ASSERT(get_instr_count(caller_fn, bb_entry) == 2);
  TEST_ASSERT(get_instr(caller_fn, bb_entry, 0)->kind == SSA_INSTR_VAL);
  TEST_ASSERT(get_instr(caller_fn, bb_entry, 0)->val == call_val);

  term = get_terminator(caller_fn, bb_entry);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_GOTO);
  TEST_ASSERT(term->cond == SSA_INVALID_VAL);
  TEST_ASSERT(term->ret_val == SSA_INVALID_VAL);
  TEST_ASSERT(term->true_dst == bb_exit);
  TEST_ASSERT(term->false_dst == SSA_INVALID_BB);

  TEST_ASSERT(emit_goto(module, caller, bb_entry, bb_exit) < 0);

  TEST_ASSERT(emit_return(module, caller, bb_exit, call_val) == 0);
  TEST_ASSERT(get_instr_count(caller_fn, bb_exit) == 1);
  term = get_terminator(caller_fn, bb_exit);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_RETURN);
  TEST_ASSERT(term->cond == SSA_INVALID_VAL);
  TEST_ASSERT(term->ret_val == call_val);
  TEST_ASSERT(term->true_dst == SSA_INVALID_BB);
  TEST_ASSERT(term->false_dst == SSA_INVALID_BB);

  TEST_ASSERT(emit_return(module, caller, bb_exit, call_val) < 0);

  TEST_ASSERT(dump_test_case("simple_call_and_return", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_branch_and_phi(void)
{
  static const SSAValueType calc_arg_types[] = {SSA_i1};

  SSAModule *module;
  SSAFuncName cond_fn;
  SSAFuncName calc_fn;
  SSAFuncName main_fn_name;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_true;
  SSABasicBlockName bb_false;
  SSABasicBlockName bb_merge;
  ArgList *args_true;
  ArgList *args_false;
  SSAValName cond_val;
  SSAValName true_val;
  SSAValName false_val;
  SSAValName phi_val;
  SSAFunc *main_fn;
  const SSABlockTerminator *term;

  module = new_module();
  TEST_ASSERT(module != NULL);

  cond_fn = new_func(module, "cond_provider", SSA_i1, 0, NULL, 1);
  calc_fn = new_func(module, "calc", SSA_i32, 1, calc_arg_types, 1);
  main_fn_name = new_func(module, "branch_main", SSA_i32, 0, NULL, 1);
  TEST_ASSERT(cond_fn != SSA_INVALID_FUNC);
  TEST_ASSERT(calc_fn != SSA_INVALID_FUNC);
  TEST_ASSERT(main_fn_name != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, main_fn_name);
  bb_true = new_BB(module, main_fn_name);
  bb_false = new_BB(module, main_fn_name);
  bb_merge = new_BB(module, main_fn_name);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_true != SSA_INVALID_BB);
  TEST_ASSERT(bb_false != SSA_INVALID_BB);
  TEST_ASSERT(bb_merge != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, main_fn_name, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, main_fn_name, bb_merge) == 1);

  main_fn = &module->functions[main_fn_name];

  cond_val = emit_call_assign(module, main_fn_name, bb_entry, cond_fn, NULL, 0, 0);
  TEST_ASSERT(cond_val != SSA_INVALID_VAL);
  TEST_ASSERT(main_fn->values[cond_val].type == SSA_i1);

  TEST_ASSERT(emit_cond_goto(module, main_fn_name, bb_entry, cond_val, bb_true, bb_false) == 0);
  term = get_terminator(main_fn, bb_entry);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_COND_GOTO);
  TEST_ASSERT(term->cond == cond_val);
  TEST_ASSERT(term->true_dst == bb_true);
  TEST_ASSERT(term->false_dst == bb_false);

  args_true = NULL;
  TEST_ASSERT(ArgList_append(&args_true, cond_val) == 0);
  true_val = emit_call_assign(module, main_fn_name, bb_true, calc_fn, args_true, 0, 0);
  TEST_ASSERT(true_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_true, bb_merge) == 0);

  args_false = NULL;
  TEST_ASSERT(ArgList_append(&args_false, cond_val) == 0);
  false_val = emit_call_assign(module, main_fn_name, bb_false, calc_fn, args_false, 0, 0);
  TEST_ASSERT(false_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_false, bb_merge) == 0);

  phi_val = emit_phi_assign(module, main_fn_name, bb_merge, SSA_i32);
  TEST_ASSERT(phi_val != SSA_INVALID_VAL);
  TEST_ASSERT(add_phi_option(module, main_fn_name, phi_val,
                             (PhiPair){.previous_block_name = bb_true, .value_name = true_val}) == 0);
  TEST_ASSERT(add_phi_option(module, main_fn_name, phi_val,
                             (PhiPair){.previous_block_name = bb_false, .value_name = false_val}) == 0);
  TEST_ASSERT(main_fn->values[phi_val].kind == SSA_VALUE_PHI);
  TEST_ASSERT(main_fn->values[phi_val].type == SSA_i32);
  TEST_ASSERT(main_fn->values[phi_val].parent_name == bb_merge);
  TEST_ASSERT(count_phi_options(main_fn->values[phi_val].expr.phi.options) == 2);

  TEST_ASSERT(emit_return(module, main_fn_name, bb_merge, phi_val) == 0);
  TEST_ASSERT(get_instr_count(main_fn, bb_merge) == 2);
  TEST_ASSERT(get_instr(main_fn, bb_merge, 0)->kind == SSA_INSTR_VAL);
  TEST_ASSERT(get_instr(main_fn, bb_merge, 0)->val == phi_val);
  term = get_terminator(main_fn, bb_merge);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_RETURN);
  TEST_ASSERT(term->ret_val == phi_val);

  TEST_ASSERT(emit_cond_goto(module, main_fn_name, bb_entry, cond_val, bb_true, bb_false) < 0);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_true, bb_merge) < 0);
  TEST_ASSERT(emit_return(module, main_fn_name, bb_merge, phi_val) < 0);

  TEST_ASSERT(dump_test_case("branch_and_phi", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_invalid_inputs(void)
{
  static const SSAValueType takes_i32_arg_types[] = {SSA_i32};

  SSAModule *module;
  SSAFuncName fn;
  SSAFuncName takes_i32;
  SSAFuncName void_fn;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  ArgList *args;
  SSAValName int_val;
  SSAValName cond;
  SSAValName retv;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "invalid_case", SSA_i1, 0, NULL, 0);
  takes_i32 = new_func(module, "takes_i32", SSA_i32, 1, takes_i32_arg_types, 0);
  void_fn = new_func(module, "void_sink", SSA_void, 0, NULL, 0);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  TEST_ASSERT(takes_i32 != SSA_INVALID_FUNC);
  TEST_ASSERT(void_fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn);
  bb_exit = new_BB(module, fn);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, bb_exit) == 1);

  int_val = emit_const_assign(module, fn, bb_entry, SSA_i32, make_i32(7));
  TEST_ASSERT(int_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_const_assign(module, fn, bb_entry, SSA_void, make_i32(0)) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_phi_assign(module, fn, bb_entry, SSA_void) == SSA_INVALID_VAL);

  TEST_ASSERT(emit_call_assign(module, fn, bb_entry, SSA_INVALID_FUNC, NULL, 0, 0) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_call_assign(module, fn, bb_entry, void_fn, NULL, 0, 0) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_void_call(module, fn, bb_entry, fn, NULL) < 0);
  TEST_ASSERT(emit_void_call(module, fn, bb_entry, void_fn, NULL) == 0);

  cond = emit_call_assign(module, fn, bb_entry, fn, NULL, 0, 0);
  TEST_ASSERT(cond != SSA_INVALID_VAL);

  args = NULL;
  TEST_ASSERT(ArgList_append(&args, int_val) == 0);
  TEST_ASSERT(emit_call_assign(module, fn, bb_entry, fn, args, 0, 0) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_call_assign(module, fn, bb_entry, takes_i32, NULL, 0, 0) == SSA_INVALID_VAL);

  TEST_ASSERT(emit_return(module, fn, bb_entry, cond) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, SSA_INVALID_VAL, bb_entry, bb_exit) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, int_val, bb_entry, bb_exit) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, SSA_INVALID_BB, bb_exit) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) < 0);

  retv = emit_call_assign(module, fn, bb_exit, fn, NULL, 0, 0);
  TEST_ASSERT(retv != SSA_INVALID_VAL);
  TEST_ASSERT(emit_return(module, fn, bb_exit, SSA_INVALID_VAL) < 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, retv) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, retv) < 0);
  TEST_ASSERT(emit_goto(module, fn, bb_exit, bb_entry) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_exit, cond, bb_entry, bb_exit) < 0);

  TEST_ASSERT(dump_test_case("invalid_inputs", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_const_assign(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  SSAValName v0;
  SSAValName v1;
  SSAValName v2;
  SSAFunc *f;
  const SSABlockTerminator *term;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "const_main", SSA_i32, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn);
  bb_exit = new_BB(module, fn);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, bb_exit) == 1);

  f = &module->functions[fn];

  v0 = emit_const_assign(module, fn, bb_entry, SSA_i32, make_i32(42));
  v1 = emit_const_assign(module, fn, bb_entry, SSA_i32, make_i32(INT_MAX));
  v2 = emit_const_assign(module, fn, bb_exit, SSA_i32, make_i32(INT_MIN));
  TEST_ASSERT(v0 != SSA_INVALID_VAL);
  TEST_ASSERT(v1 != SSA_INVALID_VAL);
  TEST_ASSERT(v2 != SSA_INVALID_VAL);

  TEST_ASSERT(f->values_count == 3);
  TEST_ASSERT(f->values[v0].kind == SSA_VALUE_CONST);
  TEST_ASSERT(f->values[v1].kind == SSA_VALUE_CONST);
  TEST_ASSERT(f->values[v2].kind == SSA_VALUE_CONST);
  TEST_ASSERT(f->values[v0].type == SSA_i32);
  TEST_ASSERT(f->values[v1].type == SSA_i32);
  TEST_ASSERT(f->values[v2].type == SSA_i32);
  TEST_ASSERT(f->values[v0].is_const == 1);
  TEST_ASSERT(f->values[v1].is_const == 1);
  TEST_ASSERT(f->values[v2].is_const == 1);
  TEST_ASSERT(f->values[v0].expr.cnst.i32_value == 42);
  TEST_ASSERT(f->values[v1].expr.cnst.i32_value == INT_MAX);
  TEST_ASSERT(f->values[v2].expr.cnst.i32_value == INT_MIN);
  TEST_ASSERT(f->values[v0].parent_name == bb_entry);
  TEST_ASSERT(f->values[v2].parent_name == bb_exit);

  TEST_ASSERT(emit_const_assign(module, fn, SSA_INVALID_BB, SSA_i32, make_i32(7)) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_const_assign(module, fn, bb_entry, SSA_void, make_i32(7)) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, v2) == 0);

  TEST_ASSERT(get_instr_count(f, bb_entry) == 3);
  TEST_ASSERT(get_instr(f, bb_entry, 0)->val == v0);
  TEST_ASSERT(get_instr(f, bb_entry, 1)->val == v1);
  term = get_terminator(f, bb_entry);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_GOTO);

  TEST_ASSERT(get_instr_count(f, bb_exit) == 2);
  TEST_ASSERT(get_instr(f, bb_exit, 0)->val == v2);
  term = get_terminator(f, bb_exit);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_RETURN);
  TEST_ASSERT(term->ret_val == v2);

  TEST_ASSERT(dump_test_case("const_assign", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_void_value(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  SSAFunc *f;
  const SSABlockTerminator *term;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "void_main", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn);
  bb_exit = new_BB(module, fn);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, bb_exit) == 1);

  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, SSA_VALUE_VOID) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, SSA_INVALID_VAL) < 0);

  f = &module->functions[fn];
  term = get_terminator(f, bb_exit);
  TEST_ASSERT(term != NULL);
  TEST_ASSERT(term->type == SSA_TERM_RETURN);
  TEST_ASSERT(term->ret_val == SSA_VALUE_VOID);

  TEST_ASSERT(dump_test_case("void_value", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_dump_func_args(void)
{
  static const SSAValueType arg_types[] = {SSA_i32, SSA_i1};

  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  FILE *fp;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "with_args", SSA_void, 2, arg_types, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  TEST_ASSERT(module->functions[fn].values[0].type == SSA_i32);
  TEST_ASSERT(module->functions[fn].values[1].type == SSA_i1);

  bb_entry = new_BB(module, fn);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(emit_return(module, fn, bb_entry, SSA_VALUE_VOID) == 0);

  fp = tmpfile();
  TEST_ASSERT(fp != NULL);
  TEST_ASSERT(SSA_to_L_tri_call_module(module, fp) == 0);
  TEST_ASSERT(stream_contains(fp, "(func with_args v0 v1"));
  fclose(fp);

  destroy_module(module);
  return 0;
}

static int test_bool_cast_dump(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  SSAValName v0;
  SSAValName v1;
  FILE *fp;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "bool_cast_main", SSA_i1, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn);
  bb_exit = new_BB(module, fn);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, bb_exit) == 1);

  v0 = emit_const_assign(module, fn, bb_entry, SSA_i64, make_i64(42));
  TEST_ASSERT(v0 != SSA_INVALID_VAL);
  v1 = emit_bool_cast_assign(module, fn, bb_entry, v0, SSA_i64);
  TEST_ASSERT(v1 != SSA_INVALID_VAL);
  TEST_ASSERT(module->functions[fn].values[v1].kind == SSA_VALUE_BOOL_CAST);
  TEST_ASSERT(module->functions[fn].values[v1].type == SSA_i1);
  TEST_ASSERT(module->functions[fn].values[v1].expr.bool_val == v0);

  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, v1) == 0);

  fp = tmpfile();
  TEST_ASSERT(fp != NULL);
  TEST_ASSERT(SSA_to_L_tri_call_module(module, fp) == 0);
  TEST_ASSERT(stream_contains(fp, "(bool-cast v0)"));
  fclose(fp);

  fp = tmpfile();
  TEST_ASSERT(fp != NULL);
  TEST_ASSERT(SSA_dump_module_graphviz(module, fp) == 0);
  TEST_ASSERT(stream_contains(fp, "bool-cast v0"));
  fclose(fp);

  destroy_module(module);
  return 0;
}

static int test_dom_tree_and_dump(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_left;
  SSABasicBlockName bb_right;
  SSABasicBlockName bb_merge;
  SSABasicBlockName bb_loop;
  SSABasicBlockName bb_exit;
  SSAValName cond;
  SSAFunc *func;
  FILE *fp;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "dom_main", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn);
  bb_left = new_BB(module, fn);
  bb_right = new_BB(module, fn);
  bb_merge = new_BB(module, fn);
  bb_loop = new_BB(module, fn);
  bb_exit = new_BB(module, fn);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_left != SSA_INVALID_BB);
  TEST_ASSERT(bb_right != SSA_INVALID_BB);
  TEST_ASSERT(bb_merge != SSA_INVALID_BB);
  TEST_ASSERT(bb_loop != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, bb_entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, bb_exit) == 1);

  cond = emit_const_assign(module, fn, bb_entry, SSA_i1, make_i1(1));
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, bb_left, bb_right) == 0);
  TEST_ASSERT(emit_goto(module, fn, bb_left, bb_merge) == 0);
  TEST_ASSERT(emit_goto(module, fn, bb_right, bb_merge) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_merge, cond, bb_loop, bb_exit) == 0);
  TEST_ASSERT(emit_goto(module, fn, bb_loop, bb_merge) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, SSA_VALUE_VOID) == 0);

  TEST_ASSERT(require_Dom_tree(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.valid_DOM == 1);
  TEST_ASSERT(func->CFG_info.Dom_tree.parent_arr[bb_entry] == bb_entry);
  TEST_ASSERT(func->CFG_info.Dom_tree.parent_arr[bb_left] == bb_entry);
  TEST_ASSERT(func->CFG_info.Dom_tree.parent_arr[bb_right] == bb_entry);
  TEST_ASSERT(func->CFG_info.Dom_tree.parent_arr[bb_merge] == bb_entry);
  TEST_ASSERT(func->CFG_info.Dom_tree.parent_arr[bb_loop] == bb_merge);
  TEST_ASSERT(func->CFG_info.Dom_tree.parent_arr[bb_exit] == bb_merge);
  TEST_ASSERT(tree_has_child(&func->CFG_info.Dom_tree, bb_entry, bb_left));
  TEST_ASSERT(tree_has_child(&func->CFG_info.Dom_tree, bb_entry, bb_right));
  TEST_ASSERT(tree_has_child(&func->CFG_info.Dom_tree, bb_entry, bb_merge));
  TEST_ASSERT(tree_has_child(&func->CFG_info.Dom_tree, bb_merge, bb_loop));
  TEST_ASSERT(tree_has_child(&func->CFG_info.Dom_tree, bb_merge, bb_exit));

  fp = tmpfile();
  TEST_ASSERT(fp != NULL);
  TEST_ASSERT(SSA_dump_func_cfg_info_graphviz(module, fn, fp) == 0);
  TEST_ASSERT(stream_contains(fp, "digraph SSA_CFG_INFO"));
  TEST_ASSERT(stream_contains(fp, "function: dom_main"));
  TEST_ASSERT(stream_contains(fp, "DOM tree"));
  TEST_ASSERT(stream_contains(fp, "PDOM tree"));
  TEST_ASSERT(stream_contains(fp, "predecessors"));
  TEST_ASSERT(stream_contains(fp, "successors"));
  TEST_ASSERT(stream_contains(fp, "structure annotation"));
  TEST_ASSERT(stream_contains(fp, "role: loop-header"));
  TEST_ASSERT(stream_contains(fp, "role: loop-latch"));
  TEST_ASSERT(stream_contains(fp, "loop: 0"));
  TEST_ASSERT(stream_contains(fp, "f0_dom_bb0 -> f0_dom_bb1"));
  TEST_ASSERT(stream_contains(fp, "f0_dom_bb0 -> f0_dom_bb2"));
  TEST_ASSERT(stream_contains(fp, "f0_dom_bb0 -> f0_dom_bb3"));
  TEST_ASSERT(stream_contains(fp, "f0_dom_bb3 -> f0_dom_bb4"));
  TEST_ASSERT(stream_contains(fp, "f0_dom_bb3 -> f0_dom_bb5"));
  TEST_ASSERT(stream_contains(fp, "f0_succs_bb0 -> f0_succs_bb1"));
  TEST_ASSERT(stream_contains(fp, "f0_preds_bb0 -> f0_preds_bb1"));
  TEST_ASSERT(stream_contains(fp, "f0_struct_bb3 -> f0_struct_bb4"));
  fclose(fp);

  destroy_module(module);
  return 0;
}

static int test_structure_annotation_complex_cfgs(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSAFunc *func;
  const CFGLoop *cfg_loop;
  const CFGLoop *outer_cfg_loop;
  const CFGLoop *inner_cfg_loop;
  SSAValName cond;
  int loop;

  SSABasicBlockName entry;
  SSABasicBlockName preheader1;
  SSABasicBlockName h1;
  SSABasicBlockName body1;
  SSABasicBlockName side1;
  SSABasicBlockName latch1;
  SSABasicBlockName after1;
  SSABasicBlockName h2;
  SSABasicBlockName body2;
  SSABasicBlockName latch2;
  SSABasicBlockName exit;

  SSABasicBlockName header;
  SSABasicBlockName body_a;
  SSABasicBlockName left;
  SSABasicBlockName right;
  SSABasicBlockName latch;

  SSABasicBlockName outer_h;
  SSABasicBlockName outer_body;
  SSABasicBlockName inner_h;
  SSABasicBlockName inner_body;
  SSABasicBlockName inner_latch;
  SSABasicBlockName outer_latch;
  int outer_loop;
  int inner_loop;

  SSABasicBlockName latch_a;
  SSABasicBlockName latch_b;

  SSABasicBlockName blocks[12];
  SSABasicBlockName large[14];

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "structure_two_loops", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  entry = new_BB(module, fn);
  preheader1 = new_BB(module, fn);
  h1 = new_BB(module, fn);
  body1 = new_BB(module, fn);
  side1 = new_BB(module, fn);
  latch1 = new_BB(module, fn);
  after1 = new_BB(module, fn);
  h2 = new_BB(module, fn);
  body2 = new_BB(module, fn);
  latch2 = new_BB(module, fn);
  exit = new_BB(module, fn);
  TEST_ASSERT(set_entry_BB(module, fn, entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, exit) == 1);
  cond = emit_const_assign(module, fn, entry, SSA_i1, make_i1(1));
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, entry, preheader1) == 0);
  TEST_ASSERT(emit_goto(module, fn, preheader1, h1) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, h1, cond, body1, after1) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, body1, cond, latch1, side1) == 0);
  TEST_ASSERT(emit_goto(module, fn, side1, latch1) == 0);
  TEST_ASSERT(emit_goto(module, fn, latch1, h1) == 0);
  TEST_ASSERT(emit_goto(module, fn, after1, h2) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, h2, cond, body2, exit) == 0);
  TEST_ASSERT(emit_goto(module, fn, body2, latch2) == 0);
  TEST_ASSERT(emit_goto(module, fn, latch2, h2) == 0);
  TEST_ASSERT(emit_return(module, fn, exit, SSA_VALUE_VOID) == 0);
  TEST_ASSERT(require_CFG_structure_annotation(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.structure_annotation.loops_count == 2);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[h1].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch1].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[body1].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[side1].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[h2].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch2].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[body2].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[h1].parent_idx !=
              func->CFG_info.structure_annotation.block_roles[h2].parent_idx);
  cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, h1);
  TEST_ASSERT(cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(cfg_loop->latches) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->latches, latch1) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, h1) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, body1) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, side1) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, latch1) == 1);
  cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, h2);
  TEST_ASSERT(cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(cfg_loop->latches) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->latches, latch2) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, h2) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, body2) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, latch2) == 1);

  fn = new_func(module, "structure_diamond_loop", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  entry = new_BB(module, fn);
  header = new_BB(module, fn);
  body_a = new_BB(module, fn);
  left = new_BB(module, fn);
  right = new_BB(module, fn);
  latch = new_BB(module, fn);
  exit = new_BB(module, fn);
  TEST_ASSERT(set_entry_BB(module, fn, entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, exit) == 1);
  cond = emit_const_assign(module, fn, entry, SSA_i1, make_i1(1));
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, entry, header) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, header, cond, body_a, exit) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, body_a, cond, left, right) == 0);
  TEST_ASSERT(emit_goto(module, fn, left, latch) == 0);
  TEST_ASSERT(emit_goto(module, fn, right, latch) == 0);
  TEST_ASSERT(emit_goto(module, fn, latch, header) == 0);
  TEST_ASSERT(emit_return(module, fn, exit, SSA_VALUE_VOID) == 0);
  TEST_ASSERT(require_CFG_structure_annotation(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.valid_structure_annotation == 1);
  TEST_ASSERT(func->CFG_info.structure_annotation.loops_count == 1);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[header].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[body_a].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[left].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[right].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch].role == CFG_LOOP_LATCH);

  loop = func->CFG_info.structure_annotation.block_roles[header].parent_idx;
  TEST_ASSERT(loop >= 0);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[body_a].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[left].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[right].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch].parent_idx == loop);
  cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, header);
  TEST_ASSERT(cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(cfg_loop->latches) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->latches, latch) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, header) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, body_a) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, left) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, right) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, latch) == 1);

  fn = new_func(module, "structure_nested_loops", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  entry = new_BB(module, fn);
  outer_h = new_BB(module, fn);
  outer_body = new_BB(module, fn);
  inner_h = new_BB(module, fn);
  inner_body = new_BB(module, fn);
  inner_latch = new_BB(module, fn);
  outer_latch = new_BB(module, fn);
  exit = new_BB(module, fn);
  TEST_ASSERT(set_entry_BB(module, fn, entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, exit) == 1);
  cond = emit_const_assign(module, fn, entry, SSA_i1, make_i1(1));
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, entry, outer_h) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, outer_h, cond, outer_body, exit) == 0);
  TEST_ASSERT(emit_goto(module, fn, outer_body, inner_h) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, inner_h, cond, inner_body, outer_latch) == 0);
  TEST_ASSERT(emit_goto(module, fn, inner_body, inner_latch) == 0);
  TEST_ASSERT(emit_goto(module, fn, inner_latch, inner_h) == 0);
  TEST_ASSERT(emit_goto(module, fn, outer_latch, outer_h) == 0);
  TEST_ASSERT(emit_return(module, fn, exit, SSA_VALUE_VOID) == 0);
  TEST_ASSERT(require_Dom_tree(module, fn) == 1);
  TEST_ASSERT(require_PDom_tree(module, fn) == 1);
  TEST_ASSERT(require_CFG_structure_annotation(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.structure_annotation.loops_count == 2);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[outer_h].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[outer_body].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[outer_latch].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[inner_h].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[inner_body].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[inner_latch].role == CFG_LOOP_LATCH);
  outer_loop = func->CFG_info.structure_annotation.block_roles[outer_h].parent_idx;
  inner_loop = func->CFG_info.structure_annotation.block_roles[inner_h].parent_idx;
  TEST_ASSERT(outer_loop >= 0);
  TEST_ASSERT(inner_loop >= 0);
  TEST_ASSERT(outer_loop != inner_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[outer_body].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[outer_latch].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[inner_body].parent_idx == inner_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[inner_latch].parent_idx == inner_loop);
  outer_cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, outer_h);
  inner_cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, inner_h);
  TEST_ASSERT(outer_cfg_loop != NULL);
  TEST_ASSERT(inner_cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(outer_cfg_loop->latches) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->latches, outer_latch) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, outer_h) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, outer_body) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, inner_h) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, inner_body) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, inner_latch) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, outer_latch) == 1);
  TEST_ASSERT(bb_list_count(inner_cfg_loop->latches) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->latches, inner_latch) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, inner_h) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, inner_body) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, inner_latch) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, outer_h) == 0);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, outer_latch) == 0);

  fn = new_func(module, "structure_multi_latch_loop", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  entry = new_BB(module, fn);
  header = new_BB(module, fn);
  body_a = new_BB(module, fn);
  left = new_BB(module, fn);
  right = new_BB(module, fn);
  latch_a = new_BB(module, fn);
  latch_b = new_BB(module, fn);
  exit = new_BB(module, fn);
  TEST_ASSERT(set_entry_BB(module, fn, entry) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, exit) == 1);
  cond = emit_const_assign(module, fn, entry, SSA_i1, make_i1(1));
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, entry, header) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, header, cond, body_a, exit) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, body_a, cond, left, right) == 0);
  TEST_ASSERT(emit_goto(module, fn, left, latch_a) == 0);
  TEST_ASSERT(emit_goto(module, fn, right, latch_b) == 0);
  TEST_ASSERT(emit_goto(module, fn, latch_a, header) == 0);
  TEST_ASSERT(emit_goto(module, fn, latch_b, header) == 0);
  TEST_ASSERT(emit_return(module, fn, exit, SSA_VALUE_VOID) == 0);
  TEST_ASSERT(require_CFG_structure_annotation(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.structure_annotation.loops_count == 1);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[header].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch_a].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch_b].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[body_a].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[left].role == CFG_LOOP_BODY);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[right].role == CFG_LOOP_BODY);
  loop = func->CFG_info.structure_annotation.block_roles[header].parent_idx;
  TEST_ASSERT(loop >= 0);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[body_a].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[left].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[right].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch_a].parent_idx == loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[latch_b].parent_idx == loop);
  cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, header);
  TEST_ASSERT(cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(cfg_loop->latches) == 2);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->latches, latch_a) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->latches, latch_b) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, header) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, body_a) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, left) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, right) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, latch_a) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->body, latch_b) == 1);

  fn = new_func(module, "structure_big_goto_loop", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  for (int i = 0; i < 12; ++i)
  {
    blocks[i] = new_BB(module, fn);
    TEST_ASSERT(blocks[i] != SSA_INVALID_BB);
  }
  exit = new_BB(module, fn);
  TEST_ASSERT(exit != SSA_INVALID_BB);
  TEST_ASSERT(set_entry_BB(module, fn, blocks[0]) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, exit) == 1);
  for (int i = 0; i < 11; ++i)
    TEST_ASSERT(emit_goto(module, fn, blocks[i], blocks[i + 1]) == 0);
  TEST_ASSERT(emit_goto(module, fn, blocks[11], blocks[0]) == 0);
  TEST_ASSERT(emit_return(module, fn, exit, SSA_VALUE_VOID) == 0);
  TEST_ASSERT(require_CFG_structure_annotation(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.structure_annotation.loops_count == 1);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[blocks[0]].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[blocks[11]].role == CFG_LOOP_LATCH);
  for (int i = 1; i < 11; ++i)
    TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[blocks[i]].role == CFG_LOOP_BODY);
  cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, blocks[0]);
  TEST_ASSERT(cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(cfg_loop->latches) == 1);
  TEST_ASSERT(bb_list_count_bb(cfg_loop->latches, blocks[11]) == 1);
  for (int i = 0; i < 12; ++i)
    TEST_ASSERT(bb_list_count_bb(cfg_loop->body, blocks[i]) == 1);

  fn = new_func(module, "structure_large_reducible_cfg", SSA_void, 0, NULL, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);
  for (int i = 0; i < 14; ++i)
  {
    large[i] = new_BB(module, fn);
    TEST_ASSERT(large[i] != SSA_INVALID_BB);
  }
  TEST_ASSERT(set_entry_BB(module, fn, large[0]) == 1);
  TEST_ASSERT(set_exit_BB(module, fn, large[13]) == 1);
  cond = emit_const_assign(module, fn, large[0], SSA_i1, make_i1(1));
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, large[0], large[1]) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, large[1], cond, large[2], large[13]) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, large[2], cond, large[3], large[4]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[3], large[4]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[4], large[5]) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, large[5], cond, large[6], large[10]) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, large[6], cond, large[7], large[8]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[7], large[9]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[8], large[5]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[9], large[5]) == 0);
  TEST_ASSERT(emit_cond_goto(module, fn, large[10], cond, large[11], large[12]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[11], large[1]) == 0);
  TEST_ASSERT(emit_goto(module, fn, large[12], large[1]) == 0);
  TEST_ASSERT(emit_return(module, fn, large[13], SSA_VALUE_VOID) == 0);
  TEST_ASSERT(require_CFG_structure_annotation(module, fn) == 1);
  func = &module->functions[fn];
  TEST_ASSERT(func->CFG_info.structure_annotation.loops_count == 2);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[1]].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[5]].role == CFG_LOOP_HEADER);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[11]].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[12]].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[8]].role == CFG_LOOP_LATCH);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[9]].role == CFG_LOOP_LATCH);
  outer_loop = func->CFG_info.structure_annotation.block_roles[large[1]].parent_idx;
  inner_loop = func->CFG_info.structure_annotation.block_roles[large[5]].parent_idx;
  TEST_ASSERT(outer_loop >= 0);
  TEST_ASSERT(inner_loop >= 0);
  TEST_ASSERT(outer_loop != inner_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[2]].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[3]].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[4]].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[10]].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[11]].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[12]].parent_idx == outer_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[6]].parent_idx == inner_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[7]].parent_idx == inner_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[8]].parent_idx == inner_loop);
  TEST_ASSERT(func->CFG_info.structure_annotation.block_roles[large[9]].parent_idx == inner_loop);
  outer_cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, large[1]);
  inner_cfg_loop = find_loop_by_header(&func->CFG_info.structure_annotation, large[5]);
  TEST_ASSERT(outer_cfg_loop != NULL);
  TEST_ASSERT(inner_cfg_loop != NULL);
  TEST_ASSERT(bb_list_count(outer_cfg_loop->latches) == 2);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->latches, large[11]) == 1);
  TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->latches, large[12]) == 1);
  for (int i = 1; i <= 12; ++i)
    TEST_ASSERT(bb_list_count_bb(outer_cfg_loop->body, large[i]) == 1);
  TEST_ASSERT(bb_list_count(inner_cfg_loop->latches) == 2);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->latches, large[8]) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->latches, large[9]) == 1);
  for (int i = 5; i <= 9; ++i)
    TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, large[i]) == 1);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, large[1]) == 0);
  TEST_ASSERT(bb_list_count_bb(inner_cfg_loop->body, large[10]) == 0);

  TEST_ASSERT(dump_cfg_info_test_case("structure_annotation", module) == 0);

  destroy_module(module);
  return 0;
}

typedef int (*test_fn)(void);

typedef struct
{
  const char *name;
  test_fn fn;
} TestCase;

int main(void)
{
  const TestCase tests[] = {
      {"simple_call_and_return", test_simple_call_and_return},
      {"branch_and_phi", test_branch_and_phi},
      {"invalid_inputs", test_invalid_inputs},
      {"const_assign", test_const_assign},
      {"void_value", test_void_value},
      {"dump_func_args", test_dump_func_args},
      {"bool_cast_dump", test_bool_cast_dump},
      {"dom_tree_and_dump", test_dom_tree_and_dump},
      {"structure_annotation_complex_cfgs", test_structure_annotation_complex_cfgs},
  };
  int failed = 0;
  size_t i;

  for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
  {
    int rc;
    printf("[RUN ] %s\n", tests[i].name);
    rc = tests[i].fn();
    if (rc == 0)
      printf("[PASS] %s\n", tests[i].name);
    else
    {
      printf("[FAIL] %s\n", tests[i].name);
      failed += 1;
    }
  }

  printf("\nTotal: %zu, passed: %zu, failed: %d\n",
         sizeof(tests) / sizeof(tests[0]),
         (sizeof(tests) / sizeof(tests[0])) - (size_t)failed,
         failed);

  return failed == 0 ? 0 : 1;
}
