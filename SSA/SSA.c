#include "SSA.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

static void *realloc_array(void *ptr, size_t count, size_t elem_size)
{
  if (elem_size != 0 && count > SIZE_MAX / elem_size)
    return NULL;
  return realloc(ptr, count * elem_size);
}

static const char *ssa_op_to_string(SSA_operator op)
{
  switch (op)
  {
  case SSA_ADD:
    return "+";
  case SSA_SUB:
    return "-";
  case SSA_MUL:
    return "*";
  case SSA_DIV:
    return "/";
  case SSA_LT:
    return "<";
  case SSA_GT:
    return ">";
  case SSA_LTE:
    return "<=";
  case SSA_GTE:
    return ">=";
  case SSA_EQ:
    return "==";
  default:
    return NULL;
  }
}

static int print_dest_name(FILE *out_fp, int global_name)
{
  if (global_name == SSA_RESULT_GLOBAL_NAME)
    return fprintf(out_fp, "result") < 0 ? -1 : 0;
  if (global_name < 0)
    return -1;
  return fprintf(out_fp, "v%d", global_name) < 0 ? -1 : 0;
}

static int print_operand_name(FILE *out_fp, int operand)
{
  if (operand == SSA_RESULT_GLOBAL_NAME)
    return fprintf(out_fp, "result") < 0 ? -1 : 0;
  if (operand < 0)
  {
    long long imm = -(long long)operand - 1;
    return fprintf(out_fp, "%lld", imm) < 0 ? -1 : 0;
  }
  return fprintf(out_fp, "v%d", operand) < 0 ? -1 : 0;
}

static int print_bb_label(FILE *out_fp, int bb_index)
{
  return fprintf(out_fp, "bb%d", bb_index) < 0 ? -1 : 0;
}

static int emit_basic_block(FILE *out_fp, const SSA_BasicBlock *bb, int bb_index)
{
  if (fprintf(out_fp, "  (") < 0 || print_bb_label(out_fp, bb_index) < 0 || fprintf(out_fp, "\n") < 0)
    return -1;

  for (int i = 0; i < bb->statements_count; ++i)
  {
    const SSA_stmt *stmt = &bb->statements[i];

    switch (stmt->type)
    {
    case SSA_VAL:
    {
      const SSA_val *val = &stmt->variant.val;

      if (fprintf(out_fp, "    (let ") < 0 || print_dest_name(out_fp, val->global_name) < 0 || fprintf(out_fp, " ") < 0)
        return -1;

      if (val->is_phi_node)
      {
        if (!val->phi_list)
          return -1;

        if (fprintf(out_fp, "(phi") < 0)
          return -1;
        for (int j = 0; j < val->phi_list->intems_count; ++j)
        {
          PhiPair pair = val->phi_list->items[j];
          if (fprintf(out_fp, " (") < 0 || print_bb_label(out_fp, pair.BB_index) < 0 || fprintf(out_fp, " ") < 0 ||
              print_operand_name(out_fp, pair.val_global_name) < 0 || fprintf(out_fp, ")") < 0)
            return -1;
        }
        if (fprintf(out_fp, "))\n") < 0)
          return -1;
      }
      else if (val->op == SSA_PASSTHROUGH)
      {
        if (print_operand_name(out_fp, val->arg0_global_name) < 0 || fprintf(out_fp, ")\n") < 0)
          return -1;
      }
      else
      {
        const char *op = ssa_op_to_string(val->op);
        if (!op)
          return -1;
        if (fprintf(out_fp, "(%s ", op) < 0 || print_operand_name(out_fp, val->arg0_global_name) < 0 ||
            fprintf(out_fp, " ") < 0 || print_operand_name(out_fp, val->arg1_global_name) < 0 ||
            fprintf(out_fp, "))\n") < 0)
          return -1;
      }
      break;
    }
    case SSA_IF_GOTO:
    {
      const SSA_if_goto *if_goto = &stmt->variant.if_goto;
      if (fprintf(out_fp, "    (if-goto ") < 0 || print_operand_name(out_fp, if_goto->condition_val_global_name) < 0 ||
          fprintf(out_fp, " ") < 0 || print_bb_label(out_fp, if_goto->true_dst_BB_index) < 0 ||
          fprintf(out_fp, " else ") < 0 || print_bb_label(out_fp, if_goto->false_dst_BB_index) < 0 ||
          fprintf(out_fp, ")\n") < 0)
        return -1;
      break;
    }
    case SSA_GOTO:
      if (fprintf(out_fp, "    (goto ") < 0 || print_bb_label(out_fp, stmt->variant.go.dst_BB_index) < 0 ||
          fprintf(out_fp, ")\n") < 0)
        return -1;
      break;
    default:
      return -1;
    }
  }

  return fprintf(out_fp, "  )\n") < 0 ? -1 : 0;
}

