#ifndef SSA_D
#define SSA_D

typedef unsigned int SSAValName;
typedef unsigned int SSABasicBlockName;
typedef unsigned int SSAFuncName;

#define SSA_INVALID_VAL ((SSAValName)~0u)
#define SSA_INVALID_BB ((SSABasicBlockName)~0u)
#define SSA_INVALID_FUNC ((SSAFuncName)~0u)

typedef struct _SSAModule SSAModule;

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

typedef struct
{
  SSAFuncName calee_name;
  ArgList *args;
} _FuncCall;

typedef struct
{
  PhiList *options;
} _PhiNode;

typedef union
{
  _PhiNode phi;
  _FuncCall call;
} _SSAExpr;

typedef enum
{
  SSA_VALUE_INVALID = 0,
  SSA_VALUE_PHI,
  SSA_VALUE_CALL,
} SSAValueType;

typedef struct
{
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

typedef struct
{
  SSAFuncName parent_name;
  SSABlockTerminator terminator;
} SSABasicBlock;

typedef struct
{
  char *name;

  unsigned int values_count;
  unsigned int values_cap;
  SSAValue *values;

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
SSAFuncName new_func(SSAModule *module, const char *name, int is_global /*maybe other properties*/);
SSABasicBlockName new_BB(SSAModule *module, SSAFuncName func, int is_entry, int is_exit);
SSAValName emit_phi_assign(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, PhiList *phi_list);
SSAValName emit_call_assign(SSAModule *module, SSAFuncName func,
                            SSABasicBlockName BB, SSAFuncName callee, ArgList *arg_list, int is_constexpr);

int emit_cond_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName cond_name,
                   SSABasicBlockName true_dst, SSABasicBlockName false_dst);
int emit_goto(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSABasicBlockName dst);
int emit_return(SSAModule *module, SSAFuncName func, SSABasicBlockName BB, SSAValName ret_name);

PhiList *new_PhiList(SSAModule *module, SSAFuncName func, SSABasicBlockName BB);
ArgList *new_ArgList(SSAModule *module, SSAFuncName func, SSABasicBlockName BB);

int PhiList_append(PhiList *list, PhiPair pair);
int ArgList_append(ArgList *list, SSAValName arg_name);

#endif
