#ifndef SSA_D
#define SSA_D
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
  SSA_PASSTHROUGH,
  SSA_ADD,
  SSA_SUB,
  SSA_MUL,
  SSA_DIV,
  SSA_LT,
  SSA_GT,
  SSA_LTE,
  SSA_GTE,
  SSA_EQ
} SSA_operator;

typedef struct
{
  int BB_index;
  int val_global_name;
} PhiPair;

typedef struct
{
  int intems_count;
  PhiPair *items;
} PhiList;

typedef struct
{
  int global_name;
  int is_phi_node;

  PhiList *phi_list;
  SSA_operator op;
  int arg0_global_name;
  int arg1_global_name;
  int arg2_global_name;

} SSA_val;

typedef struct
{
  int condition_val_global_name;
  int true_dst_BB_index;
  int false_dst_BB_index;
} SSA_if_goto;

typedef struct
{
  int dst_BB_index;
} SSA_goto;

typedef enum
{
  SSA_VAL,
  SSA_IF_GOTO,
  SSA_GOTO
} SSA_stmt_type;

typedef union
{
  SSA_val val;
  SSA_if_goto if_goto;
  SSA_goto go;
} SSA_stmt_variant;

typedef struct
{
  SSA_stmt_type type;
  SSA_stmt_variant variant;
} SSA_stmt;

typedef struct
{
  int statements_count;
  SSA_stmt *statements;
} SSA_BasicBlock;

typedef struct
{
  int basic_blocks_count;
  SSA_BasicBlock *basic_blocks;
  int entry_BB_index;
} SSA_Func;

int SSA_Func_init(SSA_Func *f);
int SSA_Func_add_BB(SSA_Func *f, SSA_BasicBlock bb); /*returns index*/
SSA_BasicBlock *SSA_Func_get_BB_ptr(SSA_Func *f, int index);
int SSA_Func_destroy(SSA_Func *f);

int SSA_BasicBlock_init(SSA_BasicBlock *bb);
int SSA_BasicBlock_add_stmt(SSA_BasicBlock *bb, SSA_stmt stmt);
SSA_stmt SSA_BasicBlock_get_stmt(SSA_BasicBlock *bb, int index);
int SSA_BasicBlock_set_stmt(SSA_BasicBlock *bb, int index, SSA_stmt stmt);
int SSA_BasicBlock_destroy(SSA_BasicBlock *bb);

int SSA_stmt_init(SSA_stmt *stmt, int is_phi_node, PhiList *phi_list, SSA_operator op,
                  int arg0_name, int arg1_name, int arg2_name);
int SSA_stmt_destroy(SSA_stmt *stmt);

int generate_L_tri_if(SSA_Func f, FILE *out_fp);

#define SSA_RESULT_GLOBAL_NAME INT_MIN
#define SSA_IMM(value) (-(int)((value) + 1))

#define SSA_ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define SSA_BLOCK(name, ...) SSA_stmt name[] = {__VA_ARGS__}

#define SSA_PHI_PAIR(bb_index, value_name) ((PhiPair){.BB_index = (bb_index), .val_global_name = (value_name)})

#define SSA_STMT_VAL(dst_name, operator_name, arg0, arg1, arg2) \
  ((SSA_stmt){                                                  \
      .type = SSA_VAL,                                          \
      .variant.val = {                                          \
          .global_name = (dst_name),                            \
          .is_phi_node = 0,                                     \
          .phi_list = NULL,                                     \
          .op = (operator_name),                                \
          .arg0_global_name = (arg0),                           \
          .arg1_global_name = (arg1),                           \
          .arg2_global_name = (arg2),                           \
      },                                                        \
  })

#define SSA_STMT_PHI(dst_name, phi_inputs) \
  ((SSA_stmt){                             \
      .type = SSA_VAL,                     \
      .variant.val = {                     \
          .global_name = (dst_name),       \
          .is_phi_node = 1,                \
          .phi_list = (phi_inputs),        \
          .op = SSA_PASSTHROUGH,           \
          .arg0_global_name = -1,          \
          .arg1_global_name = -1,          \
          .arg2_global_name = -1,          \
      },                                   \
  })

