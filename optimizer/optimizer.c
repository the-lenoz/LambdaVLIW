#include "optimizer.h"
#include "../SSA/SSA.h"
#include <stdlib.h>

#include "../parser/AST.h"

typedef struct
{
  SSAValueType type;
  SSAConst constant;
} LatticeVal;
typedef enum
{
  LAT_UNDEF = 0,
  LAT_CONST,
  LAT_OVERDEF
} LatticeValKind;
typedef struct
{
  LatticeValKind kind;
  LatticeVal val;
} LatticeValue;

typedef struct ssa_wl
{
  SSAInstrName use;
  struct ssa_wl *next;
} SSAWorkList;

typedef struct cfg_wl
{
  SSABasicBlockName src;
  SSABasicBlockName dst;
  struct cfg_wl *next;
} CFGWorkList;

typedef struct
{
  SSAModule *module;
  SSAFuncName fn;
  SSAFunc *f;
  LatticeValue *lattice;
  int *executable;
  int *bb_executions_count;
  SSAWorkList *ssa_work_list;
  CFGWorkList *cfg_work_list;
} SCCPContext;

void visit_instr(SCCPContext *context, SSABasicBlockName BB, SSAInstrName instr);

static void CFGWorkList_push(CFGWorkList **wl, CFGWorkList val)
{
  if (!wl) return;
  val.next = *wl;
  *wl = malloc(sizeof(CFGWorkList));
  **wl = val;
}

static CFGWorkList CFGWorkList_pop(CFGWorkList **wl)
{
  if (!wl || !*wl) return (CFGWorkList){};
  const CFGWorkList result = **wl;
  CFGWorkList *to_free = *wl;
  *wl = (*wl)->next;
  free(to_free);
  return result;
}

static void SSAWorkList_push(SSAWorkList **wl, SSAWorkList val)
{
  if (!wl) return;
  val.next = *wl;
  *wl = malloc(sizeof(SSAWorkList));
  **wl = val;
}

static SSAWorkList SSAWorkList_pop(SSAWorkList **wl)
{
  if (!wl || !*wl) return (SSAWorkList){};
  const SSAWorkList result = **wl;
  SSAWorkList *to_free = *wl;
  *wl = (*wl)->next;
  free(to_free);
  return result;
}


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
    if (function->values[val].expr.call.callee_name != func)
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

int const_eq(const LatticeVal a, const LatticeVal b)
{
  if (a.type != b.type) return 0;
  switch (a.type)
  {
    case SSA_i1:
    case SSA_i8:
    case SSA_i32:
    case SSA_i64:
      return a.constant.int_value == b.constant.int_value;
    case SSA_fp32:
    case SSA_fp64:
      return a.constant.float_value == b.constant.float_value;
    default:
      return 0;
  }
}

LatticeValue meet(LatticeValue A, LatticeValue B)
{
  if (A.kind == LAT_OVERDEF || B.kind == LAT_OVERDEF) return (LatticeValue){LAT_OVERDEF};
  if (A.kind == LAT_UNDEF) return B;
  if (B.kind == LAT_UNDEF) return A;
  // Here both are const
  if (const_eq(A.val, B.val))
    return A;
  return (LatticeValue){LAT_OVERDEF};
}

int is_true(const LatticeVal val)
{
  switch (val.type) {
    case SSA_i1:
    case SSA_i8:
    case SSA_i32:
    case SSA_i64:
      return !!val.constant.int_value;
    case SSA_fp32:
    case SSA_fp64:
      return !!val.constant.float_value;
    default:
      return 0;
  }
}

