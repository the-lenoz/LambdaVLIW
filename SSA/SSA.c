#include "SSA.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define BASIC_ARRAY_SIZE 64

static int ensure_array_capacity(void **arr, unsigned int *cap, size_t elem_size, unsigned int min_elems)
{
  unsigned int new_cap;
  void *new_arr;

  if (!arr || !cap || elem_size == 0)
    return -1;

  if (*cap >= min_elems)
    return 0;

  new_cap = (*cap == 0) ? BASIC_ARRAY_SIZE : *cap;
  while (new_cap < min_elems)
  {
    if (new_cap > UINT_MAX / 2)
      return -1;
    new_cap *= 2;
  }

  if ((size_t)new_cap > (size_t)-1 / elem_size)
    return -1;

  new_arr = realloc(*arr, (size_t)new_cap * elem_size);
  if (!new_arr)
    return -1;

  if (new_cap > *cap)
    memset((char *)new_arr + (size_t)(*cap) * elem_size, 0, (size_t)(new_cap - *cap) * elem_size);

  *arr = new_arr;
  *cap = new_cap;
  return 0;
}

SSAFunc *get_func(SSAModule *module, SSAFuncName func)
{
  if (!module || func >= module->functions_count)
    return NULL;

  return &module->functions[func];
}

int is_valid_bb(SSAModule *module, SSAFuncName func, SSABasicBlockName bb)
{
  SSAFunc *f = get_func(module, func);
  return f && bb < f->basic_blocks_count;
}

int is_valid_value(SSAModule *module, SSAFuncName func, SSAValName value)
{
  SSAFunc *f = get_func(module, func);
  return f && value < f->values_count && f->values[value].kind != SSA_VALUE_INVALID;
}

int bb_has_terminator(SSAModule *module, SSAFuncName func, SSABasicBlockName bb)
{
  SSAFunc *function = get_func(module, func);
  return is_valid_bb(module, func, bb) &&
         function->basic_blocks[bb].first_instr != SSA_INVALID_INSTR &&
         function->basic_blocks[bb].last_instr != SSA_INVALID_INSTR &&
         function->basic_blocks[bb].last_instr->kind == SSA_INSTR_TERM;
}

SSAInstrName get_BB_terminator(SSAModule *module, SSAFuncName fn, SSABasicBlockName BB)
{
  SSAFunc *function;
  if (!module || !(function = get_func(module, fn)))
    return SSA_INVALID_INSTR;
  if (!bb_has_terminator(module, fn, BB))
    return SSA_INVALID_INSTR;
  return function->basic_blocks[BB].last_instr;
}

int clear_BB_terminator(SSAModule *module, SSAFuncName fn, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, fn);
  if (!function || !is_valid_bb(module, fn, BB))
    return 0;

  if (!bb_has_terminator(module, fn, BB))
    return 0;

  SSAInstrName term = get_BB_terminator(module, fn, BB);

  return remove_instr(module, fn, BB, term);
}

static void destroy_phi_list(PhiList *list)
{
  if (!list)
    return;
  PhiList *next = list->next;
  free(list);
  return destroy_phi_list(next);
}

static void destroy_arg_list(ArgList *list)
{
  if (!list)
    return;
  ArgList *next = list->next;
  free(list);
  return destroy_arg_list(next);
}

static void destroy_BB(SSABasicBlock *bb)
{
  if (!bb)
    return;
  for (SSAInstrName instr = bb->first_instr; instr != SSA_INVALID_INSTR;)
  {
    SSAInstrName next = instr->next;
    if (instr->kind == SSA_INSTR_VOID_CALL)
      destroy_arg_list(instr->call.args);
    free(instr);
    instr = next;
  }
}

static void destroy_BBTree(BBTree *tree)
{
  if (!tree)
    return;
  free(tree->child_arr);
  free(tree->sibling_arr);
  free(tree->parent_arr);
}

static void destroy_CFGInfo(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return;
  CFGInfo *info = &function->CFG_info;
  if (info->preds)
    for (int i = 0; i < function->basic_blocks_count; ++i)
      SSABasicBlockList_destroy(info->preds[i]);
  free(info->preds);

  if (info->succs)
    for (int i = 0; i < function->basic_blocks_count; ++i)
      SSABasicBlockList_destroy(info->succs[i]);
  free(info->succs);

  free(info->RPO_index);
  free(info->inverse_RPO_index);

  free(info->entry_reachable);
  free(info->exit_reachable);

  destroy_BBTree(&info->Dom_tree);
  destroy_BBTree(&info->PDom_tree);

  *info = (CFGInfo){};
}

static void destroy_func(SSAModule *module, SSAFuncName fn)
{
  SSAFunc *func = get_func(module, fn);
  if (!func)
    return;

  for (int i = 0; i < func->values_count; ++i)
  {
    if (func->values[i].kind == SSA_VALUE_CALL)
      destroy_arg_list(func->values[i].expr.call.args);
    else if (func->values[i].kind == SSA_VALUE_PHI)
      destroy_phi_list(func->values[i].expr.phi.options);
  }

  free(func->name);
  free(func->values);

  for (int i = 0; i < func->basic_blocks_count; ++i)
    destroy_BB(func->basic_blocks + i);

  free(func->basic_blocks);
  free(func->arg_types);
  free(func->arg_SSA_names);

  destroy_CFGInfo(module, fn);

  *func = (SSAFunc){};
}

static SSAValName append_value(SSAFunc *function, SSAValue value)
{
  if (!function)
    return SSA_INVALID_VAL;

  if (ensure_array_capacity((void **)&function->values, &function->values_cap,
                            sizeof(SSAValue), function->values_count + 1) < 0)
    return SSA_INVALID_VAL;

  function->values[function->values_count] = value;
  function->values_count += 1;
  return function->values_count - 1;
}

static int pop_value(SSAFunc *function, SSAValName value)
{
  if (!function || value != function->values_count - 1)
    return 0;
  function->values_count--;
  return 1;
}

