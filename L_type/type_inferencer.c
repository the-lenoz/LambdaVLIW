#include "type_inferencer.h"
#include <stdlib.h>

#define BASIC_ARRAY_SIZE 64

TypeInferencer *new_inferencer()
{
  TypeInferencer *inferencer = malloc(sizeof(TypeInferencer));

  if (!inferencer)
    return NULL;
  
  inferencer->functions_typing = ht_init(0);
  if (!inferencer->functions_typing)
  {
    free(inferencer);
    return NULL;
  }

  inferencer->vars_count = 0;
  inferencer->vars_cap = BASIC_ARRAY_SIZE;

  inferencer->vars = calloc(inferencer->vars_cap, sizeof(InferType));

  if (!inferencer->vars)
  {
    ht_destroy(inferencer->functions_typing);
    free(inferencer);
    return NULL;
  }    

  return inferencer;
}

void destroy_inferencer(TypeInferencer *inferencer)
{
  if (!inferencer)
    return;
  ht_destroy(inferencer->functions_typing);
  free(inferencer->vars);
  *inferencer = (TypeInferencer){};  
  free(inferencer);
}

static int is_valid_var_name(TypeInferencer *inferencer, InferVarName t)
{
  if (!inferencer || t >= inferencer->vars_count)
    return 0;
  return 1;
}

static InferVarName add_type_name(TypeInferencer *inferencer)
{
  if (!inferencer)
    return INFER_TYPE_NOT_ASSIGNED;
  if (inferencer->vars_cap <= inferencer->vars_count)
  {
    inferencer->vars = realloc(inferencer->vars, inferencer->vars_cap * 2 * sizeof(InferType));
    inferencer->vars_cap *= 2;
  }
  inferencer->vars_count++;
  
  return inferencer->vars_count - 1;
}

static int requires_type_assignment(TypeInferencer *inferencer, AST *node)
{
  if (!node)
    return 0;
  switch (node->type)
  {
  case CONST_NUM:
    return 1;
  case CONST_STR:
    return 1;
  case NAME:
    return 1;
  case SEXP:
    return 1;
  case CODE_BLOCK:
    return 0;
  case PROGRAM:
    return 0;
  default: // for unused "LIST"
    return 0;
  }
}

static TypedAST *typed_clone_AST(TypeInferencer *inferencer, AST *tree)
{
  if (!tree || !inferencer)
    return NULL;
  TypedAST *node = malloc(sizeof(TypedAST));
  if (!node)
    return NULL;

  InferVarName type_name = INFER_TYPE_NOT_ASSIGNED;
  if (requires_type_assignment(inferencer, tree))
    add_type_name(inferencer);

  *node = (TypedAST){
      tree->type,
      tree->value_len,
      strdup(tree->value),
      type_name,
      typed_clone_AST(inferencer, CAR(tree)),
      typed_clone_AST(inferencer, CDR(tree))};
  return node;
}

void destroy_typed_AST(TypedAST *tree)
{
  if (!tree)
    return;
  destroy_typed_AST(tree->lchild);
  destroy_typed_AST(tree->rchild);
  free(tree->value);
  free(tree);  
}    

InferVarName prune(TypeInferencer *inferencer, InferVarName t)
{
  if (!inferencer || !is_valid_var_name(inferencer, t))
    return INFER_TYPE_NOT_ASSIGNED;
  InferType *type = &inferencer->vars[t];
  if (type->kind == INFER_VAR)
    return prune(inferencer, type->var_value_type_name);
  return t;
}

int depends_on(TypeInferencer *inferencer, InferVarName t, InferVarName possible_dependency)
{
  return 0; // TODO
}

InferStatus unify(TypeInferencer *inferencer, InferVarName t1, InferVarName t2)
{
  return INFER_SUCCESS; // TOOO
}

TypedAST *inference_expr(TypeInferencer *inferencer, AST *expr)
{
  TypedAST *tree = typed_clone_AST(inferencer, expr);

  
}    
