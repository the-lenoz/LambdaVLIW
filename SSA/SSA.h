#ifndef SSA_D
#define SSA_D

#include <stdint.h>

typedef struct _instr SSAInstr;

typedef unsigned int SSAValName;
typedef unsigned int SSABasicBlockName;
typedef unsigned int SSAFuncName;
typedef SSAInstr *SSAInstrName;

#define SSA_INVALID_VAL ((SSAValName)~0u)
#define SSA_VALUE_VOID ((SSAValName)~1u)
#define SSA_INVALID_BB ((SSABasicBlockName)~0u)
#define SSA_INVALID_FUNC ((SSAFuncName)~0u)
#define SSA_INVALID_INSTR NULL

typedef struct _SSAModule SSAModule;

typedef enum
{
  SSA_i1,
  SSA_i8,
  SSA_i32,
  SSA_i64,
  SSA_fp32,
  SSA_fp64,
  SSA_void,
  SSA_invalid_type
} SSAValueType;

typedef struct
{
  SSABasicBlockName previous_block_name;
  SSAValName value_name;
} PhiPair;

typedef struct _PhiList
{
  PhiPair pair;
  struct _PhiList *next;
} PhiList;

typedef struct _ArgList
{
  SSAValName name;
  struct _ArgList *next;
} ArgList;

#define ARG_FIRST(args) (args)->name
#define ARG_SECOND(args) (args)->next->name
#define ARG_THIRD(args) (args)->next->next->name

typedef struct
{
  SSAFuncName calee_name;
  ArgList *args;
} _FuncCall;

typedef union
{
  int64_t i1_value;
  int64_t i8_value;
  int64_t i32_value;
  int64_t i64_value;
  double fp32_value;
  double fp64_value;
} SSAConst;

typedef struct
{
  PhiList *options;
} _PhiNode;

typedef union
{
  _PhiNode phi;
  _FuncCall call;
  SSAConst cnst;
  SSAValName bool_val;
} _SSAExpr;

typedef enum
{
  SSA_VALUE_INVALID = 0,
  SSA_VALUE_PHI,
  SSA_VALUE_CALL,
  SSA_VALUE_ARG,
  SSA_VALUE_CONST,
  SSA_VALUE_BOOL_CAST
} SSAValueKind;

typedef struct
{
  SSAValueKind kind;
  SSAValueType type;
  int is_const; /*Ability of bijection between name and expression*/
  _SSAExpr expr;
  SSABasicBlockName parent_name;
} SSAValue;

typedef enum
{
  SSA_TERM_NONE,
  SSA_TERM_GOTO,
  SSA_TERM_COND_GOTO,
  SSA_TERM_RETURN
} BlockTerminatorType;

typedef struct
{
  BlockTerminatorType type;
  SSAValName cond;
  SSAValName ret_val;
  SSABasicBlockName true_dst;
  SSABasicBlockName false_dst;
} SSABlockTerminator;

typedef enum
{
  SSA_INSTR_VAL,
  SSA_INSTR_VOID_CALL,
  SSA_INSTR_TERM
} SSAInstrKind;

struct _instr
{
  SSAInstrKind kind;
  SSAValName val;
  _FuncCall call;
  SSABlockTerminator term;

  SSAInstrName prev;
  SSAInstrName next;
};

typedef struct
{
  SSAFuncName parent_name;

  SSAInstrName first_instr;
  SSAInstrName last_instr;

} SSABasicBlock;

typedef struct _i_list
{
  SSAInstrName instr;
  struct _i_list *next;
} SSAInstrList;

typedef struct _bb_list
{
  SSABasicBlockName BB;
  struct _bb_list *next;
} SSABasicBlockList;

typedef struct
{
  SSABasicBlockName *parent_arr;
  SSABasicBlockName *child_arr;
  SSABasicBlockName *sibling_arr;
} BBTree;

typedef struct
{
} CFGLoop;

typedef struct
{
} CFGFork;

typedef enum
{
  CFG_COMMON_BB = 0,
  CFG_LOOP_HEADER,
  CFG_LOOP_LATCH,
  CFG_FORK_COND_BB,
  CFG_FORK_JOINT
} SSABasicBlockRoleType;

typedef struct
{
  SSABasicBlockRoleType role;
  int parent_idx; /*Parent may be a fork or a loop or nothing*/
} SSABasicBlockCFGRole;

typedef struct
{
  int loops_count;
  CFGLoop *loops;

  int forks_count;
  CFGFork *forks;

  SSABasicBlockCFGRole *block_roles;
} CFGStructureAnnotation;

typedef struct
{
  int valid_entry_reachable;
  int valid_exit_reachable;
  int valid_preds;
  int valid_succs;
  int valid_RPO;
  int valid_back_RPO;
  int valid_DOM;
  int valid_PDOM;

  int valid_structure_annotation;

  int *entry_reachable;
  int *exit_reachable;

  SSABasicBlockList **preds;
  SSABasicBlockList **succs;

  int *RPO_index;
  SSABasicBlockName *inverse_RPO_index;

  int *back_RPO_index;
  SSABasicBlockName *inverse_back_RPO_index;

  BBTree Dom_tree;
  BBTree PDom_tree;

  CFGStructureAnnotation structure_annotation;
} CFGInfo;