SSAModule *new_module()
{
  SSAModule *module = (SSAModule *)calloc(1, sizeof(SSAModule));

  if (!module)
    return NULL;

  module->functions_cap = BASIC_ARRAY_SIZE;
  module->functions = (SSAFunc *)calloc(module->functions_cap, sizeof(SSAFunc));
  if (!module->functions)
  {
    free(module);
    return NULL;
  }

  return module;
}

void destroy_module(SSAModule *module)
{
  unsigned int i;

  if (!module)
    return;

  for (i = 0; i < module->functions_count; ++i)
    destroy_func(module, i);

  free(module->functions);
  *module = (SSAModule){};
  free(module);
}

SSAFuncName new_func(SSAModule *module, const char *name, SSAValueType return_type,
                     unsigned int args_count,
                     const SSAValueType *arg_types, int is_global)
{
  SSAFunc *func;

  (void)is_global;

  if (!module || !name || (args_count && !arg_types))
    return SSA_INVALID_FUNC;

  if (ensure_array_capacity((void **)&module->functions, &module->functions_cap,
                            sizeof(SSAFunc), module->functions_count + 1) < 0)
    return SSA_INVALID_FUNC;

  func = &module->functions[module->functions_count];
  *func = (SSAFunc){};

  func->name = strdup(name);
  if (!func->name)
    return SSA_INVALID_FUNC;

  func->return_type = return_type;
  if (args_count)
  {
    func->arg_types = (SSAValueType *)calloc(args_count, sizeof(SSAValueType));
    if (!func->arg_types)
    {
      free(func->name);
      *func = (SSAFunc){};
      return SSA_INVALID_FUNC;
    }
    memcpy(func->arg_types, arg_types, args_count * sizeof(SSAValueType));
  }

  func->values_cap = BASIC_ARRAY_SIZE + args_count;
  func->values = (SSAValue *)calloc(func->values_cap, sizeof(SSAValue));
  if (!func->values)
  {
    free(func->arg_types);
    free(func->name);
    *func = (SSAFunc){};
    return SSA_INVALID_FUNC;
  }

  func->values_count = args_count;
  for (unsigned int i = 0; i < args_count; ++i)
  {
    func->values[i].kind = SSA_VALUE_ARG;
    func->values[i].type = arg_types[i];
  }

  func->args_count = args_count;
  func->arg_SSA_names = (SSAValName *)calloc(func->args_count, sizeof(SSAValName));
  if (!func->arg_SSA_names)
  {
    free(func->arg_types);
    free(func->values);
    free(func->name);
    *func = (SSAFunc){};
    return SSA_INVALID_FUNC;
  }

  for (unsigned int i = 0; i < args_count; ++i)
    func->arg_SSA_names[i] = i;

  func->basic_blocks_cap = BASIC_ARRAY_SIZE;
  func->basic_blocks = (SSABasicBlock *)calloc(func->basic_blocks_cap, sizeof(SSABasicBlock));
  if (!func->basic_blocks)
  {
    free(func->arg_types);
    free(func->values);
    free(func->name);
    free(func->arg_SSA_names);
    *func = (SSAFunc){};
    return SSA_INVALID_FUNC;
  }

  func->entry_block = SSA_INVALID_BB;
  func->exit_block = SSA_INVALID_BB;
  func->parent_module = module;

  func->CFG_info = (CFGInfo){};

  module->functions_count += 1;
  return module->functions_count - 1;
}

SSABasicBlockName new_BB(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  SSABasicBlockName bb_name;

  if (!function)
    return SSA_INVALID_BB;

  destroy_CFGInfo(module, func);

  if (ensure_array_capacity((void **)&function->basic_blocks, &function->basic_blocks_cap,
                            sizeof(SSABasicBlock), function->basic_blocks_count + 1) < 0)
    return SSA_INVALID_BB;

  bb_name = function->basic_blocks_count;
  SSABasicBlock *BB = &function->basic_blocks[bb_name];

  BB->parent_name = func;
  BB->first_instr = SSA_INVALID_INSTR;
  BB->last_instr = SSA_INVALID_INSTR;
  function->basic_blocks_count++;
  return bb_name;
}

int set_entry_BB(SSAModule *module, SSAFuncName func, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB))
    return 0;
  destroy_CFGInfo(module, func);
  function->entry_block = BB;
  return 1;
}
int set_exit_BB(SSAModule *module, SSAFuncName func, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB))
    return 0;
  destroy_CFGInfo(module, func);
  function->exit_block = BB;
  return 1;
}

static SSAInstrName add_instr(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAInstr instr)
{
  SSAFunc *f = get_func(module, func);
  if (!is_valid_bb(module, func, BB))
    return SSA_INVALID_INSTR;
  SSABasicBlock *block = &f->basic_blocks[BB];

  SSAInstrName instr_name = malloc(sizeof(SSAInstr));
  if (!instr_name || !block)
    return SSA_INVALID_INSTR;
  instr.prev = block->last_instr;
  instr.next = SSA_INVALID_INSTR;
  *instr_name = instr;

  if (block->last_instr != SSA_INVALID_INSTR)
    block->last_instr->next = instr_name;
  else
    block->first_instr = instr_name;

  block->last_instr = instr_name;

  destroy_CFGInfo(module, func);

  return instr_name;
}

unsigned get_BB_instr_count(SSAModule *module, SSAFuncName func, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, func);
  unsigned count = 0;

  if (!function || !is_valid_bb(module, func, BB))
    return 0;

  for (SSAInstrName instr = function->basic_blocks[BB].first_instr; instr != SSA_INVALID_INSTR; instr = instr->next)
    count += 1;

  return count;
}

SSABasicBlockName get_val_declaration_BB(SSAModule *module, SSAFuncName func, SSAValName value)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_value(module, func, value))
    return SSA_INVALID_BB;
  return function->values[value].parent_name;
}

SSAInstrName get_val_declaration_instr(SSAModule *module, SSAFuncName func, SSAValName value)
{
  SSAFunc *function = get_func(module, func);
  SSABasicBlockName bb;

  if (!function || !is_valid_value(module, func, value))
    return SSA_INVALID_INSTR;

  bb = function->values[value].parent_name;
  if (!is_valid_bb(module, func, bb))
    return SSA_INVALID_INSTR;

  for (SSAInstrName instr = function->basic_blocks[bb].first_instr; instr != SSA_INVALID_INSTR; instr = instr->next)
    if (instr->kind == SSA_INSTR_VAL && instr->val == value)
      return instr;

  return SSA_INVALID_INSTR;
}

