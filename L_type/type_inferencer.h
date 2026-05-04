#ifndef TYPE_INFERENCER_D
#define TYPE_INFERENCER_D

#include "../parser/AST.h"
#include "../htable/htable.h"
#include <stdint.h>

typedef enum {INFER_SUCCESS, INFER_FAILED} InferStatus;

typedef unsigned int InferVarName;
#define INFER_TYPE_NOT_ASSIGNED ((InferVarName)~0u)
typedef struct _infer_type InferType;

typedef enum
{
  INFER_TYPE_INTEGER,
  INFER_TYPE_STRING,
  INFER_VAR,
  INFER_PAIR,
  INFER_CALL
} InferTypeKind;

typedef struct _type_list
{
  InferVarName cur_type;
  struct _type_list *next;
} TypeList;

struct _infer_type
{
  InferTypeKind kind;

  int64_t integer_value; // for const
  char *string_value;

  InferVarName var_value_type_name; // for var

  InferVarName car_type_name; // for pair
  InferVarName cdr_type_name;

  InferVarName return_type_name;
  TypeList *arg_types;
};

typedef struct
{
  unsigned int vars_count;
  unsigned int vars_cap;
  InferType *vars;

  HTable *functions_typing;
} TypeInferencer;

typedef struct _typed_AST
{
  AST_T parser_type;

  int value_len;
  char *value;

  InferVarName type_label;

  struct _typed_AST *lchild;
  struct _typed_AST *rchild;
} TypedAST;

TypeInferencer *new_inferencer();
void destroy_inferencer(TypeInferencer *inferencer);
TypedAST *inference_expr(TypeInferencer *inferencer, AST *expr);

#endif
