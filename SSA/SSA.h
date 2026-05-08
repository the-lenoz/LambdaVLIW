#ifndef SSA_D
#define SSA_D

#include <stdint.h>
typedef unsigned int SSAValName;
typedef unsigned int SSABasicBlockName;
typedef unsigned int SSAFuncName;

#define SSA_INVALID_VAL ((SSAValName)~0u)
#define SSA_VALUE_VOID ((SSAValName)~1u)
#define SSA_INVALID_BB ((SSABasicBlockName)~0u)
#define SSA_INVALID_FUNC ((SSAFuncName)~0u)

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

typedef struct
{
  SSAInstrKind kind;
  SSAValName val;
  _FuncCall call;
  SSABlockTerminator term;
} SSAInstr;

typedef struct
{
  SSAFuncName parent_name;

  unsigned int instructions_count;
  unsigned int instructions_cap;
  SSAInstr *instructions;

} SSABasicBlock;

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

SSABasicBlockName new_BB(SSAModule *module, SSAFuncName func, int is_entry, int is_exit);
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
int ArgList_append(ArgList **list, SSAValName arg_name);


SSAFunc *get_func(SSAModule *module, SSAFuncName fn);
int bb_has_terminator(const SSAFunc *func, SSABasicBlockName bb);
SSAInstr get_BB_terminator(SSAModule *module, SSAFuncName fn, SSABasicBlockName BB);

#endif
