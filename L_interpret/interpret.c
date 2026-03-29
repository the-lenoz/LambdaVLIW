#include "interpret.h"
#include "../htable/htable.h"
#include "hash_helper.h"
#include <stdio.h>
#include <string.h>

#define CALL_STACK_SIZE 1024

#define GET_OP(x) (CAR(x)->value)

#define DEFUN_GET_ID(x) (SECOND(x)->value)

typedef struct
{
  int depth;
  char *name;
  AST *source;
} call_t;

typedef struct
{
  int depth;
  call_t trace[CALL_STACK_SIZE];
} call_trace_t;


typedef enum {OK, WARN, FATAL} err_kind;
typedef struct
{
  err_kind kind;
  const char *message;
} err_t;


static call_trace_t call_trace = {};
static err_t err = {};



static int eval_expr(AST *expr, HTable *funcs)
{
  
  return 0;
}    

static int interpret_stmt(AST *stmt, HTable *funcs)
{
  if (!strcmp(GET_OP(stmt), "defun"))
  {
    call_trace.trace[call_trace.depth] = (call_t){call_trace.depth, "defun", stmt};
    call_trace.depth++;
    AST *fun = NULL;
    if (ht_get(funcs, DEFUN_GET_ID(stmt), (void **)&fun))
      return err = (err_t){FATAL, "function already defined"}, -1;
    ht_set(funcs, DEFUN_GET_ID(stmt), THIRD(stmt));
    call_trace.depth--;
  }
  else
    return eval_expr(stmt, funcs);
  return 0;
}    


int interpret_program(AST *program, AST *arg_data)
{
  HTable *functions = ht_init(hash, str_eq, k_dup, v_dup, free_str, 0);

  int status = 0;
  for (AST *block = program->lchild; block && status != -1; block = block->rchild)
    status = interpret_stmt(block->lchild, functions);
  
  ht_destroy(functions);
  return status;
}
int print_call_trace(FILE *fp)
{
  fprintf(fp, "Backtrace (most recent call last):\n");
  for (call_t *call = call_trace.trace; call < call_trace.trace + call_trace.depth; ++call)
  {
    for (int j = 0; j < call->depth; ++j)
      putc(' ', fp);
    fprintf(fp, "in '%s':\n", call->name);
  }
  return call_trace.depth != 0;
}
int print_err(FILE *fp)
{
  switch (err.kind)
  {
  case OK:    
    break;
  case WARN:
    fprintf(fp, "Warn: %s.\n", err.message);    
    break;
  case FATAL:
    fprintf(fp, "Fatal: %s.\n", err.message);
    break;
  default:break;
  }
  return 0;  
}    
