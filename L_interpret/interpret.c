#include "interpret.h"
#include "../htable/htable.h"
#include "../htable/hash_helper.h"
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

#define OPT(x) STR_CASE(x) return x;
static char *op_copy(const char *op)
{
  STR_MATCH(op)
  OPT("quote")
  OPT("+")
  OPT("-")
  OPT("*")
  OPT("/")
  OPT("<")
  OPT(">")
  OPT("==")
  OPT("<=")
  OPT(">=")
  OPT("and")
  OPT("or")
  OPT("not")
  OPT("car")
  OPT("cdr")
  OPT("cons")
  OPT("cond")
  OPT("print")
  OPT("let")
  OPT("defun")
  STR_DEFAULT
  return "<defined>";
  STR_MATCH_END
}
#undef OPT

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

static const char *lt_num(const char *a, const char *b)
{
  return atoi(a) < atoi(b) ? "1" : "0";
}

static const char *gt_num(const char *a, const char *b)
{
  return atoi(a) > atoi(b) ? "1" : "0";
}

static const char *eq_num(const char *a, const char *b)
{
  return atoi(a) == atoi(b) ? "1" : "0";
}

static const char *not_bool(const char *x)
{
  return *x == '0' ? "1" : !strcmp(x, "nil") ? "1"
                                             : "0";
}

static const char *and_bool(const char *a, const char *b)
{
  return atoi(not_bool(not_bool(a))) && atoi(not_bool(not_bool(b))) ? "1" : "0";
}

static const char *or_bool(const char *a, const char *b)
{
  return atoi(not_bool(not_bool(a))) || atoi(not_bool(not_bool(b))) ? "1" : "0";
}

static int is_redefined(const char *name, AST *let)
{
  for (AST *list = SECOND(let); list; list = CDR(list))
  {
    if (!strcmp(name, GET_OP(CAR(list))))
      return 1;
  }
  return 0;
}

static int is_true(AST *value)
{
  switch (value->type)
  {
  case CONST_NUM:
    return *value->value != '0';
  case CONST_STR:
    return value->value_len != 0;
  default:
    return 1;
  }
}