SSAValName emit_phi_assign(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                           SSAValueType type)
{
  SSAFunc *function = get_func(module, func);
  SSAValue value;

  if (!function || !is_valid_bb(module, func, BB) || bb_has_terminator(module, func, BB) || type == SSA_void)
    return SSA_INVALID_VAL;

  value = (SSAValue){};
  value.kind = SSA_VALUE_PHI;
  value.is_const = 0;
  value.expr.phi.options = NULL;
  value.parent_name = BB;
  value.type = type;

  SSAValName val_name = append_value(function, value);

  if (val_name == SSA_INVALID_VAL)
    return val_name;

  if (!add_instr(module, func, BB, (SSAInstr){SSA_INSTR_VAL, val_name, {}}))
  {
    pop_value(function, val_name);
    return SSA_INVALID_VAL;
  }

  return val_name;
}

SSAValName emit_call_assign(SSAModule *module, SSAFuncName func,
                            SSABasicBlockName BB, SSAFuncName callee, ArgList *arg_list, int is_constexpr)
{
  SSAFunc *function = get_func(module, func);
  SSAValue value;

  if (!function || !is_valid_bb(module, func, BB) || bb_has_terminator(module, func, BB))
    return SSA_INVALID_VAL;
  SSAFunc *calee_func = get_func(module, callee);
  if (!calee_func || calee_func->return_type == SSA_void)
    return SSA_INVALID_VAL;

  unsigned int i = 0;
  for (ArgList *arg = arg_list; arg; arg = arg->next, i++)
  {
    if (!is_valid_value(module, func, arg->name) ||
        i >= calee_func->args_count ||
        function->values[arg->name].type != calee_func->arg_types[i])
      return SSA_INVALID_VAL;
  }
  if (i != calee_func->args_count)
    return SSA_INVALID_VAL;

  value = (SSAValue){};
  value.kind = SSA_VALUE_CALL;
  value.is_const = !!is_constexpr;
  value.expr.call.calee_name = callee;
  value.expr.call.args = arg_list;
  value.parent_name = BB;
  value.type = calee_func->return_type;

  SSAValName val_name = append_value(function, value);

  if (val_name == SSA_INVALID_VAL)
    return val_name;

  if (!add_instr(module, func, BB, (SSAInstr){SSA_INSTR_VAL, val_name, {}}))
  {
    pop_value(function, val_name);
    return SSA_INVALID_VAL;
  }

  return val_name;
}

SSAValName get_arg_val_name(SSAModule *module, SSAFuncName func, unsigned int arg_index)
{
  if (!module || func >= module->functions_count || arg_index >= module->functions[func].args_count)
    return SSA_INVALID_VAL;

  return module->functions[func].arg_SSA_names[arg_index];
}

SSAValName emit_const_assign(SSAModule *module, SSAFuncName func,
                             SSABasicBlockName BB, SSAValueType type, SSAConst val)
{
  SSAFunc *function = get_func(module, func);
  SSAValue ssa_value;

  if (!function || !is_valid_bb(module, func, BB) || type == SSA_void || bb_has_terminator(module, func, BB))
    return SSA_INVALID_VAL;

  ssa_value = (SSAValue){};
  ssa_value.kind = SSA_VALUE_CONST;
  ssa_value.is_const = 1;
  ssa_value.expr.cnst = val;
  ssa_value.parent_name = BB;
  ssa_value.type = type;

  SSAValName val_name = append_value(function, ssa_value);

  if (val_name == SSA_INVALID_VAL)
    return val_name;

  if (!add_instr(module, func, BB, (SSAInstr){SSA_INSTR_VAL, val_name, {}, {}}))
  {
    pop_value(function, val_name);
    return SSA_INVALID_VAL;
  }

  return val_name;
}
SSAValName emit_bool_cast_assign(SSAModule *module, SSAFuncName func,
                                 SSABasicBlockName BB, SSAValName val, SSAValueType type)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB) || type == SSA_void || bb_has_terminator(module, func, BB))
    return SSA_INVALID_VAL;

  SSAValue value = (SSAValue){
      SSA_VALUE_BOOL_CAST,
      SSA_i1,
      1,
      {.bool_val = val},
      BB};

  SSAValName val_name = append_value(function, value);

  if (val_name == SSA_INVALID_VAL)
    return val_name;

  if (!add_instr(module, func, BB, (SSAInstr){SSA_INSTR_VAL, val_name, {}, {}}))
  {
    pop_value(function, val_name);
    return SSA_INVALID_VAL;
  }

  return val_name;
}

int emit_void_call(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                   SSAFuncName callee, ArgList *arg_list)
{
  SSAFunc *function = get_func(module, func);
  SSAValue value;

  if (!function || !is_valid_bb(module, func, BB))
    return -1;
  SSAFunc *calee_func = get_func(module, callee);
  if (!calee_func || calee_func->return_type != SSA_void)
    return -1;

  unsigned int i = 0;
  for (ArgList *arg = arg_list; arg; arg = arg->next, i++)
  {
    if (!is_valid_value(module, func, arg->name) ||
        i >= calee_func->args_count ||
        function->values[arg->name].type != calee_func->arg_types[i])
      return -1;
  }
  if (i != calee_func->args_count)
    return -1;

  if (!add_instr(module, func, BB,
                 (SSAInstr){
                     SSA_INSTR_VOID_CALL,
                     SSA_INVALID_VAL,
                     (_FuncCall){callee, arg_list},
                     {}}))

    return -1;

  return 0;
}

