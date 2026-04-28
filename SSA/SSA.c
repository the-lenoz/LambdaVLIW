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

static SSAFunc *get_func(SSAModule *module, SSAFuncName func)
{
  if (!module || func >= module->functions_count)
    return NULL;

  return &module->functions[func];
}

static int is_valid_bb(const SSAFunc *func, SSABasicBlockName bb)
{
  return func && bb < func->basic_blocks_count;
}

static int is_valid_value(const SSAFunc *func, SSAValName value)
{
  return func && value < func->values_count;
}

static int is_valid_or_void_value(const SSAFunc *func, SSAValName value)
{
  return value == SSA_VAL_VOID || is_valid_value(func, value);
}

static int bb_has_terminator(const SSAFunc *func, SSABasicBlockName bb)
{
  return is_valid_bb(func, bb) && func->basic_blocks[bb].terminator.type != SSA_TERM_NONE;
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

static void destroy_func(SSAFunc *func)
{
  if (!func)
    return;

  for (int i = 0; i < func->values_count; ++i)
  {
    if (func->values[i].type == SSA_VALUE_CALL)
      destroy_arg_list(func->values[i].expr.call.args);
    else if (func->values[i].type == SSA_VALUE_PHI)
      destroy_phi_list(func->values[i].expr.phi.options);
  }

  free(func->name);
  free(func->values);
  free(func->basic_blocks);
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
    destroy_func(&module->functions[i]);

  free(module->functions);
  *module = (SSAModule){};
  free(module);
}

SSAFuncName new_func(SSAModule *module, const char *name, unsigned int args_count, int is_global)
{
  SSAFunc *func;

  (void)is_global;

  if (!module || !name)
    return SSA_INVALID_FUNC;

  if (ensure_array_capacity((void **)&module->functions, &module->functions_cap,
                            sizeof(SSAFunc), module->functions_count + 1) < 0)
    return SSA_INVALID_FUNC;

  func = &module->functions[module->functions_count];
  *func = (SSAFunc){};

  func->name = strdup(name);
  if (!func->name)
    return SSA_INVALID_FUNC;

  func->values_cap = BASIC_ARRAY_SIZE + args_count;
  func->values = (SSAValue *)calloc(func->values_cap, sizeof(SSAValue));
  if (!func->values)
  {
    free(func->name);
    *func = (SSAFunc){};
    return SSA_INVALID_FUNC;
  }

  func->values_count = args_count;
  for (unsigned int i = 0; i < args_count; ++i)
  {
    func->values[i].type = SSA_VALUE_ARG;
  }

  func->args_count = args_count;
  func->arg_SSA_names = (SSAValName *)calloc(func->args_count, sizeof(SSAValName));
  if (!func->arg_SSA_names)
  {
    free(func->values);
    free(func->name);
    *func = (SSAFunc){};
    return SSA_INVALID_FUNC;
  }

  func->basic_blocks_cap = BASIC_ARRAY_SIZE;
  func->basic_blocks = (SSABasicBlock *)calloc(func->basic_blocks_cap, sizeof(SSABasicBlock));
  if (!func->basic_blocks)
  {
    free(func->values);
    free(func->name);
    free(func->arg_SSA_names);
    *func = (SSAFunc){};
    return SSA_INVALID_FUNC;
  }

  func->entry_block = SSA_INVALID_BB;
  func->exit_block = SSA_INVALID_BB;
  func->parent_module = module;

  module->functions_count += 1;
  return module->functions_count - 1;
}

SSABasicBlockName new_BB(SSAModule *module, SSAFuncName func, int is_entry, int is_exit)
{
  SSAFunc *function = get_func(module, func);
  SSABasicBlockName bb_name;

  if (!function)
    return SSA_INVALID_BB;

  if (is_entry && function->entry_block != SSA_INVALID_BB)
    return SSA_INVALID_BB;
  if (is_exit && function->exit_block != SSA_INVALID_BB)
    return SSA_INVALID_BB;

  if (ensure_array_capacity((void **)&function->basic_blocks, &function->basic_blocks_cap,
                            sizeof(SSABasicBlock), function->basic_blocks_count + 1) < 0)
    return SSA_INVALID_BB;

  bb_name = function->basic_blocks_count;
  function->basic_blocks[bb_name] = (SSABasicBlock){
      func,
      {
          .type = SSA_TERM_NONE,
          .cond = SSA_INVALID_VAL,
          .ret_val = SSA_INVALID_VAL,
          .true_dst = SSA_INVALID_BB,
          .false_dst = SSA_INVALID_BB,
      }};
  function->basic_blocks_count++;

  if (is_entry)
    function->entry_block = bb_name;
  if (is_exit)
    function->exit_block = bb_name;

  return bb_name;
}

SSAValName emit_phi_assign(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, PhiList *phi_list)
{
  SSAFunc *function = get_func(module, func);
  SSAValue value;

  if (!function || !is_valid_bb(function, BB))
    return SSA_INVALID_VAL;

  value = (SSAValue){};
  value.type = SSA_VALUE_PHI;
  value.is_const = 0;
  value.expr.phi.options = phi_list;
  value.parent_name = BB;

  return append_value(function, value);
}

SSAValName emit_call_assign(SSAModule *module, SSAFuncName func,
                            SSABasicBlockName BB, SSAFuncName callee, ArgList *arg_list, int is_constexpr)
{
  SSAFunc *function = get_func(module, func);
  SSAValue value;

  if (!function || !is_valid_bb(function, BB))
    return SSA_INVALID_VAL;
  if (!get_func(module, callee))
    return SSA_INVALID_VAL;

  value = (SSAValue){};
  value.type = SSA_VALUE_CALL;
  value.is_const = !!is_constexpr;
  value.expr.call.calee_name = callee;
  value.expr.call.args = arg_list;
  value.parent_name = BB;

  return append_value(function, value);
}

SSAValName get_arg_val_name(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, unsigned int arg_index)
{
  if (!module || func >= module->functions_count || BB >= module->functions[func].basic_blocks_count ||
      arg_index >= module->functions[func].args_count)
    return SSA_INVALID_VAL;

  return module->functions[func].arg_SSA_names[arg_index];
}

SSAValName emit_const_assign(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, int value)
{
  SSAFunc *function = get_func(module, func);
  SSAValue ssa_value;

  if (!function || !is_valid_bb(function, BB))
    return SSA_INVALID_VAL;

  ssa_value = (SSAValue){};
  ssa_value.type = SSA_VALUE_CONST;
  ssa_value.is_const = 1;
  ssa_value.expr.cnst.value = value;
  ssa_value.parent_name = BB;

  return append_value(function, ssa_value);
}

int emit_cond_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName cond_name,
                   SSABasicBlockName true_dst, SSABasicBlockName false_dst)
{
  SSAFunc *function = get_func(module, func);

  if (!function)
    return -1;
  if (!is_valid_bb(function, BB) || !is_valid_bb(function, true_dst) || !is_valid_bb(function, false_dst))
    return -1;
  if (!is_valid_value(function, cond_name))
    return -1;
  if (bb_has_terminator(function, BB))
    return -1;

  function->basic_blocks[BB].terminator = (SSABlockTerminator){
      SSA_TERM_COND_GOTO,
      cond_name,
      SSA_INVALID_VAL,
      true_dst,
      false_dst};

  return 0;
}

