#include "SSA.h"
#include <stdio.h>

#define TEST_ASSERT(cond)                                                            \
  do                                                                                 \
  {                                                                                  \
    if (!(cond))                                                                     \
    {                                                                                \
      fprintf(stderr, "    assert failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
      return -1;                                                                     \
    }                                                                                \
  } while (0)

static int test_simple_call_and_goto(void)
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

  callee = new_func(module, "callee", 1);
  TEST_ASSERT(callee != SSA_INVALID_FUNC);

  caller = new_func(module, "main", 1);
  TEST_ASSERT(caller != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, caller, 1, 0);
  bb_exit = new_BB(module, caller, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);

  caller_fn = &module->functions[caller];
  TEST_ASSERT(caller_fn->entry_block == bb_entry);
  TEST_ASSERT(caller_fn->exit_block == bb_exit);

  args = new_ArgList(module, caller, bb_entry);
  TEST_ASSERT(args != NULL);

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
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.true_dst == bb_exit);
  TEST_ASSERT(caller_fn->basic_blocks[bb_entry].terminator.false_dst == SSA_INVALID_BB);

  TEST_ASSERT(emit_goto(module, caller, bb_entry, bb_exit) < 0);

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

  cond_fn = new_func(module, "cond_provider", 1);
  calc_fn = new_func(module, "calc", 1);
  main_fn_name = new_func(module, "branch_main", 1);
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

  args_cond = new_ArgList(module, main_fn_name, bb_entry);
  TEST_ASSERT(args_cond != NULL);
  cond_val = emit_call_assign(module, main_fn_name, bb_entry, cond_fn, args_cond, 0);
  TEST_ASSERT(cond_val != SSA_INVALID_VAL);

  TEST_ASSERT(emit_cond_goto(module, main_fn_name, bb_entry, cond_val, bb_true, bb_false) == 0);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.type == SSA_TERM_COND_GOTO);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.cond == cond_val);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.true_dst == bb_true);
  TEST_ASSERT(main_fn->basic_blocks[bb_entry].terminator.false_dst == bb_false);

  args_true = new_ArgList(module, main_fn_name, bb_true);
  TEST_ASSERT(args_true != NULL);
  TEST_ASSERT(ArgList_append(args_true, cond_val) == 0);
  true_val = emit_call_assign(module, main_fn_name, bb_true, calc_fn, args_true, 0);
  TEST_ASSERT(true_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_true, bb_merge) == 0);

  args_false = new_ArgList(module, main_fn_name, bb_false);
  TEST_ASSERT(args_false != NULL);
  false_val = emit_call_assign(module, main_fn_name, bb_false, calc_fn, args_false, 0);
  TEST_ASSERT(false_val != SSA_INVALID_VAL);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_false, bb_merge) == 0);

  phi = new_PhiList(module, main_fn_name, bb_merge);
  TEST_ASSERT(phi != NULL);
  TEST_ASSERT(PhiList_append(phi, (PhiPair){.previous_block_name = bb_true, .value_name = true_val}) == 0);
  TEST_ASSERT(PhiList_append(phi, (PhiPair){.previous_block_name = bb_false, .value_name = false_val}) == 0);

  phi_val = emit_phi_assign(module, main_fn_name, bb_merge, phi);
  TEST_ASSERT(phi_val != SSA_INVALID_VAL);
  TEST_ASSERT(main_fn->values[phi_val].expr.phi.options == phi);
  TEST_ASSERT(main_fn->values[phi_val].parent_name == bb_merge);

  TEST_ASSERT(emit_cond_goto(module, main_fn_name, bb_entry, cond_val, bb_true, bb_false) < 0);
  TEST_ASSERT(emit_goto(module, main_fn_name, bb_true, bb_merge) < 0);

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

  module = new_module();
  TEST_ASSERT(module != NULL);

  fn = new_func(module, "invalid_case", 0);
  TEST_ASSERT(fn != SSA_INVALID_FUNC);

  bb_entry = new_BB(module, fn, 1, 0);
  bb_exit = new_BB(module, fn, 0, 1);
  TEST_ASSERT(bb_entry != SSA_INVALID_BB);
  TEST_ASSERT(bb_exit != SSA_INVALID_BB);
  TEST_ASSERT(new_BB(module, fn, 1, 0) == SSA_INVALID_BB);
  TEST_ASSERT(new_BB(module, fn, 0, 1) == SSA_INVALID_BB);

  args = new_ArgList(module, fn, bb_entry);
  TEST_ASSERT(args != NULL);

  TEST_ASSERT(emit_call_assign(module, fn, bb_entry, SSA_INVALID_FUNC, args, 0) == SSA_INVALID_VAL);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, SSA_INVALID_VAL, bb_entry, bb_exit) < 0);

  cond = emit_call_assign(module, fn, bb_entry, fn, args, 0);
  TEST_ASSERT(cond != SSA_INVALID_VAL);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, SSA_INVALID_BB, bb_exit) < 0);
  TEST_ASSERT(emit_cond_goto(module, fn, bb_entry, cond, bb_entry, bb_exit) == 0);
  TEST_ASSERT(emit_goto(module, fn, bb_entry, bb_exit) < 0);

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
      {"simple_call_and_goto", test_simple_call_and_goto},
      {"branch_and_phi", test_branch_and_phi},
      {"invalid_inputs", test_invalid_inputs},
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