typedef struct
{
  char *name;
  SSAValueType return_type;
  SSAValueType *arg_types;

  unsigned int values_count;
  unsigned int values_cap;
  SSAValue *values;

  unsigned int args_count;
  SSAValName *arg_SSA_names;

  unsigned int basic_blocks_count;
  unsigned int basic_blocks_cap;
  SSABasicBlock *basic_blocks;

  SSABasicBlockName entry_block;
  SSABasicBlockName exit_block;

  CFGInfo CFG_info;

  SSAModule *parent_module;
} SSAFunc;

struct _SSAModule
{
  unsigned int functions_count;
  unsigned int functions_cap;
  SSAFunc *functions;
};
SSAModule *new_module();
void destroy_module(SSAModule *module);
SSAFuncName new_func(SSAModule *module, const char *name,
                     SSAValueType return_type,
                     unsigned int args_count,
                     const SSAValueType *arg_types, int is_global);

SSABasicBlockName new_BB(SSAModule *module, SSAFuncName func);

int set_entry_BB(SSAModule *module, SSAFuncName func, SSABasicBlockName BB);
int set_exit_BB(SSAModule *module, SSAFuncName func, SSABasicBlockName BB);

SSAValName emit_phi_assign(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                           SSAValueType type);
SSAValName emit_call_assign(SSAModule *module, SSAFuncName func,
                            SSABasicBlockName BB, SSAFuncName callee, ArgList *arg_list, int is_constexpr);
SSAValName get_arg_val_name(SSAModule *module, SSAFuncName func, unsigned int arg_index);
SSAValName emit_const_assign(SSAModule *module, SSAFuncName func,
                             SSABasicBlockName BB, SSAValueType type, SSAConst val);

SSAValName emit_bool_cast_assign(SSAModule *module, SSAFuncName func,
                                 SSABasicBlockName BB, SSAValName val, SSAValueType type);

int emit_void_call(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                   SSAFuncName callee, ArgList *arg_list);

int emit_cond_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName cond_name,
                   SSABasicBlockName true_dst, SSABasicBlockName false_dst);
int emit_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSABasicBlockName dst);
int emit_return(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName ret_name);

int add_phi_option(SSAModule *module, SSAFuncName fn, SSAValName val_name, PhiPair pair);
int remove_phi_option_by_pred(SSAModule *module, SSAFuncName fn, SSAValName val_name, SSABasicBlockName pred);
int ArgList_append(ArgList **list, SSAValName arg_name);

int SSAInstrList_append(SSAInstrList **list, SSAInstrName instr);
void SSAInstrList_destroy(SSAInstrList *list);

int SSABasicBlockList_append(SSABasicBlockList **list, SSABasicBlockName bb);
void SSABasicBlockList_destroy(SSABasicBlockList *list);

SSAInstrList *find_all_val_usages(SSAModule *module, SSAFuncName func, SSAValName val);
int rename_all_val_uses(SSAModule *module, SSAFuncName func, SSAValName old, SSAValName new);

SSAFunc *get_func(SSAModule *module, SSAFuncName fn);
int bb_has_terminator(SSAModule *module, SSAFuncName func, SSABasicBlockName bb);
SSAInstrName get_BB_terminator(SSAModule *module, SSAFuncName fn, SSABasicBlockName BB);
int clear_BB_terminator(SSAModule *module, SSAFuncName fn, SSABasicBlockName BB);

unsigned get_BB_instr_count(SSAModule *module, SSAFuncName func, SSABasicBlockName BB);
SSABasicBlockName get_val_declaration_BB(SSAModule *module, SSAFuncName func, SSAValName value);
SSAInstrName get_val_declaration_instr(SSAModule *module, SSAFuncName func, SSAValName value);

int insert_instr_before(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                        SSAInstrName place, SSAInstr instr);
int insert_instr_after(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                       SSAInstrName place, SSAInstr instr);
int replace_instr(SSAModule *module, SSAFuncName func, SSABasicBlockName BB,
                  SSAInstrName place, SSAInstr instr);
int remove_instr(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAInstrName instr);

int is_valid_bb(SSAModule *module, SSAFuncName func, SSABasicBlockName bb);
int is_valid_value(SSAModule *module, SSAFuncName func, SSAValName val);
int validate_func(SSAModule *module, SSAFuncName func);

int require_entry_reachable(SSAModule *module, SSAFuncName func);
int require_exit_reachable(SSAModule *module, SSAFuncName func);

int require_RPO(SSAModule *module, SSAFuncName func);
int require_back_RPO(SSAModule *module, SSAFuncName func);

int require_predecessors_list(SSAModule *module, SSAFuncName func);
int require_successors_list(SSAModule *module, SSAFuncName func);

int require_Dom_tree(SSAModule *module, SSAFuncName func);
int require_PDom_tree(SSAModule *module, SSAFuncName func);

int is_dominator_of(SSAModule *module, SSAFuncName func, SSABasicBlockName A, SSABasicBlockName B);

#endif
