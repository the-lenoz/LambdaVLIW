#include "SSA_to_L_tri_call.h"

static const SSAFunc *dump_get_func(const SSAModule *module, SSAFuncName func)
{
  if (!module || func >= module->functions_count)
    return NULL;
  return &module->functions[func];
}

static int dump_print_val_name(FILE *out_fp, SSAValName name)
{
  if (name == SSA_VALUE_VOID)
    return fprintf(out_fp, "nil") < 0 ? -1 : 0;
  return fprintf(out_fp, "v%u", name) < 0 ? -1 : 0;
}

static int dump_print_bb_name(FILE *out_fp, SSABasicBlockName bb)
{
  return fprintf(out_fp, "bb%u", bb) < 0 ? -1 : 0;
}

static int dump_print_func_args(FILE *out_fp, const SSAFunc *func)
{
  if (!func)
    return -1;

  for (unsigned int i = 0; i < func->args_count; ++i)
  {
    if (func->values[i].kind != SSA_VALUE_ARG)
      return -1;
    if (fprintf(out_fp, " ") < 0 || dump_print_val_name(out_fp, (SSAValName)i) < 0)
      return -1;
  }

  return 0;
}

static int dump_print_args(FILE *out_fp, const ArgList *args)
{
  for (const ArgList *it = args; it; it = it->next)
  {
    if (it->name == SSA_INVALID_VAL)
      return -1;
    if (fprintf(out_fp, " ") < 0 || dump_print_val_name(out_fp, it->name) < 0)
      return -1;
  }

  return 0;
}

static int dump_print_const(FILE *out_fp, SSAValueType type, SSAConst value)
{
  switch (type)
  {
  case SSA_i1:
    return fprintf(out_fp, "%d", value.i1_value ? 1 : 0) < 0 ? -1 : 0;
  case SSA_i8:
    return fprintf(out_fp, "%d", (int)value.i8_value) < 0 ? -1 : 0;
  case SSA_i32:
    return fprintf(out_fp, "%d", value.i32_value) < 0 ? -1 : 0;
  case SSA_i64:
    return fprintf(out_fp, "%lld", (long long)value.i64_value) < 0 ? -1 : 0;
  case SSA_fp32:
    return fprintf(out_fp, "%g", value.fp32_value) < 0 ? -1 : 0;
  case SSA_fp64:
    return fprintf(out_fp, "%g", value.fp64_value) < 0 ? -1 : 0;
  default:
    return -1;
  }
}

static int dump_print_phi(FILE *out_fp, const PhiList *options)
{
  if (fprintf(out_fp, "(phi") < 0)
    return -1;

  for (const PhiList *it = options; it; it = it->next)
  {
    if (it->pair.previous_block_name == SSA_INVALID_BB || it->pair.value_name == SSA_INVALID_VAL)
      return -1;

    if (fprintf(out_fp, " (") < 0 || dump_print_bb_name(out_fp, it->pair.previous_block_name) < 0 ||
        fprintf(out_fp, " ") < 0 || dump_print_val_name(out_fp, it->pair.value_name) < 0 ||
        fprintf(out_fp, ")") < 0)
      return -1;
  }

  return fprintf(out_fp, ")") < 0 ? -1 : 0;
}

static int dump_emit_call(FILE *out_fp, const SSAModule *module, const _FuncCall *call)
{
  const SSAFunc *callee;

  if (!module || !call)
    return -1;

  callee = dump_get_func(module, call->calee_name);
  if (!callee || !callee->name)
    return -1;

  if (fprintf(out_fp, "(call %s", callee->name) < 0 || dump_print_args(out_fp, call->args) < 0 ||
      fprintf(out_fp, ")") < 0)
    return -1;

  return 0;
}

static int dump_emit_value(FILE *out_fp, const SSAModule *module, const SSAFunc *func, SSAValName value_name)
{
  const SSAValue *value;

  if (!func || value_name >= func->values_count)
    return -1;

  value = &func->values[value_name];
  if (value->kind == SSA_VALUE_ARG)
    return -1;

  if (fprintf(out_fp, "    (let ") < 0 || dump_print_val_name(out_fp, value_name) < 0 || fprintf(out_fp, " ") < 0)
    return -1;

  switch (value->kind)
  {
  case SSA_VALUE_PHI:
    if (dump_print_phi(out_fp, value->expr.phi.options) < 0)
      return -1;
    break;
  case SSA_VALUE_CONST:
    if (dump_print_const(out_fp, value->type, value->expr.cnst) < 0)
      return -1;
    break;
  case SSA_VALUE_CALL:
    if (dump_emit_call(out_fp, module, &value->expr.call) < 0)
      return -1;
    break;
  default:
    return -1;
  }

  return fprintf(out_fp, ")\n") < 0 ? -1 : 0;
}