int emit_cond_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName cond_name,
                   SSABasicBlockName true_dst, SSABasicBlockName false_dst)
{
  SSAFunc *function = get_func(module, func);

  if (!function)
    return -1;
  if (!is_valid_bb(module, func, BB) || !is_valid_bb(module, func, true_dst) ||
      !is_valid_bb(module, func, false_dst))
    return -1;
  if (!is_valid_value(module, func, cond_name))
    return -1;
  if (bb_has_terminator(module, func, BB))
    return -1;

  if (function->values[cond_name].type != SSA_i1)
    return -1;

  if (!add_instr(module, func, BB,
                 (SSAInstr){
                     SSA_INSTR_TERM,
                     SSA_INVALID_VAL,
                     {},
                     (SSABlockTerminator){
                         SSA_TERM_COND_GOTO,
                         cond_name,
                         SSA_INVALID_VAL,
                         true_dst,
                         false_dst}}))
    return -1;

  return 0;
}

int emit_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSABasicBlockName dst)
{
  SSAFunc *function = get_func(module, func);

  if (!function)
    return -1;
  if (!is_valid_bb(module, func, BB) || !is_valid_bb(module, func, dst))
    return -1;
  if (bb_has_terminator(module, func, BB))
    return -1;

  if (!add_instr(module, func, BB,
                 (SSAInstr){
                     SSA_INSTR_TERM,
                     SSA_INVALID_VAL,
                     {},
                     (SSABlockTerminator){
                         SSA_TERM_GOTO,
                         SSA_INVALID_VAL,
                         SSA_INVALID_VAL,
                         dst,
                         SSA_INVALID_BB}}))
    return -1;

  return 0;
}

int emit_return(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName ret_name)
{
  SSAFunc *function = get_func(module, func);

  if (!function)
    return -1;
  if (!is_valid_bb(module, func, BB))
    return -1;
  if (function->exit_block == SSA_INVALID_BB || BB != function->exit_block)
    return -1;
  if (!is_valid_value(module, func, ret_name) && function->return_type != SSA_void)
    return -1;
  if (bb_has_terminator(module, func, BB))
    return -1;
  if (function->return_type == SSA_void)
  {
    if (ret_name != SSA_VALUE_VOID)
      return -1;
  }
  else if (function->return_type != function->values[ret_name].type)
    return -1;

  if (!add_instr(module, func, BB,
                 (SSAInstr){
                     SSA_INSTR_TERM,
                     SSA_INVALID_VAL,
                     {},
                     (SSABlockTerminator){
                         SSA_TERM_RETURN,
                         SSA_INVALID_VAL,
                         ret_name,
                         SSA_INVALID_BB,
                         SSA_INVALID_BB}}))
    return -1;

  return 0;
}

int add_phi_option(SSAModule *module, SSAFuncName fn, SSAValName val_name, PhiPair pair)
{
  if (!module)
    return -1;

  SSAFunc *function = get_func(module, fn);
  if (!function || !is_valid_value(module, fn, val_name) ||
      function->values[val_name].kind != SSA_VALUE_PHI)
    return -1;

  if (!is_valid_value(module, fn, pair.value_name) || !is_valid_bb(module, fn, pair.previous_block_name))
    return -1;

  if (function->values[pair.value_name].type != function->values[val_name].type)
    return -1;

  PhiList **list = &function->values[val_name].expr.phi.options;

  PhiList *new_head = malloc(sizeof(PhiList));
  if (!new_head)
    return -1;
  *new_head = (PhiList){pair, *list};
  *list = new_head;

  return 0;
}

int remove_phi_option_by_pred(SSAModule *module, SSAFuncName fn, SSAValName val_name, SSABasicBlockName pred)
{
  SSAFunc *function = get_func(module, fn);
  if (!function || !is_valid_value(module, fn, val_name) || !is_valid_bb(module, fn, pred))
    return 0;

  if (function->values[val_name].kind != SSA_VALUE_PHI)
    return 0;

  for (PhiList **option = &function->values[val_name].expr.phi.options;
       *option;
       option = &(*option)->next)
    if ((*option)->pair.previous_block_name == pred)
    {
      PhiList *tmp = *option;
      *option = (*option)->next;
      free(tmp);
      return 1;
    }

  return 0;
}

int ArgList_append(ArgList **list, SSAValName arg_name)
{
  if (!list)
    return -1;

  if (!*list)
  {
    ArgList *l = malloc(sizeof(ArgList));
    if (!l)
      return -1;
    *list = l;
    **list = (ArgList){arg_name, NULL};
    return 0;
  }

  return ArgList_append(&(*list)->next, arg_name);
}

int SSAInstrList_append(SSAInstrList **list, SSAInstrName instr)
{
  if (!list)
    return 0;

  SSAInstrList *l = malloc(sizeof(SSAInstrList));
  if (!l)
    return 0;
  l->next = *list;
  l->instr = instr;
  *list = l;
  return 1;
}

void SSAInstrList_destroy(SSAInstrList *list)
{
  if (!list)
    return;
  SSAInstrList *next = list->next;
  free(list);
  SSAInstrList_destroy(next);
}

int SSABasicBlockList_append(SSABasicBlockList **list, SSABasicBlockName bb)
{
  if (!list)
    return 0;

  SSABasicBlockList *l = malloc(sizeof(SSABasicBlockList));
  if (!l)
    return 0;
  l->next = *list;
  l->BB = bb;
  *list = l;
  return 1;
}

void SSABasicBlockList_destroy(SSABasicBlockList *list)
{
  if (!list)
    return;
  SSABasicBlockList *next = list->next;
  free(list);
  SSABasicBlockList_destroy(next);
}

