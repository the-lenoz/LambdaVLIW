#include "../htable/htable.h"
#include "../parser/parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PhiBinding
{
  const char *name;
  int32_t value;
  struct PhiBinding *next;
} PhiBinding;

static size_t strhash(const void *k)
{
  const unsigned char *str = (const unsigned char *)k;
  size_t hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c;

  return hash;
}

static int str_eq(const void *a, const void *b) { return !strcmp(a, b); }

static void free_key_only(HTable *ht, void *k, void *v)
{
  (void)ht;
  (void)v;
  free(k);
}

static void free_key_and_value(HTable *ht, void *k, void *v)
{
  (void)ht;
  free(k);
  free(v);
}

static void *k_dup(const void *k)
{
  const char *src = (const char *)k;
  size_t n;
  char *copy;

  if (!src)
    return NULL;

  n = strlen(src);
  copy = malloc(n + 1);
  if (!copy)
    return NULL;

  memcpy(copy, src, n + 1);
  return copy;
}

static void *ptr_dup(const void *v) { return (void *)v; }

static void *int_dup(const void *v)
{
  int32_t *copy = malloc(sizeof(*copy));
  if (!copy)
    return NULL;
  *copy = *(const int32_t *)v;
  return copy;
}

static AST *list_nth(AST *list, int index)
{
  for (AST *item = list; item; item = CDR(item), --index)
    if (index == 1)
      return CAR(item);
  return NULL;
}

static int is_name(AST *node) { return node && node->type == NAME; }

static int is_form(AST *expr, const char *name)
{
  return expr && expr->type == SEXP && is_name(FIRST(expr)) && !strcmp(GET_OP(expr), name);
}

static int is_if_goto_form(AST *expr)
{
  return is_form(expr, "if-goto") || is_form(expr, "goto-if");
}

static int get_var(HTable *vars, const char *name, int32_t *value)
{
  int32_t *stored = NULL;
  if (!ht_get(vars, name, (void **)&stored))
    return fprintf(stderr, "Fatal: undefined variable '%s'\n", name), -1;
  *value = *stored;
  return 0;
}

static int set_var(HTable *vars, const char *name, int32_t value)
{
  return ht_set(vars, name, &value) < 0 ? -1 : 0;
}

static int eval_expr(AST *expr, HTable *vars, int32_t *value);

static int eval_binary_op(const char *op, int32_t lhs, int32_t rhs, int32_t *value)
{
  if (!op || !value)
    return -1;

  if (!strcmp(op, "+"))
    *value = lhs + rhs;
  else if (!strcmp(op, "-"))
    *value = lhs - rhs;
  else if (!strcmp(op, "*"))
    *value = lhs * rhs;
  else if (!strcmp(op, "/"))
  {
    if (!rhs)
      return fprintf(stderr, "Fatal: division by zero\n"), -1;
    *value = lhs / rhs;
  }
  else if (!strcmp(op, ">"))
    *value = lhs > rhs;
  else if (!strcmp(op, "<"))
    *value = lhs < rhs;
  else if (!strcmp(op, "==") || !strcmp(op, "="))
    *value = lhs == rhs;
  else if (!strcmp(op, ">="))
    *value = lhs >= rhs;
  else if (!strcmp(op, "<="))
    *value = lhs <= rhs;
  else
    return fprintf(stderr, "Fatal: unsupported operator '%s'\n", op), -1;

  return 0;
}

static int eval_binary(AST *expr, HTable *vars, int32_t *value)
{
  int32_t lhs;
  int32_t rhs;

  if (!expr || expr->type != SEXP || list_len(expr) != 3 || !is_name(FIRST(expr)))
    return fprintf(stderr, "Fatal: malformed expression\n"), -1;

  if (eval_expr(SECOND(expr), vars, &lhs) < 0 || eval_expr(THIRD(expr), vars, &rhs) < 0)
    return -1;

  return eval_binary_op(GET_OP(expr), lhs, rhs, value);
}

