#include "SSA.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define BASIC_ARRAY_SIZE 64

typedef enum
{
  OWNED_LIST_PHI,
  OWNED_LIST_ARG
} OwnedListKind;

typedef struct _OwnedList
{
  SSAModule *owner;
  void *list;
  OwnedListKind kind;
  struct _OwnedList *next;
} OwnedList;

static OwnedList *owned_lists = NULL;

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

static int bb_has_terminator(const SSAFunc *func, SSABasicBlockName bb)
{
  return is_valid_bb(func, bb) && func->basic_blocks[bb].terminator.type != SSA_TERM_NONE;
}

static void destroy_phi_list(PhiList *list)
{
  while (list)
  {
    PhiList *next = list->next;
    free(list);
    list = next;
  }
}

static void destroy_arg_list(ArgList *list)
{
  while (list)
  {
    ArgList *next = list->next;
    free(list);
    list = next;
  }
}

static int register_owned_list(SSAModule *module, void *list, OwnedListKind kind)
{
  OwnedList *node;

  if (!module || !list)
    return -1;

  node = (OwnedList *)malloc(sizeof(OwnedList));
  if (!node)
    return -1;

  node->owner = module;
  node->list = list;
  node->kind = kind;
  node->next = owned_lists;
  owned_lists = node;

  return 0;
}

static void destroy_owned_lists_for_module(SSAModule *module)
{
  OwnedList **slot = &owned_lists;

  while (*slot)
  {
    OwnedList *current = *slot;
    if (current->owner != module)
    {
      slot = &current->next;
      continue;
    }

    if (current->kind == OWNED_LIST_PHI)
      destroy_phi_list((PhiList *)current->list);
    else
      destroy_arg_list((ArgList *)current->list);

    *slot = current->next;
    free(current);
  }
}

static void destroy_func(SSAFunc *func)
{
  if (!func)
    return;

  free(func->name);
  free(func->values);
  free(func->basic_blocks);
  *func = (SSAFunc){};
}

static SSAValName append_value(SSAFunc *function, SSAValue value)
{
  if (!function)
    return SSA_INVALID_VAL;

  if (ensure_array_capacity((void **)&function->values, &function->values_cap, sizeof(SSAValue), function->values_count + 1) < 0)
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

  destroy_owned_lists_for_module(module);

  free(module->functions);
  *module = (SSAModule){};
  free(module);
}

SSAFuncName new_func(SSAModule *module, const char *name, int is_global /*maybe other properties*/)
{
  SSAFunc *func;

  (void)is_global;

  if (!module || !name)
    return SSA_INVALID_FUNC;

  if (ensure_array_capacity((void **)&module->functions, &module->functions_cap, sizeof(SSAFunc), module->functions_count + 1) < 0)
    return SSA_INVALID_FUNC;

  func = &module->functions[module->functions_count];
  *func = (SSAFunc){};

  func->name = (char *)malloc(strlen(name) + 1);
  if (!func->name)
    return SSA_INVALID_FUNC;

  strcpy(func->name, name);

  func->values_cap = BASIC_ARRAY_SIZE;
  func->values = (SSAValue *)calloc(func->values_cap, sizeof(SSAValue));
  if (!func->values)
  {
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

  if (ensure_array_capacity((void **)&function->basic_blocks, &function->basic_blocks_cap, sizeof(SSABasicBlock), function->basic_blocks_count + 1) < 0)
    return SSA_INVALID_BB;

  bb_name = function->basic_blocks_count;
  function->basic_blocks[bb_name] = (SSABasicBlock){
      .parent_name = func,
      .terminator = {
          .type = SSA_TERM_NONE,
          .cond = SSA_INVALID_VAL,
          .true_dst = SSA_INVALID_BB,
          .false_dst = SSA_INVALID_BB,
      }};
  function->basic_blocks_count += 1;

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

  if (!function || !is_valid_bb(function, BB) || !phi_list)
    return SSA_INVALID_VAL;

  value = (SSAValue){};
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

  if (!function || !is_valid_bb(function, BB) || !arg_list)
    return SSA_INVALID_VAL;
  if (!get_func(module, callee))
    return SSA_INVALID_VAL;

  value = (SSAValue){};
  value.is_const = !!is_constexpr;
  value.expr.call.calee_name = callee;
  value.expr.call.args = arg_list;
  value.parent_name = BB;

  return append_value(function, value);
}

int emit_cond_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName cond_name,
                   SSABasicBlockName true_dst, SSABasicBlockName false_dst)
{
  SSAFunc *function = get_func(module, func);
  SSABlockTerminator *terminator;

  if (!function)
    return -1;
  if (!is_valid_bb(function, BB) || !is_valid_bb(function, true_dst) || !is_valid_bb(function, false_dst))
    return -1;
  if (!is_valid_value(function, cond_name))
    return -1;
  if (bb_has_terminator(function, BB))
    return -1;

  terminator = &function->basic_blocks[BB].terminator;
  terminator->type = SSA_TERM_COND_GOTO;
  terminator->cond = cond_name;
  terminator->true_dst = true_dst;
  terminator->false_dst = false_dst;

  return 0;
}