int insert_instr_before(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                        SSAInstrName place, SSAInstr instr)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB))
    return 0;

  SSABasicBlock *bb = &function->basic_blocks[BB];

  if (!bb->first_instr || place == SSA_INVALID_INSTR)
    return 0;

  destroy_CFGInfo(module, func);

  if (bb->first_instr == place)
  {
    if (instr.kind == SSA_INSTR_VAL)
    {
      if (!is_valid_value(module, func, instr.val))
        return 0;
      function->values[instr.val].parent_name = BB;
    }
    instr.next = bb->first_instr;
    instr.prev = SSA_INVALID_INSTR;
    bb->first_instr = malloc(sizeof(SSAInstr));
    if (!bb->first_instr)
    {
      bb->first_instr = instr.next;
      return 0;
    }
    *bb->first_instr = instr;
    bb->first_instr->next->prev = bb->first_instr;

    return 1;
  }
  for (SSAInstrName i = bb->first_instr; i; i = i->next)
    if (i->next == place)
    {
      if (instr.kind == SSA_INSTR_VAL)
      {
        if (!is_valid_value(module, func, instr.val))
          return 0;
        function->values[instr.val].parent_name = BB;
      }
      SSAInstrName new = malloc(sizeof(SSAInstr));
      if (!new)
        return 0;
      instr.next = i->next;
      instr.prev = i;
      *new = instr;
      i->next->prev = new;
      i->next = new;
      return 1;
    }
  return 0;
}
int insert_instr_after(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                       SSAInstrName place, SSAInstr instr)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB))
    return 0;

  SSABasicBlock *bb = &function->basic_blocks[BB];

  if (!bb->first_instr || place == SSA_INVALID_INSTR)
    return 0;

  destroy_CFGInfo(module, func);

  if (bb->last_instr == place)
  {
    if (instr.kind == SSA_INSTR_VAL)
    {
      if (!is_valid_value(module, func, instr.val))
        return 0;
      function->values[instr.val].parent_name = BB;
    }
    instr.prev = place;
    instr.next = SSA_INVALID_INSTR;
    SSAInstrName new = malloc(sizeof(SSAInstr));
    if (!new)
      return 0;
    *new = instr;
    bb->last_instr->next = new;
    bb->last_instr = new;
    return 1;
  }
  return insert_instr_before(module, func, BB, place->next, instr);
}

int replace_instr(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                  SSAInstrName place, SSAInstr instr)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB))
    return 0;

  destroy_CFGInfo(module, func);

  SSABasicBlock *bb = &function->basic_blocks[BB];
  for (SSAInstrName i = bb->first_instr; i; i = i->next)
  {
    if (i == place)
    {
      if (instr.kind == SSA_INSTR_VAL)
      {
        if (!is_valid_value(module, func, instr.val))
          return 0;
        function->values[instr.val].parent_name = BB;
      }
      if (i->kind == SSA_INSTR_VOID_CALL)
        destroy_arg_list(i->call.args);
      else if (i->kind == SSA_INSTR_VAL)
      {
        if (!is_valid_value(module, func, i->val))
          return 0;
        SSAValue *v = function->values + i->val;
        if (v->kind == SSA_VALUE_CALL)
          destroy_arg_list(v->expr.call.args);
        else if (v->kind == SSA_VALUE_PHI)
          destroy_phi_list(v->expr.phi.options);
        v->kind = SSA_VALUE_INVALID;
      }
      instr.next = i->next;
      instr.prev = i->prev;
      *i = instr;
      return 1;
    }
  }
  return 0;
}

int remove_instr(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAInstrName instr)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, BB))
    return 0;

  SSABasicBlock *bb = &function->basic_blocks[BB];

  destroy_CFGInfo(module, func);

  for (SSAInstrName i = bb->first_instr; i; i = i->next)
    if (i == instr)
    {
      if (i == bb->first_instr)
        bb->first_instr = i->next;
      else
        i->prev->next = i->next;
      if (i == bb->last_instr)
        bb->last_instr = i->prev;
      else
        i->next->prev = i->prev;

      if (i->kind == SSA_INSTR_VOID_CALL)
        destroy_arg_list(i->call.args);
      else if (i->kind == SSA_INSTR_VAL)
      {
        if (!is_valid_value(module, func, i->val))
          return 0;
        SSAValue *v = function->values + i->val;
        if (v->kind == SSA_VALUE_CALL)
          destroy_arg_list(v->expr.call.args);
        else if (v->kind == SSA_VALUE_PHI)
          destroy_phi_list(v->expr.phi.options);
        v->kind = SSA_VALUE_INVALID;
      }
      free(i);
      return 1;
    }
  return 0;
}

static int replace_in_call(SSAFunc *func, _FuncCall call, SSAValName old, SSAValName new)
{
  int changed = 0;
  for (ArgList *arg = call.args; arg; arg = arg->next)
    if (arg->name == old)
    {
      arg->name = new;
      changed++;
    }
  return changed;
}

static int replace_in_phi(SSAFunc *func, _PhiNode phi, SSAValName old, SSAValName new)
{
  int changed = 0;
  for (PhiList *option = phi.options; option; option = option->next)
    if (option->pair.value_name == old)
    {
      option->pair.value_name = new;
      changed++;
    }
  return changed;
}

static int replace_in_val_def(SSAFunc *func, SSAValName def, SSAValName old, SSAValName new)
{
  SSAValue *val = &func->values[def];
  switch (val->kind)
  {
  case SSA_VALUE_CALL:
    return replace_in_call(func, val->expr.call, old, new);
  case SSA_VALUE_PHI:
    return replace_in_phi(func, val->expr.phi, old, new);
  case SSA_VALUE_BOOL_CAST:
    if (val->expr.bool_val == old)
    {
      val->expr.bool_val = new;
      return 1;
    }
    return 0;
  default:
    return 0;
  }
}

static int replace_in_terminator(SSAFunc *func, SSABlockTerminator *term, SSAValName old, SSAValName new)
{
  switch (term->type)
  {
  case SSA_TERM_COND_GOTO:
    if (term->cond == old)
    {
      term->cond = new;
      return 1;
    }
    return 0;
  case SSA_TERM_RETURN:
    if (term->ret_val == old)
    {
      term->ret_val = new;
      return 1;
    }
    return 0;
  default:
    return 0;
  }
}

static int is_used_in_call(SSAFunc *func, _FuncCall call, SSAValName val)
{
  for (ArgList *arg = call.args; arg; arg = arg->next)
    if (arg->name == val)
      return 1;

  return 0;
}

static int is_used_in_phi(SSAFunc *func, _PhiNode phi, SSAValName val)
{
  for (PhiList *option = phi.options; option; option = option->next)
    if (option->pair.value_name == val)
      return 1;

  return 0;
}

