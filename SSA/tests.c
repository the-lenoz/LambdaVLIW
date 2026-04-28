#include "SSA.h"
#include "SSA_dump_graphviz.h"
#include "SSA_to_L_tri_call.h"
#include <limits.h>
#include <stdio.h>
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

static int test_simple_call_and_return(void)
{
  SSAModule *module;
  SSAFuncName callee;
  SSAFuncName caller;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  ArgList *args;
  SSAValName call_val;
  SSAFunc *caller_fn;

  module = new_module();
  TEST_ASSERT(module != NULL);

  callee = new_func(module, "callee", 0, 1);
  TEST_ASSERT(callee != SSA_INVALID_FUNC);

  caller = new_func(module, "main", 0, 1);
  TEST_ASSERT(caller != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, caller, 1, 0);
  bb_exit = new_BB(module, caller, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);

  caller_fn = &module->functions[caller];
  TEST_ASSERT(caller_fn->entry_block == bb_entry);
  TEST_ASSERT(caller_fn->exit_block == bb_exit);

  args = NULL;

  call_val = emit_call_assign(module, caller, bb_entry, callee, args, 1);
  TEST_ASSERT(call_val != SSA_INVALID_VAL);
  TEST_ASSERT(caller_fn->values_count == 1);
  TEST_ASSERT(caller_fn->values[call_val].is_const == 1);
  TEST_ASSERT(caller_fn->values[call_val].expr.call.calee_name == callee);
  TEST_ASSERT(caller_fn->values[call_val].expr.call.args == args);
  TEST_ASSERT(caller_fn->values[call_val].parent_name == bb_entry);

  TEST_ASSERT(emit_goto(module, caller, bb_entry, bb_exit) == 0);
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.type == SSA_TERM_GOTO);
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.cond == SSA_INVALID_VAL);
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.ret_val == SSA_INVALID_VAL);
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.true_dst == bb_exit);
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.false_dst == SSA_INVALID_BB);

  TEST_ASSERT(emit_goto(module, caller, bb_entry, bb_exit) < 0);

  TEST_ASSERT(emit_return(module, caller, bb_exit, call_val) == 0);
  TEST_ASSERT(caller_fn->basic_blocks[bb_exit].terminator.type == SSA_TERM_RETURN);
  TEST_ASSERT(caller_fn->basic_blocks[bb_exit].terminator.cond == SSA_INVALID_VAL);
  TEST_ASSERT(caller_fn->basic_blocks[bb_exit].terminator.ret_val == call_val);
  TEST_ASSERT(caller_fn->basic_blocks[bb_exit].terminator.true_dst == SSA_INVALID_BB);
  TEST_ASSERT(caller_fn->basic_blocks[bb_exit].terminator.false_dst == SSA_INVALID_BB);

  TEST_ASSERT(emit_return(module, caller, bb_exit, call_val) < 0);

  TEST_ASSERT(dump_test_case("simple_call_and_return", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_branch_and_phi(void)
{
  SSAModule *module;
  SSAFuncName cond_fn;
  SSAFuncName calc_fn;
  SSAFuncName main_fn_name;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_true;
  SSABasicBlockName bb_false;
  SSABasicBlockName bb_merge;
  ArgList *args_cond;
  ArgList *args_true;
  ArgList *args_false;
  PhiList *phi;
  SSAValName cond_val;
  SSAValName true_val;
  SSAValName false_val;
  SSAValName phi_val;
  SSAFunc *main_fn;

  module = new_module();
  TEST_ASSERT(module != NULL);

  cond_fn = new_func(module, "cond_provider", 0, 1);
  calc_fn = new_func(module, "calc", 1, 1);
  main_fn_name = new_func(module, "branch_main", 0, 1);
  TEST_ASSERT(cond_fn != SSA_INVALID_FUNC);
  TEST_ASSERT(calc_fn != SSA_INVALID_FUNC);
  TEST_ASSERT(main_fn_name != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, main_fn_name, 1, 0);
  bb_true = new_BB(module, main_fn_name, 0, 0);
  bb_false = new_BB(module, main_fn_name, 0, 0);
  bb_merge = new_BB(module, main_fn_name, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_true != SSA_INVALID_BB);
  TEST_ASSERT(bb_false != SSA_INVALID_BB);
  TEST_ASSERT(bb_merge != SSA_INVALID_BB);

  main_fn = &module->functions[main_fn_name];

  args_cond = NULL;
  cond_val = emit_call_assign(module, main_fn_name, bb_entry, cond_fn, args_cond, 0);
  TEST_ASSERT(cond_val != SSA_INVALID_VAL);

  TEST_ASSERT(emit_cond_goto(module, main_fn_name, bb_entry, cond_val, bb_true, bb_false) == 0);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.type == SSA_TERM_COND_GOTO);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.cond == cond_val);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.true_dst == bb_true);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.false_dst == bb_false);

  args_true = NULL;
  TEST_ASSERT(ArgList_append(&args_true, cond_val) == 0);
  true_val = emit_call_assign(module, main_fn_name, bb_true, calc_fn, args_true, 0);
  TEST_ASSERT(true_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_true, bb_merge) == 0);

  args_false = NULL;
  false_val = emit_call_assign(module, main_fn_name, bb_false, calc_fn, args_false, 0);
  TEST_ASSERT(false_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_false, bb_merge) == 0);

  phi = NULL;
  TEST_ASSERT(PhiList_append(&phi, (PhiPair){.previous_block_name = bb_true, .value_name = true_val}) == 0);
  TEST_ASSERT(PhiList_append(&phi, (PhiPair){.previous_block_name = bb_false, .value_name = false_val}) == 0);

  phi_val = emit_phi_assign(module, main_fn_name, bb_merge, phi);
  TEST_ASSERT(phi_val != SSA_INVALID_VAL);
  TEST_ASSERT(main_fn->values[phi_val].expr.phi.options == phi);
  TEST_ASSERT(main_fn->values[phi_val].parent_name == bb_merge);

  TEST_ASSERT(emit_return(module, main_fn_name, bb_merge, phi_val) == 0);
  TEST_ASSERT(main_fn->basic_blocks[bb_merge].terminator.type == SSA_TERM_RETURN);
  TEST_ASSERT(main_fn->basic_blocks[bb_merge].terminator.ret_val == phi_val);
  TEST_ASSERT(main_fn->basic_blocks[bb_merge].terminator.cond == SSA_INVALID_VAL);
  TEST_ASSERT(main_fn->basic_blocks[bb_merge].terminator.true_dst == SSA_INVALID_BB);
  TEST_ASSERT(main_fn->basic_blocks[bb_merge].terminator.false_dst == SSA_INVALID_BB);

  TEST_ASSERT(emit_cond_goto(module, main_fn_name, bb_entry, cond_val, bb_true, bb_false) < 0);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_true, bb_merge) < 0);
  TEST_ASSERT(emit_return(module, main_fn_name, bb_merge, phi_val) < 0);

  TEST_ASSERT(dump_test_case("branch_and_phi", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_invalid_inputs(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  SSABasicBlockName bb_exit;
  ArgList *args;
  SSAValName cond;
  SSAValName retv;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "invalid_case", 0, 0);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn, 1, 0);
  bb_exit = new_BB(module, fn, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(new_BB(module, fn, 1, 0) == SSA_INVALID_BB);
  TEST_ASSERT(new_BB(module, fn, 0, 1) == SSA_INVALID_BB);

  args = NULL;

  TEST_ASSERT(emit_call_assign(module, fn, bb_entry, SSA_INVALID_FUNC, args, 0) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, SSA_INVALID_VAL, bb_entry, bb_exit) < 0);

  cond = emit_call_assign(module, fn, bb_entry, fn, args, 0);
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  retv = emit_call_assign(module, fn, bb_exit, fn, args, 0);
  TEST_ASSERT(retv != SSA_INVALID_VAL);

  TEST_ASSERT(emit_return(module, fn, bb_entry, cond) < 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, SSA_INVALID_VAL) < 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, retv) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, retv) < 0);
  TEST_ASSERT(emit_goto(module, fn, bb_exit, bb_entry) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_exit, cond, bb_entry, bb_exit) < 0);

  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, SSA_INVALID_BB, bb_exit) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) < 0);

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

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "const_main", 0, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn, 1, 0);
  bb_exit = new_BB(module, fn, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);

  f = &module->functions[fn];

  v0 = emit_const_assign(module, fn, bb_entry, 42);
  v1 = emit_const_assign(module, fn, bb_entry, INT_MAX);
  v2 = emit_const_assign(module, fn, bb_exit, INT_MIN);
  TEST_ASSERT(v0 != SSA_INVALID_VAL);
  TEST_ASSERT(v1 != SSA_INVALID_VAL);
  TEST_ASSERT(v2 != SSA_INVALID_VAL);

  TEST_ASSERT(f->values_count == 3);
  TEST_ASSERT(f->values[v0].type == SSA_VALUE_CONST);
  TEST_ASSERT(f->values[v1].type == SSA_VALUE_CONST);
  TEST_ASSERT(f->values[v2].type == SSA_VALUE_CONST);
  TEST_ASSERT(f->values[v0].is_const == 1);
  TEST_ASSERT(f->values[v1].is_const == 1);
  TEST_ASSERT(f->values[v2].is_const == 1);
  TEST_ASSERT(f->values[v0].expr.cnst.value == 42);
  TEST_ASSERT(f->values[v1].expr.cnst.value == INT_MAX);
  TEST_ASSERT(f->values[v2].expr.cnst.value == INT_MIN);
  TEST_ASSERT(f->values[v0].parent_name == bb_entry);
  TEST_ASSERT(f->values[v2].parent_name == bb_exit);

  TEST_ASSERT(emit_const_assign(module, fn, SSA_INVALID_BB, 7) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, v2) == 0);

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

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "void_main", 0, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn, 1, 0);
  bb_exit = new_BB(module, fn, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);

  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_return(module, fn, bb_exit, SSA_VAL_VOID) == 0);

  TEST_ASSERT(dump_test_case("void_value", module) == 0);

  destroy_module(module);
  return 0;
}

static int test_dump_func_args(void)
{
  SSAModule *module;
  SSAFuncName fn;
  SSABasicBlockName bb_entry;
  FILE *fp;

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "with_args", 2, 1);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn, 1, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(emit_return(module, fn, bb_entry, SSA_VAL_VOID) == 0);

  fp = tmpfile();
  TEST_ASSERT(fp != NULL);
  TEST_ASSERT(SSA_to_L_tri_call_module(module, fp) == 0);
  TEST_ASSERT(stream_contains(fp, "(func with_args v0 v1"));
  fclose(fp);

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