int emit_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSABasicBlockName dst)
{
  SSAFunc *function = get_func(module, func);
  SSABlockTerminator *terminator;

  if (!function)
    return -1;
  if (!is_valid_bb(function, BB) || !is_valid_bb(function, dst))
    return -1;
  if (bb_has_terminator(function, BB))
    return -1;

  terminator = &function->basic_blocks[BB].terminator;
  terminator->type = SSA_TERM_GOTO;
  terminator->cond = SSA_INVALID_VAL;
  terminator->true_dst = dst;
  terminator->false_dst = SSA_INVALID_BB;

  return 0;
}

PhiList *new_PhiList(SSAModule *module, SSAFuncName func, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, func);
  PhiList *list;

  (void)BB;

  if (!function)
    return NULL;

  list = (PhiList *)calloc(1, sizeof(PhiList));
  if (!list)
    return NULL;

  list->pair.previous_block_name = SSA_INVALID_BB;
  list->pair.value_name = SSA_INVALID_VAL;

  if (register_owned_list(module, list, OWNED_LIST_PHI) < 0)
  {
    free(list);
    return NULL;
  }

  return list;
}

ArgList *new_ArgList(SSAModule *module, SSAFuncName func, SSABasicBlockName BB)
{
  SSAFunc *function = get_func(module, func);
  ArgList *list;

  (void)BB;

  if (!function)
    return NULL;

  list = (ArgList *)calloc(1, sizeof(ArgList));
  if (!list)
    return NULL;

  list->name = SSA_INVALID_VAL;

  if (register_owned_list(module, list, OWNED_LIST_ARG) < 0)
  {
    free(list);
    return NULL;
  }

  return list;
}

int PhiList_append(PhiList *list, PhiPair pair)
{
  PhiList *cur;

  if (!list)
    return -1;

  if (list->pair.previous_block_name == SSA_INVALID_BB &&
      list->pair.value_name == SSA_INVALID_VAL &&
      list->next == NULL)
  {
    list->pair = pair;
    return 0;
  }

  cur = list;
  while (cur->next)
    cur = cur->next;

  cur->next = (PhiList *)calloc(1, sizeof(PhiList));
  if (!cur->next)
    return -1;

  cur->next->pair = pair;
  return 0;
}

int ArgList_append(ArgList *list, SSAValName arg_name)
{
  ArgList *cur;

  if (!list)
    return -1;

  if (list->name == SSA_INVALID_VAL && list->next == NULL)
  {
    list->name = arg_name;
    return 0;
  }

  cur = list;
  while (cur->next)
    cur = cur->next;

  cur->next = (ArgList *)calloc(1, sizeof(ArgList));
  if (!cur->next)
    return -1;

  cur->next->name = arg_name;
  return 0;
}