int emit_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSABasicBlockName dst)
{
  SSAFunc *function = get_func(module, func);

  if (!function)
    return -1;
  if (!is_valid_bb(function, BB) || !is_valid_bb(function, dst))
    return -1;
  if (bb_has_terminator(function, BB))
    return -1;

  function->basic_blocks[BB].terminator = (SSABlockTerminator){
      SSA_TERM_GOTO,
      SSA_INVALID_VAL,
      SSA_INVALID_VAL,
      dst,
      SSA_INVALID_BB};

  return 0;
}

int emit_return(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName ret_name)
{
  SSAFunc *function = get_func(module, func);

  if (!function)
    return -1;
  if (!is_valid_bb(function, BB))
    return -1;
  if (function->exit_block == SSA_INVALID_BB || BB != function->exit_block)
    return -1;
  if (!is_valid_or_void_value(function, ret_name))
    return -1;
  if (bb_has_terminator(function, BB))
    return -1;

  function->basic_blocks[BB].terminator = (SSABlockTerminator){
      SSA_TERM_RETURN,
      SSA_INVALID_VAL,
      ret_name,
      SSA_INVALID_BB,
      SSA_INVALID_BB};
  return 0;
}

int PhiList_append(PhiList **list, PhiPair pair)
{
  if (!list)
    return -1;

  PhiList *new_head = malloc(sizeof(PhiList));
  *new_head = (PhiList){pair, *list};
  *list = new_head;

  return 0;
}

int ArgList_append(ArgList **list, SSAValName arg_name)
{
  if (!list)
    return -1;

  if (!*list)
  {
    *list = malloc(sizeof(ArgList));
    **list = (ArgList){arg_name, NULL};
    return 0;
  }

  return ArgList_append(&(*list)->next, arg_name);
}
