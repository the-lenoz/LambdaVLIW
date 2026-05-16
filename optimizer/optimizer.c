#include "optimizer.h"
#include "../SSA/SSA.h"
#include <stdlib.h>

static int is_simply_returned(SSAModule *module, SSAFuncName func, SSAValName val)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;
  SSAInstrName instr = get_val_declaration_instr(module, func, val);
  if (instr == SSA_INVALID_INSTR)
    return 0;

  SSAInstrList *usages = find_all_val_usages(module, func, val);
  if (!usages || usages->next) // not only returned
    return 0;
  SSAInstrName usage_instr = usages->instr;
  SSAInstrList_destroy(usages);

  instr = instr->next;
  while (instr)
    switch (instr->kind)
    {
    case SSA_INSTR_VOID_CALL:
      return 0;
    case SSA_INSTR_VAL:
      if (function->values[instr->val].kind != SSA_VALUE_PHI)
        return 0;
      if (instr != usage_instr) // phi does not contain value
        return 0;
      return is_simply_returned(module, func, instr->val);
    case SSA_INSTR_TERM:
      if (instr->term.type == SSA_TERM_RETURN)
      {
        if (instr == usage_instr)
          return 1;
        else
          return 0; // returns other value
      }
      if (instr->term.type == SSA_TERM_GOTO)
      {
        instr = function->basic_blocks[instr->term.true_dst].first_instr;
        continue;
      }
      return 0;
    }
  return 0;
}

static SSAInstrList *find_self_tail_calls(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return NULL;

  SSAInstrList *result = NULL;

  for (SSAValName val = 0; val < function->values_count; ++val)
  {
    SSAInstrName instr = get_val_declaration_instr(module, func, val);
    if (function->values[val].kind != SSA_VALUE_CALL)
      continue;
    if (function->values[val].expr.call.calee_name != func)
      continue; // only self-calls
    if (is_simply_returned(module, func, val))
      SSAInstrList_append(&result, instr);
  }
  return result;
}

int self_TCO_func(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  SSAInstrList *tail_calls = find_self_tail_calls(module, func);
  if (!tail_calls)
    return 0;

  SSABasicBlockName new_entry = new_BB(module, func);
  SSABasicBlockName TCO_header = new_BB(module, func);
  emit_goto(module, func, new_entry, TCO_header);

  SSAValName *arg_phis = calloc(function->args_count, sizeof(SSAValName));
  for (int i = 0; i < function->args_count; i++)
  {
    arg_phis[i] = emit_phi_assign(module, func, TCO_header, function->arg_types[i]);
    rename_all_val_uses(module, func, function->arg_SSA_names[i], arg_phis[i]);
    add_phi_option(module, func, arg_phis[i], (PhiPair){new_entry, function->arg_SSA_names[i]});
  }
  emit_goto(module, func, TCO_header, function->entry_block);
  set_entry_BB(module, func, new_entry);

  for (SSAInstrList *tc = tail_calls; tc; tc = tc->next)
  {
    SSAInstrName call_i = tc->instr;
    _FuncCall call = function->values[call_i->val].expr.call;
    SSABasicBlockName BB = get_val_declaration_BB(module, func, call_i->val);

    int i = 0;
    for (ArgList *args = call.args; args; args = args->next, i++)
      add_phi_option(module, func, arg_phis[i], (PhiPair){BB, args->name});
    clear_BB_terminator(module, func, BB);
    replace_instr(module, func, BB, call_i,
                  (SSAInstr){
                      SSA_INSTR_TERM,
                      SSA_INVALID_VAL,
                      (_FuncCall){},
                      (SSABlockTerminator){
                          SSA_TERM_GOTO,
                          SSA_INVALID_VAL,
                          SSA_INVALID_VAL,
                          TCO_header,
                          SSA_INVALID_BB},
                      SSA_INVALID_INSTR,
			SSA_INVALID_INSTR});
  }

  free(arg_phis);
  SSAInstrList_destroy(tail_calls);
  return 1;
}

int self_TCO_module(SSAModule *module)
{
  if (!module)
    return 0;
  int optimized = 0;
  for (SSAFuncName i = 0; i < module->functions_count; ++i)
  {
    optimized += self_TCO_func(module, i);
  }
  return optimized;
}

int optimize_module(SSAModule *module)
{
  int result = 0;
  result += self_TCO_module(module);
  return result;
}