SSAInstrList *find_all_val_usages(SSAModule *module, SSAFuncName func, SSAValName val)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_value(module, func, val))
    return NULL;

  SSAInstrList *result = NULL;

  for (SSABasicBlock *bb = function->basic_blocks;
       bb < function->basic_blocks + function->basic_blocks_count; ++bb)
    for (SSAInstrName i = bb->first_instr; i; i = i->next)
    {
      switch (i->kind)
      {
      case SSA_INSTR_VAL:
        if (!is_valid_value(module, func, i->val))
          continue;
        switch (function->values[i->val].kind)
        {
        case SSA_VALUE_BOOL_CAST:
          if (function->values[i->val].expr.bool_val == val)
            SSAInstrList_append(&result, i);
          break;
        case SSA_VALUE_CALL:
          if (is_used_in_call(function, function->values[i->val].expr.call, val))
            SSAInstrList_append(&result, i);
          break;
        case SSA_VALUE_PHI:
          if (is_used_in_phi(function, function->values[i->val].expr.phi, val))
            SSAInstrList_append(&result, i);
          break;
        default:
          break;
        }
        break;
      case SSA_INSTR_VOID_CALL:
        if (is_used_in_call(function, i->call, val))
          SSAInstrList_append(&result, i);
        break;
      case SSA_INSTR_TERM:
        if (i->term.type == SSA_TERM_COND_GOTO && i->term.cond == val)
          SSAInstrList_append(&result, i);
        if (i->term.type == SSA_TERM_RETURN && i->term.ret_val == val)
          SSAInstrList_append(&result, i);
        break;
      }
    }
  return result;
}

int rename_all_val_uses(SSAModule *module, SSAFuncName func, SSAValName old, SSAValName new)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_value(module, func, new) || !is_valid_value(module, func, old))
    return 0;

  if (function->values[old].type != function->values[new].type)
    return 0;

  int changed = 0;
  SSAInstrList *usages = find_all_val_usages(module, func, old);
  for (SSAInstrList *u = usages; u; u = u->next)
  {
    SSAInstrName i = u->instr;
    changed++;
    switch (i->kind)
    {
    case SSA_INSTR_VAL:
      replace_in_val_def(function, i->val, old, new);
      break;
    case SSA_INSTR_VOID_CALL:
      replace_in_call(function, i->call, old, new);
      break;
    case SSA_INSTR_TERM:
      replace_in_terminator(function, &i->term, old, new);
      break;
    }
  }
  SSAInstrList_destroy(usages);
  return changed;
}

int validate_func(SSAModule *module, SSAFuncName func)
{
  return 1;
}

static int BB_dfs(SSAModule *module, SSAFuncName func, SSABasicBlockName start,
                  int *visited, SSABasicBlockList **next_arr)
{
  if (!visited || !next_arr || !is_valid_bb(module, func, start))
    return 0;
  if (visited[start])
    return 1;
  visited[start] = 1;
  for (SSABasicBlockList *l = next_arr[start]; l; l = l->next)
    BB_dfs(module, func, l->BB, visited, next_arr);
  return 1;
}

static int build_entry_reachable(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_successors_list(module, func))
    return 0;
  function->CFG_info.entry_reachable = calloc(function->basic_blocks_count, sizeof(int));
  if (!function->CFG_info.entry_reachable)
    return 0;
  int result = BB_dfs(module, func, function->entry_block,
                      function->CFG_info.entry_reachable, function->CFG_info.succs);
  if (result)
    function->CFG_info.valid_entry_reachable = 1;
  return result;
}

int require_entry_reachable(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  if (function->CFG_info.valid_entry_reachable)
    return 1;
  return build_entry_reachable(module, func);
}

static int build_exit_reachable(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_predecessors_list(module, func))
    return 0;
  function->CFG_info.exit_reachable = calloc(function->basic_blocks_count, sizeof(int));
  if (!function->CFG_info.exit_reachable)
    return 0;
  int result = BB_dfs(module, func, function->exit_block,
                      function->CFG_info.exit_reachable, function->CFG_info.preds);
  if (result)
    function->CFG_info.valid_exit_reachable = 1;
  return result;
}
int require_exit_reachable(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  if (function->CFG_info.valid_exit_reachable)
    return 1;
  return build_exit_reachable(module, func);
}

static int is_infinite_loop_header(SSAModule *module, SSAFuncName func, SSABasicBlockName header)
{
  SSAFunc *function = get_func(module, func);

  if (!function || !require_exit_reachable(module, func) || !is_valid_bb(module, func, header))
    return 0;

  return function->CFG_info.exit_reachable[header];
}

static SSABasicBlockList *find_infinite_loops(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return NULL;

  SSABasicBlockList *all_loops = find_natural_loops(module, func);
  SSABasicBlockList *infinite_loops = NULL;

  for (SSABasicBlockList *loop = all_loops; loop; loop = loop->next)
    if (is_infinite_loop_header(module, func, loop->BB))
      SSABasicBlockList_append(&infinite_loops, loop->BB);
  SSABasicBlockList_destroy(all_loops);
  return infinite_loops;
}

static int postorder_rec(SSAModule *module, SSAFuncName func, int *visited,
                         SSABasicBlockName bb, SSABasicBlockList **postorder, SSABasicBlockList **next)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !is_valid_bb(module, func, bb) || visited[bb])
    return 0;

  visited[bb] = 1;

  for (SSABasicBlockList *successor = next[bb]; successor; successor = successor->next)
    postorder_rec(module, func, visited, successor->BB, postorder, next);

  SSABasicBlockList_append(postorder, bb);
  return 1;
}

static int build_RPO(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_successors_list(module, func))
    return 0;

  SSABasicBlockList *postorder = NULL;
  int *visited = calloc(function->basic_blocks_count, sizeof(int));
  if (!postorder_rec(module, func, visited, function->entry_block, &postorder, function->CFG_info.succs))
    return free(visited), 0;
  free(visited);

  int *RPO = malloc(function->basic_blocks_count * sizeof(int));
  SSABasicBlockName *inverse_RPO = malloc(function->basic_blocks_count * sizeof(SSABasicBlockName));
  for (int i = 0; i < function->basic_blocks_count; ++i)
  {
    RPO[i] = -1;
    inverse_RPO[i] = SSA_INVALID_BB;
  }
  int i = 0;
  for (SSABasicBlockList *bb = postorder; bb && i < function->basic_blocks_count; bb = bb->next, ++i)
  {
    inverse_RPO[i] = bb->BB;
    RPO[bb->BB] = i;
  }
  SSABasicBlockList_destroy(postorder);

  function->CFG_info.inverse_RPO_index = inverse_RPO;
  function->CFG_info.RPO_index = RPO;
  function->CFG_info.valid_RPO = 1;
  return 1;
}