static AST *inline_var(AST *expr, const char *name, const AST *value)
{
  AST *head, *tail;
  if (!expr)
    return NULL;
  if (expr->type == NAME && !strcmp(name, expr->value))
  {
    return copy_AST(value);
  }
  if (expr->type == CONST_NUM || expr->type == CONST_STR)
    return copy_AST(expr);
  if (expr->type == SEXP && !strcmp("let", GET_OP(expr)) && is_redefined(name, expr))
    return copy_AST(expr);
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

  call_trace.trace[call_trace.depth] = (call_t){call_trace.depth, op_copy(GET_OP(expr)), expr};
  call_trace.depth++;
  AST *eval_result = NULL;

  STR_MATCH(GET_OP(expr))
  STR_CASE("quote")
  eval_result = copy_AST(SECOND(expr)); // block evaluation

  STR_CASE("+")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = add_num(a->value, b->value);
  eval_result = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);

  STR_CASE("-")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = sub_num(a->value, b->value);
  eval_result = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);

  STR_CASE("*")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = mul_num(a->value, b->value);
  eval_result = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);

  STR_CASE("/")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  char *result = div_num(a->value, b->value);
  eval_result = N_AST(CONST_NUM, strlen(result), result, NULL, NULL);
  free(result);

  STR_CASE(">")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  eval_result = N_AST(CONST_NUM, 1, gt_num(a->value, b->value), NULL, NULL);
  STR_CASE("<")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  eval_result = N_AST(CONST_NUM, 1, lt_num(a->value, b->value), NULL, NULL);
  STR_CASE("==")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  eval_result = N_AST(CONST_NUM, 1, eq_num(a->value, b->value), NULL, NULL);
  STR_CASE("and")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  eval_result = N_AST(CONST_NUM, 1, and_bool(a->value, b->value), NULL, NULL);
  STR_CASE("or")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  D_AST(SECOND(expr));
  D_AST(THIRD(expr));
  SECOND(expr) = a, THIRD(expr) = b;
  eval_result = N_AST(CONST_NUM, 1, or_bool(a->value, b->value), NULL, NULL);
  STR_CASE("not")
  AST *a = eval_expr(SECOND(expr), funcs);
  D_AST(SECOND(expr));
  SECOND(expr) = a;
  eval_result = N_AST(CONST_NUM, 1, not_bool(a->value), NULL, NULL);

  STR_CASE("car")
  eval_result = copy_AST(CAR((SECOND(expr))));

  STR_CASE("cdr")
  eval_result = copy_AST(CDR(SECOND(expr)));

  STR_CASE("cons")
  AST *a = eval_expr(SECOND(expr), funcs);
  AST *b = eval_expr(THIRD(expr), funcs);
  eval_result = N_AST(SEXP, 0, NULL, a, b);

  STR_CASE("cond")
  AST *tmp;
  for (AST *case_list = CDR(expr); case_list; case_list = CDR(case_list))
  {
    tmp = FIRST(CAR(case_list));
    FIRST(CAR(case_list)) = eval_expr(tmp, funcs);
    D_AST(tmp);
    if (is_true(FIRST(CAR(case_list))))
    {
      eval_result = eval_expr(SECOND(CAR(case_list)), funcs);
      break;
    }
  }

  STR_CASE("print")
  AST *value = eval_expr(SECOND(expr), funcs);
  D_AST(SECOND(expr));
  SECOND(expr) = value;
  if (!value)
  {
    printf("nil\n");
  }
  else
  {
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
  }
  STR_CASE("let")
  AST *var_list = SECOND(expr);
  AST *result = THIRD(expr);
  AST *tmp = result;
  int is_free = 0;
  while (var_list)
  {
    tmp = result;
    result = inline_var(result, GET_OP(FIRST(var_list)), SECOND(FIRST(var_list)));
    if (is_free)
      D_AST(tmp);
    is_free = 1;
    var_list = CDR(var_list);
  }
  eval_result = eval_expr(result, funcs);
  if (result != THIRD(expr))
    D_AST(result);

  STR_DEFAULT
  AST *fun = NULL;
  if (ht_get(funcs, GET_OP(expr), (void **)&fun))
  {
    AST *args = CDR(expr);
    AST *arg_names = SECOND(fun);
    if (list_len(args) == list_len(arg_names))
    {
      eval_result = CONS(N_AST(NAME, 3, "let", NULL, NULL),
                         CONS(NULL,
                              CONS(copy_AST(THIRD(fun)),
                                   NULL)));
      AST **dst = &SECOND(eval_result);
      while (args)
      {
        *dst = CONS(CONS(copy_AST(CAR(arg_names)), CONS(CAR(args), NULL)), NULL);
        dst = &CDR(*dst);
        CAR(args) = NULL;
        args = CDR(args);
        arg_names = CDR(arg_names);
      }
      AST *tmp = eval_result;
      eval_result = eval_expr(eval_result, funcs);
      D_AST(tmp);
    }
    else
      err = (err_t){FATAL, "Call args num mismatch"};
  }
  else
    err = (err_t){FATAL, "Call to undefined function"};
  STR_MATCH_END

  if (err.kind == OK)
    call_trace.depth--;

  return eval_result;
}

static AST *eval_expr(AST *expr, HTable *funcs)
{
  if (!expr)
    return NULL;
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
  return err.kind == OK ? 0 : -1;
}

int interpret_program(AST *program, AST *arg_data)
{
  HTable *functions = ht_init(str_hash, str_eq, str_k_dup, int_v_dup, free_str, 0);
  err = (err_t){OK, NULL};
  int status = 0;
  for (AST *block = program->lchild; block && err.kind == OK && status != -1; block = block->rchild)
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