int SSA_Func_init(SSA_Func *f)
{
  assert(f);
  f->basic_blocks_count = 0;
  f->basic_blocks = NULL;
  f->entry_BB_index = -1;
  return 0;
}
int SSA_Func_add_BB(SSA_Func *f, SSA_BasicBlock bb) /*returns index*/
{
  assert(f);

  if (f->basic_blocks_count == INT_MAX)
    return -1;

  int index = f->basic_blocks_count;
  SSA_BasicBlock *new_blocks = realloc_array(f->basic_blocks, (size_t)f->basic_blocks_count + 1, sizeof(*new_blocks));
  if (!new_blocks)
    return -1;

  f->basic_blocks = new_blocks;
  f->basic_blocks[index] = bb;
  f->basic_blocks_count++;

  if (f->entry_BB_index < 0)
    f->entry_BB_index = index;

  return index;
}
SSA_BasicBlock *SSA_Func_get_BB_ptr(SSA_Func *f, int index)
{
  if (!f || index < 0 || index >= f->basic_blocks_count)
    return NULL;
  return &f->basic_blocks[index];
}
int SSA_Func_destroy(SSA_Func *f)
{
  if (!f)
    return -1;

  for (int i = 0; i < f->basic_blocks_count; ++i)
    SSA_BasicBlock_destroy(&f->basic_blocks[i]);

  free(f->basic_blocks);
  *f = (SSA_Func){};
  return 0;
}

int SSA_BasicBlock_init(SSA_BasicBlock *bb)
{
  if (!bb)
    return -1;

  bb->statements_count = 0;
  bb->statements = NULL;
  return 0;
}
int SSA_BasicBlock_add_stmt(SSA_BasicBlock *bb, SSA_stmt stmt)
{
  if (!bb)
    return -1;
  if (bb->statements_count == INT_MAX)
    return -1;

  int index = bb->statements_count;
  SSA_stmt *new_stmts = realloc_array(bb->statements, (size_t)bb->statements_count + 1, sizeof(*new_stmts));
  if (!new_stmts)
    return -1;

  bb->statements = new_stmts;
  bb->statements[index] = stmt;
  bb->statements_count++;
  return index;
}
SSA_stmt SSA_BasicBlock_get_stmt(SSA_BasicBlock *bb, int index)
{
  if (!bb || index < 0 || index >= bb->statements_count)
    return (SSA_stmt){};
  return bb->statements[index];
}
int SSA_BasicBlock_set_stmt(SSA_BasicBlock *bb, int index, SSA_stmt stmt)
{
  if (!bb || index < 0 || index >= bb->statements_count)
    return -1;
  bb->statements[index] = stmt;
  return 0;
}
int SSA_BasicBlock_destroy(SSA_BasicBlock *bb)
{
  if (!bb)
    return -1;

  for (int i = 0; i < bb->statements_count; ++i)
    SSA_stmt_destroy(&bb->statements[i]);

  free(bb->statements);
  *bb = (SSA_BasicBlock){};
  return 0;
}

int SSA_stmt_init(SSA_stmt *stmt, int is_phi_node, PhiList *phi_list, SSA_operator op,
                  int arg0_name, int arg1_name, int arg2_name)
{
  if (!stmt)
    return -1;

  stmt->type = SSA_VAL;
  stmt->variant.val = (SSA_val){
      .global_name = -1,
      .is_phi_node = is_phi_node ? 1 : 0,
      .phi_list = phi_list,
      .op = op,
      .arg0_global_name = arg0_name,
      .arg1_global_name = arg1_name,
      .arg2_global_name = arg2_name,
  };

  return 0;
}
int SSA_stmt_destroy(SSA_stmt *stmt)
{
  if (!stmt)
    return -1;
  *stmt = (SSA_stmt){};
  return 0;
}

int generate_L_tri_if(SSA_Func f, FILE *out_fp)
{
  if (!out_fp)
    return -1;

  if (fprintf(out_fp, "(\n") < 0)
    return -1;

  if (f.entry_BB_index >= 0 && f.entry_BB_index < f.basic_blocks_count)
    if (emit_basic_block(out_fp, &f.basic_blocks[f.entry_BB_index], f.entry_BB_index) < 0)
      return -1;

  for (int i = 0; i < f.basic_blocks_count; ++i)
  {
    if (i == f.entry_BB_index)
      continue;
    if (emit_basic_block(out_fp, &f.basic_blocks[i], i) < 0)
      return -1;
  }

  return fprintf(out_fp, ")\n") < 0 ? -1 : 0;
}