int require_RPO(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  if (function->CFG_info.valid_RPO)
    return 1;

  return build_RPO(module, func);
}
static int build_back_RPO(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_predecessors_list(module, func))
    return 0;

  SSABasicBlockList *postorder = NULL;
  int *visited = calloc(function->basic_blocks_count, sizeof(int));
  SSABasicBlockList **preds_plus_infinite = calloc(function->basic_blocks_count, sizeof(SSABasicBlockList));
  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
    for (SSABasicBlockList *pred = function->CFG_info.preds[bb]; pred; pred = pred->next)
      SSABasicBlockList_append(preds_plus_infinite + bb, pred->BB);

  SSABasicBlockList *inf_loops = find_infinite_loops(module, func);
  for (SSABasicBlockList *loop = inf_loops; loop; loop = loop->next)
    SSABasicBlockList_append(preds_plus_infinite + function->exit_block, loop->BB);
  SSABasicBlockList_destroy(inf_loops);
  
  if (!postorder_rec(module, func, visited, function->exit_block, &postorder, preds_plus_infinite))
    return free(visited), 0;
  free(visited);

  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
    SSABasicBlockList_destroy(preds_plus_infinite[bb]);
  free(preds_plus_infinite);

  int *RPO = malloc(function->basic_blocks_count * sizeof(int));
  SSABasicBlockName *inverse_RPO = malloc(function->basic_blocks_count * sizeof(SSABasicBlockName));
  for (int i = 0; i < function->basic_blocks_count; ++i)
  {
    RPO[i] = -1;
    inverse_RPO[i] = SSA_INVALID_BB;
  }
  int i = 0;
  for (SSABasicBlockList *bb = postorder; bb && i < function->basic_blocks_count; bb = bb->next, ++i)
  {
    inverse_RPO[i] = bb->BB;
    RPO[bb->BB] = i;
  }
  SSABasicBlockList_destroy(postorder);

  function->CFG_info.inverse_back_RPO_index = inverse_RPO;
  function->CFG_info.back_RPO_index = RPO;
  function->CFG_info.valid_RPO = 1;
  return 1;
}

int require_back_RPO(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  if (function->CFG_info.valid_back_RPO)
    return 1;

  return build_back_RPO(module, func);
}

static int build_predcessors_list(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;
  SSABasicBlockList **preds = calloc(function->basic_blocks_count, sizeof(*preds));
  for (SSABasicBlockName BB = 0; BB < function->basic_blocks_count; ++BB)
  {
    SSAInstrName term = get_BB_terminator(module, func, BB);
    if (!term)
      continue;
    switch (term->term.type)
    {
    case SSA_TERM_GOTO:
      SSABasicBlockList_append(&preds[term->term.true_dst], BB);
      break;
    case SSA_TERM_COND_GOTO:
      SSABasicBlockList_append(&preds[term->term.true_dst], BB);
      if (term->term.false_dst != term->term.true_dst)
        SSABasicBlockList_append(&preds[term->term.false_dst], BB);
      break;
    default:
      break;
    }
  }
  function->CFG_info.preds = preds;
  function->CFG_info.valid_preds = 1;
  return 1;
}
int require_predecessors_list(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  if (function->CFG_info.valid_preds)
    return 1;
  return build_predcessors_list(module, func);
}

static int build_successors_list(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;
  SSABasicBlockList **succs = calloc(function->basic_blocks_count, sizeof(*succs));
  for (SSABasicBlockName BB = 0; BB < function->basic_blocks_count; ++BB)
  {
    SSAInstrName term = get_BB_terminator(module, func, BB);
    if (!term)
      continue;
    switch (term->term.type)
    {
    case SSA_TERM_GOTO:
      SSABasicBlockList_append(&succs[BB], term->term.true_dst);
      break;
    case SSA_TERM_COND_GOTO:
      SSABasicBlockList_append(&succs[BB], term->term.true_dst);
      if (term->term.false_dst != term->term.true_dst)
        SSABasicBlockList_append(&succs[BB], term->term.false_dst);
      break;
    default:
      break;
    }
  }
  function->CFG_info.succs = succs;
  function->CFG_info.valid_succs = 1;
  return 1;
}
int require_successors_list(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;

  if (function->CFG_info.valid_succs)
    return 1;
  return build_successors_list(module, func);
}

static SSABasicBlockName idom_intersect(SSAModule *module, SSAFuncName func,
                                        SSABasicBlockName *idom, int *RPO_idx_arr,
                                        SSABasicBlockName b1, SSABasicBlockName b2)
{
  if (!is_valid_bb(module, func, b1) || !is_valid_bb(module, func, b2))
    return SSA_INVALID_BB;

  while (b1 != b2)
  {
    while (RPO_idx_arr[b1] > RPO_idx_arr[b2])
      b1 = idom[b1];
    while (RPO_idx_arr[b1] < RPO_idx_arr[b2])
      b2 = idom[b2];
  }
  return b1;
}