static int eval_call(AST *expr, HTable *vars, int32_t *value)
{
  AST *args;
  int32_t lhs;
  int32_t rhs;
  const char *callee;

  if (!is_form(expr, "call") || list_len(expr) < 2 || !is_name(SECOND(expr)))
    return fprintf(stderr, "Fatal: malformed call expression\n"), -1;

  callee = SECOND(expr)->value;
  args = CDR(CDR(expr));

  if (!strcmp(callee, "+") || !strcmp(callee, "-") || !strcmp(callee, "*") || !strcmp(callee, "/") ||
      !strcmp(callee, ">") || !strcmp(callee, "<") || !strcmp(callee, "=") || !strcmp(callee, "==") ||
      !strcmp(callee, ">=") || !strcmp(callee, "<="))
  {
    if (list_len(args) != 2)
      return fprintf(stderr, "Fatal: arithmetic call '%s' must have 2 args\n", callee), -1;

    if (eval_expr(FIRST(args), vars, &lhs) < 0 || eval_expr(SECOND(args), vars, &rhs) < 0)
      return -1;

    return eval_binary_op(callee, lhs, rhs, value);
  }

  return fprintf(stderr, "Fatal: unsupported call target '%s'\n", callee), -1;
}

static int eval_expr(AST *expr, HTable *vars, int32_t *value)
{
  if (!expr)
    return fprintf(stderr, "Fatal: unexpected nil expression\n"), -1;

  switch (expr->type)
  {
  case CONST_NUM:
    *value = (int32_t)strtol(expr->value, NULL, 10);
    return 0;
  case NAME:
    if (!strcmp(expr->value, "nil") || !strcmp(expr->value, "void"))
    {
      *value = 0;
      return 0;
    }
    return get_var(vars, expr->value, value);
  case SEXP:
    if (is_form(expr, "phi"))
      return fprintf(stderr, "Fatal: phi is only allowed in let instructions\n"), -1;
    if (is_form(expr, "call"))
      return eval_call(expr, vars, value);
    return eval_binary(expr, vars, value);
  default:
    return fprintf(stderr, "Fatal: unexpected expression type\n"), -1;
  }
}

static int eval_phi(AST *expr, const char *prev_block, HTable *vars, int32_t *value)
{
  if (!is_form(expr, "phi") || list_len(expr) < 2)
    return fprintf(stderr, "Fatal: malformed phi node\n"), -1;

  if (!prev_block)
    return fprintf(stderr, "Fatal: phi requires a predecessor block\n"), -1;

  for (AST *sources = CDR(expr); sources; sources = CDR(sources))
  {
    AST *source = CAR(sources);
    if (!source || source->type != SEXP || list_len(source) != 2 || !is_name(FIRST(source)))
      return fprintf(stderr, "Fatal: malformed phi input\n"), -1;

    if (!strcmp(FIRST(source)->value, prev_block))
      return eval_expr(SECOND(source), vars, value);
  }

  return fprintf(stderr, "Fatal: phi has no input for predecessor '%s'\n", prev_block), -1;
}

static void free_phi_bindings(PhiBinding *bindings)
{
  while (bindings)
  {
    PhiBinding *next = bindings->next;
    free(bindings);
    bindings = next;
  }
}

static int collect_phi_bindings(AST *instructions, const char *prev_block, HTable *vars, PhiBinding **bindings,
                                AST **rest)
{
  PhiBinding *head = NULL;
  PhiBinding **tail = &head;
  AST *cursor = instructions;

  while (cursor)
  {
    AST *instr = CAR(cursor);
    if (!is_form(instr, "let"))
      break;
    if (list_len(instr) != 3 || !is_name(SECOND(instr)))
      return free_phi_bindings(head), fprintf(stderr, "Fatal: malformed let instruction\n"), -1;
    if (!is_form(THIRD(instr), "phi"))
      break;

    PhiBinding *binding = malloc(sizeof(*binding));
    if (!binding)
      return free_phi_bindings(head), -1;
    binding->name = SECOND(instr)->value;
    binding->next = NULL;

    if (eval_phi(THIRD(instr), prev_block, vars, &binding->value) < 0)
      return free(binding), free_phi_bindings(head), -1;

    *tail = binding;
    tail = &binding->next;
    cursor = CDR(cursor);
  }

  *bindings = head;
  *rest = cursor;
  return 0;
}

