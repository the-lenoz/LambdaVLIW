#include "interpret.h"
#include "../htable/htable.h"
#include "hash_helper.h"
#include "itoa.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALL_STACK_SIZE 1024

#define ESC2C(c) ((c) == 'n'    ? '\n' \
                  : (c) == 't'  ? '\t' \
                  : (c) == 'r'  ? '\r' \
                  : (c) == '\\' ? '\\' \
                  : (c) == '"'  ? '"'  \
                                : ' ')

#define GET_OP(x) (CAR(x)->value)

#define DEFUN_GET_ID(x) (SECOND(x)->value)

#define STR_MATCH(val)                      \
  const char *_str_switch_internal = (val); \
  {
#define STR_CASE(pattern)                     \
  }                                           \
  if (!strcmp(pattern, _str_switch_internal)) \
  {
#define STR_DEFAULT \
  }                 \
  else              \
  {
#define STR_MATCH_END }

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

typedef enum
{
  OK,
  WARN,
  FATAL
} err_kind;
typedef struct
{
  err_kind kind;
  const char *message;
} err_t;

static call_trace_t call_trace = {};
static err_t err = {};

static AST *eval_expr(AST *expr, HTable *funcs);
static int max(int a, int b) { return a > b ? a : b; }

static char *add_num(const char *a, const char *b)
{
  return itoa(atoi(a) + atoi(b), calloc(max(strlen(a), strlen(b)) + 1, sizeof(char)), 10);
}
static char *sub_num(const char *a, const char *b)
{
  return itoa(atoi(a) - atoi(b), calloc(max(strlen(a), strlen(b)) + 2, sizeof(char)), 10);
}
static char *mul_num(const char *a, const char *b)
{
  return itoa(atoi(a) * atoi(b), calloc(strlen(a) + strlen(b) + 1, sizeof(char)), 10);
}
static char *div_num(const char *a, const char *b)
{
  return itoa(atoi(a) / atoi(b), calloc(strlen(a), sizeof(char)), 10);
}

static AST *inline_var(AST *expr, const char *name, const AST *value)
{
  if (!expr)
    return NULL;
  if (expr->type == NAME)
  {
    if (!strcmp(name, expr->value))
      return copy_AST(value);
    if (!strcmp(name, "let"))
    {
      // check redef
    }
  }
  if (expr->type == CONST_NUM || expr->type == CONST_STR) return copy_AST(expr);  
  AST *head, *tail;
  head = inline_var(CAR(expr), name, value);
  tail = inline_var(CDR(expr), name, value);
  D_AST(CAR(expr));
  D_AST(CDR(expr));
  CAR(expr) = head;
  CDR(expr) = tail;
  return copy_AST(expr);
}

static AST *eval_func_call(AST *expr, HTable *funcs)
{
  assert(CAR(expr)->type == NAME);

  STR_MATCH(GET_OP(expr))
  STR_CASE("quote")
  return copy_AST(SECOND(expr)); // block evaluation

  STR_CASE("+")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = add_num(a->value, b->value);
  AST *val = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);
  return val;

  STR_CASE("-")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = sub_num(a->value, b->value);
  AST *val = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);
  return val;

  STR_CASE("*")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = mul_num(a->value, b->value);
  AST *val = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);
  return val;

  STR_CASE("/")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = div_num(a->value, b->value);
  AST *val = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);
  return val;

  STR_CASE("car")
  return copy_AST(CAR(expr));

  STR_CASE("cdr")
  return copy_AST(CDR(expr));

  STR_CASE("cons")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  return N_AST(SEXP, 0, NULL, a, b);

  STR_CASE("cond")
  // COND

  STR_CASE("print")
  AST *value = eval_expr(SECOND(expr), funcs);
  D_AST(SECOND(expr));
  SECOND(expr) = value;
  const char *c = NULL;
  switch (value->type)
  {
  case CONST_NUM:
    printf("%s\n", value->value);
    break;
  case CONST_STR:
    c = value->value + 1;
    for (int escaped = 0; (*c != '"' || escaped) &&
                          c < value->value + value->value_len;
         c += (escaped ? 2 : 1), escaped = 0)
    {
      escaped = *c == '\\' && !escaped;
      putchar(escaped ? ESC2C(*(c + 1)) : *c);
    }
    break;
  default:
    print_AST(value, stdout);
    break;
  }
  STR_CASE("let")

  STR_DEFAULT
  AST *fun = NULL;
  if (!ht_get(funcs, GET_OP(expr), (void **)&fun))
  {
    // ERROR
    return NULL;
  }

  STR_MATCH_END

  return NULL;
}

static AST *eval_expr(AST *expr, HTable *funcs)
{
  AST *expr_copy = copy_AST(expr);
  if (expr_copy->type != SEXP)
    return expr_copy; // throw consts

  if (CAR(expr_copy)->type == SEXP)
  {
    AST *tmp = CAR(expr_copy);
    CAR(expr_copy) = eval_expr(tmp, funcs);
    D_AST(tmp);
  }
  AST *result = eval_func_call(expr_copy, funcs);
  D_AST(expr_copy);
  return result;
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
    ht_set(funcs, DEFUN_GET_ID(stmt), CDR(stmt));
    call_trace.depth--;
  }
  else
    D_AST(eval_expr(stmt, funcs));
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
  default:
    break;
  }
  return 0;
}