#define SSA_STMT_MOV(dst_name, src_name) SSA_STMT_VAL((dst_name), SSA_PASSTHROUGH, (src_name), -1, -1)
#define SSA_STMT_CONST(dst_name, imm_value) SSA_STMT_MOV((dst_name), SSA_IMM(imm_value))

#define SSA_STMT_ADD(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_ADD, (lhs_name), (rhs_name), -1)
#define SSA_STMT_SUB(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_SUB, (lhs_name), (rhs_name), -1)
#define SSA_STMT_MUL(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_MUL, (lhs_name), (rhs_name), -1)
#define SSA_STMT_DIV(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_DIV, (lhs_name), (rhs_name), -1)
#define SSA_STMT_LT(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_LT, (lhs_name), (rhs_name), -1)
#define SSA_STMT_GT(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_GT, (lhs_name), (rhs_name), -1)
#define SSA_STMT_LTE(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_LTE, (lhs_name), (rhs_name), -1)
#define SSA_STMT_GTE(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_GTE, (lhs_name), (rhs_name), -1)
#define SSA_STMT_EQ(dst_name, lhs_name, rhs_name) SSA_STMT_VAL((dst_name), SSA_EQ, (lhs_name), (rhs_name), -1)

#define SSA_STMT_IF(cond_name, true_bb, false_bb)   \
  ((SSA_stmt){                                      \
      .type = SSA_IF_GOTO,                          \
      .variant.if_goto = {                          \
          .condition_val_global_name = (cond_name), \
          .true_dst_BB_index = (true_bb),           \
          .false_dst_BB_index = (false_bb),         \
      },                                            \
  })

#define SSA_STMT_GOTO(dst_bb)       \
  ((SSA_stmt){                      \
      .type = SSA_GOTO,             \
      .variant.go = {               \
          .dst_BB_index = (dst_bb), \
      },                            \
  })

static inline PhiList *SSA_PhiList_new(const PhiPair *pairs, int count)
{
  if (count < 0)
    return NULL;

  PhiList *phi_list = malloc(sizeof(*phi_list));
  if (!phi_list)
    return NULL;

  phi_list->intems_count = count;
  phi_list->items = NULL;

  if (!count)
    return phi_list;

  phi_list->items = malloc((size_t)count * sizeof(*phi_list->items));
  if (!phi_list->items)
  {
    free(phi_list);
    return NULL;
  }

  memcpy(phi_list->items, pairs, (size_t)count * sizeof(*phi_list->items));
  return phi_list;
}

#define SSA_PHI_LIST_NEW(...) \
  SSA_PhiList_new((PhiPair[]){__VA_ARGS__}, (int)(sizeof((PhiPair[]){__VA_ARGS__}) / sizeof(PhiPair)))

static inline void SSA_PhiList_destroy(PhiList *phi_list)
{
  if (!phi_list)
    return;
  free(phi_list->items);
  free(phi_list);
}

static inline int SSA_Func_add_BB_from_array(SSA_Func *f, const SSA_stmt *stmts, int stmts_count)
{
  if (!f || !stmts || stmts_count < 0)
    return -1;

  SSA_BasicBlock bb;
  if (SSA_BasicBlock_init(&bb) < 0)
    return -1;

  for (int i = 0; i < stmts_count; ++i)
  {
    if (SSA_BasicBlock_add_stmt(&bb, stmts[i]) < 0)
    {
      SSA_BasicBlock_destroy(&bb);
      return -1;
    }
  }

  int index = SSA_Func_add_BB(f, bb);
  if (index < 0)
  {
    SSA_BasicBlock_destroy(&bb);
    return -1;
  }

  return index;
}

static inline int SSA_Func_add_BB_expect(SSA_Func *f, int expected_index, const SSA_stmt *stmts, int stmts_count)
{
  int index = SSA_Func_add_BB_from_array(f, stmts, stmts_count);
  return index == expected_index ? 0 : -1;
}

#endif