static int apply_phi_bindings(HTable *vars, PhiBinding *bindings)
{
  for (PhiBinding *binding = bindings; binding; binding = binding->next)
    if (set_var(vars, binding->name, binding->value) < 0)
      return -1;
  return 0;
}

static int register_blocks(AST *blocks, HTable *block_tab, AST **entry_block)
{
  for (AST *item = blocks; item; item = CDR(item))
  {
    AST *block = CAR(item);
    AST *label = block ? FIRST(block) : NULL;
    AST *existing = NULL;

    if (!block || block->type != SEXP || !is_name(label))
      return fprintf(stderr, "Fatal: malformed basic block\n"), -1;
    if (ht_get(block_tab, label->value, (void **)&existing))
      return fprintf(stderr, "Fatal: duplicate basic block '%s'\n", label->value), -1;

    if (!*entry_block)
      *entry_block = block;
    if (ht_set(block_tab, label->value, block) < 0)
      return -1;
  }
  return 0;
}

static int execute_block(AST *block, const char *prev_block, HTable *vars, const char **next_block, int *did_return,
                         int32_t *returned_value)
{
  AST *instructions = CDR(block);
  AST *cursor = instructions;
  PhiBinding *bindings = NULL;

  *next_block = NULL;
  *did_return = 0;
  if (collect_phi_bindings(instructions, prev_block, vars, &bindings, &cursor) < 0)
    return -1;
  if (apply_phi_bindings(vars, bindings) < 0)
    return free_phi_bindings(bindings), -1;
  free_phi_bindings(bindings);

  while (cursor)
  {
    AST *instr = CAR(cursor);
    if (!instr || instr->type != SEXP || !is_name(FIRST(instr)))
      return fprintf(stderr, "Fatal: malformed instruction\n"), -1;

    if (is_form(instr, "let"))
    {
      int32_t value;
      if (list_len(instr) != 3 || !is_name(SECOND(instr)))
        return fprintf(stderr, "Fatal: malformed let instruction\n"), -1;
      if (is_form(THIRD(instr), "phi"))
        return fprintf(stderr, "Fatal: phi nodes must be at the start of a block\n"), -1;
      if (eval_expr(THIRD(instr), vars, &value) < 0)
        return -1;
      if (set_var(vars, SECOND(instr)->value, value) < 0)
        return -1;
      cursor = CDR(cursor);
      continue;
    }

    if (is_form(instr, "goto"))
    {
      AST *target = SECOND(instr);
      if (list_len(instr) != 2 || !is_name(target))
        return fprintf(stderr, "Fatal: malformed goto\n"), -1;
      *next_block = target->value;
      return 0;
    }

    if (is_if_goto_form(instr))
    {
      AST *cond_var = SECOND(instr);
      AST *then_block = THIRD(instr);
      AST *else_kw = list_nth(instr, 4);
      AST *else_block = list_nth(instr, 5);
      int32_t cond_value;

      if (list_len(instr) != 5 || !is_name(cond_var) || !is_name(then_block) || !is_name(else_kw) || !is_name(else_block) ||
          strcmp(else_kw->value, "else"))
        return fprintf(stderr, "Fatal: malformed if-goto\n"), -1;
      if (get_var(vars, cond_var->value, &cond_value) < 0)
        return -1;
      *next_block = cond_value ? then_block->value : else_block->value;
      return 0;
    }

    if (is_form(instr, "return"))
    {
      if (list_len(instr) != 2)
        return fprintf(stderr, "Fatal: malformed return\n"), -1;
      if (eval_expr(SECOND(instr), vars, returned_value) < 0)
        return -1;
      *did_return = 1;
      return 0;
    }

    return fprintf(stderr, "Fatal: unsupported instruction '%s'\n", GET_OP(instr)), -1;
  }

  return 0;
}