static int dump_emit_void_call(FILE *out_fp, const SSAModule *module, const _FuncCall *call)
{
  if (fprintf(out_fp, "    ") < 0 || dump_emit_call(out_fp, module, call) < 0 || fprintf(out_fp, "\n") < 0)
    return -1;
  return 0;
}

static int dump_emit_terminator(FILE *out_fp, const SSABlockTerminator *term)
{
  if (!term)
    return -1;

  switch (term->type)
  {
  case SSA_TERM_NONE:
    return 0;
  case SSA_TERM_GOTO:
    if (term->true_dst == SSA_INVALID_BB)
      return -1;
    if (fprintf(out_fp, "    (goto ") < 0 || dump_print_bb_name(out_fp, term->true_dst) < 0 ||
        fprintf(out_fp, ")\n") < 0)
      return -1;
    return 0;
  case SSA_TERM_COND_GOTO:
    if (term->cond == SSA_INVALID_VAL || term->true_dst == SSA_INVALID_BB || term->false_dst == SSA_INVALID_BB)
      return -1;
    if (fprintf(out_fp, "    (if-goto ") < 0 || dump_print_val_name(out_fp, term->cond) < 0 ||
        fprintf(out_fp, " ") < 0 || dump_print_bb_name(out_fp, term->true_dst) < 0 || fprintf(out_fp, " else ") < 0 ||
        dump_print_bb_name(out_fp, term->false_dst) < 0 || fprintf(out_fp, ")\n") < 0)
      return -1;
    return 0;
  case SSA_TERM_RETURN:
    if (fprintf(out_fp, "    (return ") < 0 || dump_print_val_name(out_fp, term->ret_val) < 0 ||
        fprintf(out_fp, ")\n") < 0)
      return -1;
    return 0;
  default:
    return -1;
  }
}

static int dump_emit_instruction(FILE *out_fp, const SSAModule *module, const SSAFunc *func, const SSAInstr *instr)
{
  if (!instr)
    return -1;

  switch (instr->kind)
  {
  case SSA_INSTR_VAL:
    return dump_emit_value(out_fp, module, func, instr->val);
  case SSA_INSTR_VOID_CALL:
    return dump_emit_void_call(out_fp, module, &instr->call);
  case SSA_INSTR_TERM:
    return dump_emit_terminator(out_fp, &instr->term);
  default:
    return -1;
  }
}

static int dump_emit_block(FILE *out_fp, const SSAModule *module, const SSAFunc *func, SSABasicBlockName bb)
{
  const SSABasicBlock *block;

  if (!func || bb >= func->basic_blocks_count)
    return -1;

  block = &func->basic_blocks[bb];
  if (fprintf(out_fp, "    (") < 0 || dump_print_bb_name(out_fp, bb) < 0 || fprintf(out_fp, "\n") < 0)
    return -1;

  for (unsigned int i = 0; i < block->instructions_count; ++i)
    if (dump_emit_instruction(out_fp, module, func, &block->instructions[i]) < 0)
      return -1;

  return fprintf(out_fp, "    )\n") < 0 ? -1 : 0;
}

int SSA_to_L_tri_call_func(const SSAModule *module, SSAFuncName func, FILE *out_fp)
{
  const SSAFunc *fn = dump_get_func(module, func);

  if (!fn || !out_fp || !fn->name)
    return -1;

  if (fprintf(out_fp, "  (func %s", fn->name) < 0 || dump_print_func_args(out_fp, fn) < 0 ||
      fprintf(out_fp, "\n") < 0)
    return -1;

  if (fn->entry_block != SSA_INVALID_BB)
  {
    if (dump_emit_block(out_fp, module, fn, fn->entry_block) < 0)
      return -1;
  }

  for (SSABasicBlockName bb = 0; bb < fn->basic_blocks_count; ++bb)
  {
    if (bb == fn->entry_block)
      continue;
    if (dump_emit_block(out_fp, module, fn, bb) < 0)
      return -1;
  }

  return fprintf(out_fp, "  )\n") < 0 ? -1 : 0;
}

int SSA_to_L_tri_call_module(const SSAModule *module, FILE *out_fp)
{
  if (!module || !out_fp)
    return -1;

  if (fprintf(out_fp, "(\n") < 0)
    return -1;

  for (SSAFuncName fn = 0; fn < module->functions_count; ++fn)
    if (SSA_to_L_tri_call_func(module, fn, out_fp) < 0)
      return -1;

  return fprintf(out_fp, ")\n") < 0 ? -1 : 0;
}