LatticeValue fold_add(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value + context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value + context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_sub(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value - context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value - context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_mul(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value * context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value * context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_div(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value / context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value / context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_lt(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value < context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value < context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_gt(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value > context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value > context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_eq(SCCPContext *context, SSAValName a, SSAValName b) {
  if ((context->lattice[a].val.type == SSA_fp32 || context->lattice[a].val.type == SSA_fp64) &&
      (context->lattice[b].val.type == SSA_fp32 || context->lattice[b].val.type == SSA_fp64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .float_value = context->lattice[a].val.constant.float_value == context->lattice[b].val.constant.float_value
    }}};
  if ((context->lattice[a].val.type == SSA_i1 || context->lattice[a].val.type == SSA_i8 ||
        context->lattice[a].val.type == SSA_i32 || context->lattice[a].val.type == SSA_i64) &&
      (context->lattice[b].val.type == SSA_i1 || context->lattice[b].val.type == SSA_i8 ||
        context->lattice[b].val.type == SSA_i32 || context->lattice[b].val.type == SSA_i64))
    return (LatticeValue){LAT_CONST, {SSA_fp64, {
      .int_value = context->lattice[a].val.constant.int_value == context->lattice[b].val.constant.int_value
    }}};
  return (LatticeValue){LAT_OVERDEF};
}

LatticeValue fold_call(SCCPContext *context, const _FuncCall *call)
{
  STR_MATCH(context->module->functions[call->callee_name].name)
  STR_CASE("+")
    return fold_add(context, call->args->name, call->args->next->name);
  STR_CASE("-")
    return fold_sub(context, call->args->name, call->args->next->name);
  STR_CASE("*")
    return fold_mul(context, call->args->name, call->args->next->name);
  STR_CASE("/")
    return fold_div(context, call->args->name, call->args->next->name);
  STR_CASE("<")
    return fold_lt(context, call->args->name, call->args->next->name);
  STR_CASE(">")
    return fold_gt(context, call->args->name, call->args->next->name);
  STR_CASE("=")
    return fold_eq(context, call->args->name, call->args->next->name);
  STR_DEFAULT
    return (LatticeValue){LAT_OVERDEF};
  STR_MATCH_END
}

void try_fold(SCCPContext *context, SSAValName val, LatticeValue *dst)
{
  switch (context->f->values[val].kind)
  {
    case SSA_VALUE_CALL:
      for (ArgList *args = context->f->values[val].expr.call.args; args; args = args->next)
      {
        if (context->lattice[args->name].kind == LAT_UNDEF) {
          dst->kind = LAT_UNDEF;
          return;
        }
        if (context->lattice[args->name].kind == LAT_OVERDEF) {
          dst->kind = LAT_OVERDEF;
          return;
        }
      }
      *dst = fold_call(context, &context->f->values[val].expr.call);
      break;
    case SSA_VALUE_BOOL_CAST:
      if (context->lattice[context->f->values[val].expr.bool_val].kind == LAT_UNDEF)
        dst->kind = LAT_UNDEF;
      if (context->lattice[context->f->values[val].expr.bool_val].kind == LAT_OVERDEF)
        dst->kind = LAT_OVERDEF;
      dst->kind = LAT_CONST;
      dst->val.constant.int_value = is_true(context->lattice[context->f->values[val].expr.bool_val].val);
      break;
    default:
      break;
  }
}

void visit_val(SCCPContext *context, SSAValName val)
{
  LatticeValue calculated_val = context->lattice[val];;
  int (*executable)[context->f->basic_blocks_count] = (int (*)[])context->executable;
  if ((context->f->values[val].kind == SSA_VALUE_CALL || context->f->values[val].kind == SSA_VALUE_BOOL_CAST) &&
    context->f->values[val].is_constexpr && context->lattice[val].kind == LAT_UNDEF)
  {
    try_fold(context, val, &calculated_val);
    if (calculated_val.kind == LAT_UNDEF)
      return;
  } else if (context->f->values[val].kind == SSA_VALUE_PHI)
  {
    for (PhiList *l = context->f->values[val].expr.phi.options; l; l = l->next)
      if (executable[l->pair.previous_block_name][get_val_declaration_BB(context->module, context->fn, val)])
        calculated_val = meet(calculated_val, context->lattice[l->pair.value_name]);
  }
  if (context->lattice[val].kind != calculated_val.kind ||
      !const_eq(calculated_val.val, context->lattice[val].val))
  {
    context->lattice[val] = calculated_val;

    SSAInstrList *all_uses = find_all_val_usages(context->module, context->fn, val);
    for (SSAInstrList *uses = all_uses;
      uses;
      uses = uses->next)
    {
      SSAWorkList_push(&context->ssa_work_list, (SSAWorkList){uses->instr});
    }
    SSAInstrList_destroy(all_uses);
  }
}

void visit_term(SCCPContext *context, SSABasicBlockName BB, SSAInstrName instr)
{
  if (instr->term.type == SSA_TERM_GOTO)
  {
    CFGWorkList_push(&context->cfg_work_list, (CFGWorkList){BB, instr->term.true_dst});
    return;
  }
  if (instr->term.type == SSA_TERM_RETURN)
    return;

  const SSAValName cond = instr->term.cond;
  LatticeValue val = context->lattice[cond];
  if (val.kind == LAT_CONST)
  {
    CFGWorkList_push(&context->cfg_work_list,
      (CFGWorkList){BB, is_true(val.val) ? instr->term.true_dst : instr->term.false_dst});
  } else if (val.kind == LAT_OVERDEF)
  {
    CFGWorkList_push(&context->cfg_work_list, (CFGWorkList){BB, instr->term.true_dst});
    CFGWorkList_push(&context->cfg_work_list, (CFGWorkList){BB, instr->term.false_dst});
  }
}

void visit_instr(SCCPContext *context, SSABasicBlockName BB, SSAInstrName instr)
{
  switch (instr->kind)
  {
    case SSA_INSTR_VAL:
      visit_val(context, instr->val);
      break;
    case SSA_INSTR_TERM:
      visit_term(context, BB, instr);
      break;
    default:
      break;
  }
}

void process_CFG_edge(SCCPContext *context, SSABasicBlockName src, SSABasicBlockName dst)
{
  int (*executable)[context->f->basic_blocks_count] = (int (*)[])context->executable;

  if (src != SSA_INVALID_BB)
    executable[src][dst] = 1;
  context->bb_executions_count[dst]++;

  for (SSAInstrName instr = context->f->basic_blocks[dst].first_instr; instr; instr = instr->next)
  {
    if (context->bb_executions_count[dst] > 1 && (instr->kind != SSA_INSTR_VAL || context->f->values[instr->val].kind != SSA_VALUE_PHI))
      break;
    visit_instr(context, dst, instr);
  }

}

void process_SSA_edge(SCCPContext *context, SSAInstrName use)
{
  visit_instr(context, use->parent, use);
}

void init_SCCP_context(SCCPContext *context)
{
  for (SSAValName val = 0; val < context->f->values_count; ++val)
  {
    if (val < context->f->args_count) {
      context->lattice[val] = (LatticeValue){LAT_OVERDEF};
      continue;
    }
    if (context->f->values[val].kind == SSA_VALUE_CONST)
    {
      context->lattice[val].kind = LAT_CONST;
      context->lattice[val].val.type = context->f->values[val].type;
      context->lattice[val].val.constant = context->f->values[val].expr.cnst;
    }
  }
}

void rewrite_after_SCCP(SCCPContext *context)
{
  int (*executable)[context->f->basic_blocks_count] = (int (*)[])context->executable;

  for (SSAValName v = 0; v < context->f->values_count; ++v)
  {
    if (!is_valid_value(context->module, context->fn, v))
      continue;
    if (context->f->values[v].kind == SSA_VALUE_CONST ||
        context->lattice[v].kind != LAT_CONST)
      continue;

    SSAInstrName t = get_BB_terminator(context->module, context->fn, context->f->entry_block);
    SSABlockTerminator term = t->term;
    clear_BB_terminator(context->module, context->fn, context->f->entry_block);

    SSAValName new_v = emit_const_assign(context->module, context->fn, context->f->entry_block,
      context->lattice[v].val.type, context->lattice[v].val.constant);
    insert_instr_after(context->module, context->fn, context->f->entry_block,
      context->f->basic_blocks[context->f->entry_block].last_instr, (SSAInstr){SSA_INSTR_TERM, .term = term});
    rename_all_val_uses(context->module, context->fn, v, new_v);
  }

  for (SSABasicBlockName BB = 0; BB < context->f->basic_blocks_count; ++BB)
  {
    if (!is_valid_bb(context->module, context->fn, BB))
      continue;
    for (SSAInstrName instr = context->f->basic_blocks[BB].first_instr; instr; instr = instr->next)
    {
      if (instr->kind != SSA_INSTR_VAL || context->f->values[instr->val].kind != SSA_VALUE_PHI)
        break;
      SSABasicBlockList *not_exec_preds = NULL;
      for (PhiList *l = context->f->values[instr->val].expr.phi.options; l; l = l->next)
        if (!executable[l->pair.previous_block_name][BB])
          SSABasicBlockList_append(&not_exec_preds, l->pair.previous_block_name);
      while (not_exec_preds)
      {
        SSABasicBlockName pred = SSABasicBlockList_pop(&not_exec_preds);
        remove_phi_option_by_pred(context->module, context->fn, instr->val, pred);
      }
    }

    SSAInstrName term = get_BB_terminator(context->module, context->fn, BB);
    if (term->term.type != SSA_TERM_COND_GOTO ||
        context->f->values[term->term.cond].kind != SSA_VALUE_CONST)
      continue;
    LatticeVal val = {context->f->values[term->term.cond].type, context->f->values[term->term.cond].expr.cnst};
    SSABasicBlockName dst = is_true(val) ? term->term.true_dst : term->term.false_dst;
    clear_BB_terminator(context->module, context->fn, BB);
    emit_goto(context->module, context->fn, BB, dst);
  }
}

int SCCP_func(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || function->entry_block == SSA_INVALID_BB)
    return 0;

  SCCPContext context;
  context.f = function;
  context.module = module;
  context.fn = func;
  context.lattice = calloc(function->values_count, sizeof(LatticeValue));
  context.executable = calloc(
    function->basic_blocks_count * function->basic_blocks_count, sizeof(int));
  context.bb_executions_count = calloc(function->basic_blocks_count, sizeof(int));
  context.ssa_work_list = NULL;
  context.cfg_work_list = NULL;

  init_SCCP_context(&context);

  CFGWorkList_push(&context.cfg_work_list, (CFGWorkList){SSA_INVALID_BB, function->entry_block, NULL});

  while (context.ssa_work_list || context.cfg_work_list)
  {
    if (context.ssa_work_list)
    {
      SSAWorkList val = SSAWorkList_pop(&context.ssa_work_list);
      process_SSA_edge(&context, val.use);
    }
    else
    {
      CFGWorkList val = CFGWorkList_pop(&context.cfg_work_list);
      process_CFG_edge(&context, val.src, val.dst);
    }
  }

  rewrite_after_SCCP(&context);

  free(context.bb_executions_count);
  free(context.executable);
  free(context.lattice);
  return 1;
}

int UCE_func(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!require_RPO(module, func) || !function)
    return 0;

  for (SSABasicBlockName i = 0; i < function->basic_blocks_count; ++i)
  {
    if (!is_valid_bb(module, func, i))
      continue;
    if (function->CFG_info.RPO_index[i] == -1)
    {
      destroy_BB(module, func, i);
    }
  }
  return 1;
}

int DCE_func(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  // TODO more: now depends on var order
  int *val_is_alive = calloc(function->values_count, sizeof(int));
  for (SSAValName i = 0; i < function->values_count; ++i)
  {
    if (!is_valid_value(module, func, i))
      continue;
    if (!function->values[i].is_pure)
    {
      val_is_alive[i] = 1;
      continue;
    }
    SSAInstrList *uses = find_all_val_usages(module, func, i);
    if (uses) {
      SSAInstrList_destroy(uses);
      continue;
    }

    SSAInstrName instr = get_val_declaration_instr(module, func, i);
    SSABasicBlockName bb = get_val_declaration_BB(module, func, i);
    remove_instr(module, func, bb, instr);
  }

  free(val_is_alive);
  return 1;
}

int simplify_CFG_func(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_predecessors_list(module, func))
    return 0;

  for (SSABasicBlockName i = 0; i < function->basic_blocks_count; ++i)
  {
    require_predecessors_list(module, func); // TODO optimize
    if (!is_valid_bb(module, func, i) || i == function->entry_block)
      continue;
    SSABasicBlockList *preds = function->CFG_info.preds[i];
    if (!preds || preds->next)
      continue; // Require exactly one predecessor

    merge_into_only_predecessor(module, func, i);
  }

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

int SCCP_module(SSAModule *module)
{
  if (!module)
    return 0;
  int optimized = 0;
  for (SSAFuncName i = 0; i < module->functions_count; ++i)
  {
    optimized += SCCP_func(module, i);
  }
  return optimized;
}

int UCE_module(SSAModule *module)
{
  if (!module)
    return 0;
  int optimized = 0;
  for (SSAFuncName i = 0; i < module->functions_count; ++i)
  {
    optimized += UCE_func(module, i);
  }
  return optimized;
}

int DCE_module(SSAModule *module)
{
  if (!module)
    return 0;
  int optimized = 0;
  for (SSAFuncName i = 0; i < module->functions_count; ++i)
  {
    optimized += DCE_func(module, i);
  }
  return optimized;
}

int simplify_CFG_module(SSAModule *module)
{
  if (!module)
    return 0;
  int optimized = 0;
  for (SSAFuncName i = 0; i < module->functions_count; ++i)
  {
    optimized += simplify_CFG_func(module, i);
  }
  return optimized;
}

int optimize_module(SSAModule *module)
{
  int result = 0;
  result += self_TCO_module(module);
  result += SCCP_module(module);
  result += UCE_module(module);
  result += DCE_module(module);
  result += simplify_CFG_module(module);
  return result;
}