static int interpret_cfg(AST *blocks, int32_t *result)
{
  HTable *block_tab = ht_init(strhash, str_eq, k_dup, ptr_dup, free_key_only, 0);
  HTable *vars = ht_init(strhash, str_eq, k_dup, int_dup, free_key_and_value, 0);
  AST *entry_block = NULL;
  AST *current_block = NULL;
  const char *prev_block = NULL;
  int status = -1;

  if (!blocks || blocks->type != SEXP)
    return fprintf(stderr, "Fatal: function body must be a list of basic blocks\n"), -1;

  if (register_blocks(blocks, block_tab, &entry_block) == 0)
  {
    current_block = entry_block;
    while (current_block)
    {
      const char *next_label = NULL;
      AST *next_block = NULL;
      int did_return = 0;
      int32_t returned_value = 0;

      if (execute_block(current_block, prev_block, vars, &next_label, &did_return, &returned_value) < 0)
        break;
      if (did_return)
      {
        *result = returned_value;
        status = 0;
        break;
      }
      if (!next_label)
      {
        fprintf(stderr, "Fatal: basic block '%s' has no terminator\n", FIRST(current_block)->value);
        break;
      }
      if (!ht_get(block_tab, next_label, (void **)&next_block))
      {
        fprintf(stderr, "Fatal: unknown basic block '%s'\n", next_label);
        break;
      }

      prev_block = FIRST(current_block)->value;
      current_block = next_block;
    }
  }

  ht_destroy(vars);
  ht_destroy(block_tab);
  return status;
}

static int register_functions(AST *module_forms, HTable *func_tab, AST **main_func)
{
  for (AST *item = module_forms; item; item = CDR(item))
  {
    AST *func = CAR(item);
    AST *name = func ? SECOND(func) : NULL;
    AST *existing = NULL;

    if (!func || func->type != SEXP || !is_form(func, "func") || !name || !is_name(name))
      return fprintf(stderr, "Fatal: malformed function definition\n"), -1;

    if (ht_get(func_tab, name->value, (void **)&existing))
      return fprintf(stderr, "Fatal: duplicate function '%s'\n", name->value), -1;

    if (ht_set(func_tab, name->value, func) < 0)
      return -1;

    if (!strcmp(name->value, "main"))
      *main_func = func;
  }

  return 0;
}

static int interpret_module(AST *module_forms, int32_t *result)
{
  HTable *func_tab = ht_init(strhash, str_eq, k_dup, ptr_dup, free_key_only, 0);
  AST *main_func = NULL;
  int status;

  if (!module_forms || module_forms->type != SEXP)
    return fprintf(stderr, "Fatal: program must be a list of functions\n"), -1;

  status = register_functions(module_forms, func_tab, &main_func);
  if (status == 0)
  {
    if (!main_func)
    {
      fprintf(stderr, "Fatal: function 'main' not found\n");
      status = -1;
    }
    else
      status = interpret_cfg(CDR(CDR(main_func)), result);
  }

  ht_destroy(func_tab);
  return status;
}

int interpret(AST *program)
{
  int32_t value;
  int status;

  if (!program || !program->lchild || !program->lchild->lchild)
    return -1;

  status = interpret_module(program->lchild->lchild, &value);
  if (!status)
    printf("Result: %d\n", value);
  D_AST(program);
  return status;
}

int interpret_stream(FILE *fp)
{
  AST *program = parse_stream(fp, 0);

  if (!program || !program->lchild)
    return fprintf(stderr, "Invalid syntax\n"), -1;

  return interpret(program);
}

int process_file(const char *filename)
{
  if (!filename)
    return -1;

  FILE *fp = fopen(filename, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", filename), -1;

  int status = interpret_stream(fp);
  fclose(fp);
  return status;
}

int main(int argc, char **argv)
{
  if (argc > 2)
    return fprintf(stderr, "Usage: %s source.lisp\n", argv[0]), -1;

  if (argc == 1)
    return interpret_stream(stdin);
  return process_file(argv[1]);
}