static int build_Dom_tree(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_predecessors_list(module, func) || !require_RPO(module, func))
    return 0;

  SSABasicBlockName *idom = malloc(sizeof(SSABasicBlockName) * function->basic_blocks_count);
  for (SSABasicBlockName *i = idom; i < idom + function->basic_blocks_count; ++i)
    *i = SSA_INVALID_BB;

  idom[function->entry_block] = function->entry_block;

  SSABasicBlockList *processed_preds = NULL;
  SSABasicBlockName new_idom = SSA_INVALID_BB;
  int changed = 1;
  while (changed)
  {
    changed = 0;
    for (SSABasicBlockName *BB = function->CFG_info.inverse_RPO_index;
         BB < function->CFG_info.inverse_RPO_index + function->basic_blocks_count;
         ++BB)
    {
      if (*BB == SSA_INVALID_BB || *BB == function->entry_block)
        continue;
      for (SSABasicBlockList *pred = function->CFG_info.preds[*BB]; pred; pred = pred->next)
        if (idom[pred->BB] != SSA_INVALID_BB)
          SSABasicBlockList_append(&processed_preds, pred->BB);

      if (processed_preds)
      {
        new_idom = processed_preds->BB;

        for (SSABasicBlockList *p = processed_preds->next; p; p = p->next)
          new_idom = idom_intersect(module, func, idom, function->CFG_info.RPO_index, new_idom, p->BB);
        if (idom[*BB] != new_idom)
        {
          changed = 1;
          idom[*BB] = new_idom;
        }
      }

      SSABasicBlockList_destroy(processed_preds);
      processed_preds = NULL;
    }
  }
  SSABasicBlockName *child_arr = malloc(function->basic_blocks_count * sizeof(SSABasicBlockName));
  SSABasicBlockName *sibling_arr = malloc(function->basic_blocks_count * sizeof(SSABasicBlockName));
  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
    child_arr[bb] = sibling_arr[bb] = SSA_INVALID_BB;

  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
  {
    if (bb == function->entry_block || idom[bb] == SSA_INVALID_BB)
      continue;
    SSABasicBlockName *dst = &child_arr[idom[bb]];
    while (*dst != SSA_INVALID_BB)
      dst = &sibling_arr[*dst];
    *dst = bb;
  }

  function->CFG_info.Dom_tree.parent_arr = idom;
  function->CFG_info.Dom_tree.child_arr = child_arr;
  function->CFG_info.Dom_tree.sibling_arr = sibling_arr;

  function->CFG_info.valid_DOM = 1;

  return 1;
}

int require_Dom_tree(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;
  if (function->CFG_info.valid_DOM)
    return 1;

  return build_Dom_tree(module, func);
}

static int build_PDom_tree(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_successors_list(module, func) || !require_back_RPO(module, func))
    return 0;

  SSABasicBlockName *iPdom = malloc(sizeof(SSABasicBlockName) * function->basic_blocks_count);
  for (SSABasicBlockName *i = iPdom; i < iPdom + function->basic_blocks_count; ++i)
    *i = SSA_INVALID_BB;

  iPdom[function->exit_block] = function->exit_block;

  SSABasicBlockList *processed_succs = NULL;
  SSABasicBlockName new_idom = SSA_INVALID_BB;
  int changed = 1;
  while (changed)
  {
    changed = 0;
    for (SSABasicBlockName *BB = function->CFG_info.inverse_RPO_index;
         BB < function->CFG_info.inverse_RPO_index + function->basic_blocks_count;
         ++BB)
    {
      if (*BB == SSA_INVALID_BB || *BB == function->exit_block)
        continue;
      for (SSABasicBlockList *succ = function->CFG_info.succs[*BB]; succ; succ = succ->next)
        if (iPdom[succ->BB] != SSA_INVALID_BB)
          SSABasicBlockList_append(&processed_succs, succ->BB);

      if (processed_succs)
      {
        new_idom = processed_succs->BB;

        for (SSABasicBlockList *p = processed_succs->next; p; p = p->next)
          new_idom = idom_intersect(module, func, iPdom, function->CFG_info.back_RPO_index, new_idom, p->BB);
        if (iPdom[*BB] != new_idom)
        {
          changed = 1;
          iPdom[*BB] = new_idom;
        }
      }

      SSABasicBlockList_destroy(processed_succs);
      processed_succs = NULL;
    }
  }
  SSABasicBlockName *child_arr = malloc(function->basic_blocks_count * sizeof(SSABasicBlockName));
  SSABasicBlockName *sibling_arr = malloc(function->basic_blocks_count * sizeof(SSABasicBlockName));
  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
    child_arr[bb] = sibling_arr[bb] = SSA_INVALID_BB;

  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
  {
    if (bb == function->exit_block || iPdom[bb] == SSA_INVALID_BB)
      continue;
    SSABasicBlockName *dst = &child_arr[iPdom[bb]];
    while (*dst != SSA_INVALID_BB)
      dst = &sibling_arr[*dst];
    *dst = bb;
  }

  function->CFG_info.PDom_tree.parent_arr = iPdom;
  function->CFG_info.PDom_tree.child_arr = child_arr;
  function->CFG_info.PDom_tree.sibling_arr = sibling_arr;

  function->CFG_info.valid_PDOM = 1;

  return 1;
}

int require_PDom_tree(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function)
    return 0;
  if (function->CFG_info.valid_PDOM)
    return 1;

  return build_PDom_tree(module, func);
}

int is_dominator_of(SSAModule *module, SSAFuncName func, SSABasicBlockName dom, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_Dom_tree(module, func) ||
      !is_valid_bb(module, func, dom) || !is_valid_bb(module, func, BB))
    return 0;

  if (dom == function->entry_block)
    return 1;

  while (BB != function->entry_block)
  {
    if (dom == BB)
      return 1;
    BB = function->CFG_info.Dom_tree.parent_arr[BB];
  }
  return 0;
}

SSABasicBlockList *find_natural_loops(SSAModule *module, SSAFuncName func)
{
  SSAFunc *function = get_func(module, func);
  if (!function || !require_Dom_tree(module, func) || !require_successors_list(module, func))
    return NULL;

  SSABasicBlockList *headers = NULL;

  for (SSABasicBlockName bb = 0; bb < function->basic_blocks_count; ++bb)
  {
    for (SSABasicBlockList *successor = function->CFG_info.succs[bb]; successor; successor = successor->next)
      if (is_dominator_of(module, func, successor->BB, bb))
        SSABasicBlockList_append(&headers, successor->BB);
  }

  return headers;
}

SSABasicBlockName CFG_get_cond_joint(SSAModule *module, SSAFuncName func, SSAInstrName cond_goto)
{
  SSAFunc *function = get_func(module, func);
  if (!function || cond_goto == SSA_INVALID_INSTR)
    return SSA_INVALID_BB;

  if (!require_PDom_tree(module, func))
    return SSA_INVALID_BB;

  /*INCOMPLETE*/

  return SSA_INVALID_BB;
}
