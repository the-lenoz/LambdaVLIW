#include "../htable/htable.h"
#include "../parser/parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarBinding
{
  char *name;
  int32_t value;
  struct VarBinding *next;
} VarBinding;

typedef struct PhiBinding
{
  const char *name;
  int32_t value;
  struct PhiBinding *next;
} PhiBinding;

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

static void free_var_bindings(VarBinding *vars)
{
  while (vars)
  {
    VarBinding *next = vars->next;
    free(vars->name);
    free(vars);
    vars = next;
  }
}

static int get_var(VarBinding *vars, const char *name, int32_t *value)
{
  for (VarBinding *it = vars; it; it = it->next)
  {
    if (!strcmp(it->name, name))
    {
      *value = it->value;
      return 0;
    }
  }

  return fprintf(stderr, "Fatal: undefined variable '%s'\n", name), -1;
}

static int set_var(VarBinding **vars, const char *name, int32_t value)
{
  VarBinding *binding;

  if (!vars || !name)
    return -1;

  for (VarBinding *it = *vars; it; it = it->next)
  {
    if (!strcmp(it->name, name))
    {
      it->value = value;
      return 0;
    }
  }

  binding = malloc(sizeof(*binding));
  if (!binding)
    return -1;

  binding->name = strdup(name);
  if (!binding->name)
    return free(binding), -1;

  binding->value = value;
  binding->next = *vars;
  *vars = binding;
  return 0;
}

static int parse_function_definition(AST *func, const char **name, AST **params, AST **body, int *param_count)
{
  AST *func_name = func ? SECOND(func) : NULL;
  AST *cursor;

  if (!func || func->type != SEXP || !is_form(func, "func") || !func_name || !is_name(func_name))
    return fprintf(stderr, "Fatal: malformed function definition\n"), -1;

  cursor = CDR(CDR(func));
  if (name)
    *name = func_name->value;
  if (params)
    *params = cursor;
  if (param_count)
    *param_count = 0;

  while (cursor)
  {
    AST *item = CAR(cursor);
    if (!item)
      return fprintf(stderr, "Fatal: malformed function definition\n"), -1;
    if (item->type == NAME)
    {
      if (param_count)
        *param_count += 1;
      cursor = CDR(cursor);
      continue;
    }
    if (item->type == SEXP)
      break;
    return fprintf(stderr, "Fatal: malformed function definition\n"), -1;
  }

  if (body)
    *body = cursor;
  return 0;
}

static int eval_expr(AST *expr, VarBinding *vars, HTable *func_tab, int32_t *value);
static int interpret_function(AST *func, AST *arg_exprs, VarBinding *caller_vars, HTable *func_tab, int32_t *result);

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

static int eval_binary(AST *expr, VarBinding *vars, HTable *func_tab, int32_t *value)
{
  int32_t lhs;
  int32_t rhs;

  if (!expr || expr->type != SEXP || list_len(expr) != 3 || !is_name(FIRST(expr)))
    return fprintf(stderr, "Fatal: malformed expression\n"), -1;

  if (eval_expr(SECOND(expr), vars, func_tab, &lhs) < 0 || eval_expr(THIRD(expr), vars, func_tab, &rhs) < 0)
    return -1;

  return eval_binary_op(GET_OP(expr), lhs, rhs, value);
}

static int eval_call(AST *expr, VarBinding *vars, HTable *func_tab, int32_t *value)
{
  AST *args;
  AST *func = NULL;
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

    if (eval_expr(FIRST(args), vars, func_tab, &lhs) < 0 || eval_expr(SECOND(args), vars, func_tab, &rhs) < 0)
      return -1;

    return eval_binary_op(callee, lhs, rhs, value);
  }

  if (!ht_get(func_tab, callee, (void **)&func))
    return fprintf(stderr, "Fatal: unknown function '%s'\n", callee), -1;

  return interpret_function(func, args, vars, func_tab, value);
}

static int eval_expr(AST *expr, VarBinding *vars, HTable *func_tab, int32_t *value)
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
      return eval_call(expr, vars, func_tab, value);
    return eval_binary(expr, vars, func_tab, value);
  default:
    return fprintf(stderr, "Fatal: unexpected expression type\n"), -1;
  }
}

static int eval_phi(AST *expr, const char *prev_block, VarBinding *vars, HTable *func_tab, int32_t *value)
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
      return eval_expr(SECOND(source), vars, func_tab, value);
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

static int collect_phi_bindings(AST *instructions, const char *prev_block, VarBinding *vars, HTable *func_tab,
                                PhiBinding **bindings, AST **rest)
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

    if (eval_phi(THIRD(instr), prev_block, vars, func_tab, &binding->value) < 0)
      return free(binding), free_phi_bindings(head), -1;

    *tail = binding;
    tail = &binding->next;
    cursor = CDR(cursor);
  }

  *bindings = head;
  *rest = cursor;
  return 0;
}

static int apply_phi_bindings(VarBinding **vars, PhiBinding *bindings)
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

static int execute_block(AST *block, const char *prev_block, VarBinding **vars, HTable *func_tab,
                         const char **next_block, int *did_return, int32_t *returned_value)
{
  AST *instructions = CDR(block);
  AST *cursor = instructions;
  PhiBinding *bindings = NULL;

  *next_block = NULL;
  *did_return = 0;
  if (collect_phi_bindings(instructions, prev_block, *vars, func_tab, &bindings, &cursor) < 0)
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
      if (eval_expr(THIRD(instr), *vars, func_tab, &value) < 0)
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
      if (get_var(*vars, cond_var->value, &cond_value) < 0)
        return -1;
      *next_block = cond_value ? then_block->value : else_block->value;
      return 0;
    }

    if (is_form(instr, "return"))
    {
      if (list_len(instr) != 2)
        return fprintf(stderr, "Fatal: malformed return\n"), -1;
      if (eval_expr(SECOND(instr), *vars, func_tab, returned_value) < 0)
        return -1;
      *did_return = 1;
      return 0;
    }

    return fprintf(stderr, "Fatal: unsupported instruction '%s'\n", GET_OP(instr)), -1;
  }

  return 0;
}

static int interpret_cfg(AST *blocks, HTable *func_tab, VarBinding **vars, int32_t *result)
{
  HTable *block_tab = ht_init(0);
  AST *entry_block = NULL;
  AST *current_block = NULL;
  const char *prev_block = NULL;
  int status = -1;

  if (!blocks || blocks->type != SEXP)
    return ht_destroy(block_tab), fprintf(stderr, "Fatal: function body must be a list of basic blocks\n"), -1;

  if (register_blocks(blocks, block_tab, &entry_block) == 0)
  {
    current_block = entry_block;
    while (current_block)
    {
      const char *next_label = NULL;
      AST *next_block = NULL;
      int did_return = 0;
      int32_t returned_value = 0;

      if (execute_block(current_block, prev_block, vars, func_tab, &next_label, &did_return, &returned_value) < 0)
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

  ht_destroy(block_tab);
  return status;
}

static int interpret_function(AST *func, AST *arg_exprs, VarBinding *caller_vars, HTable *func_tab, int32_t *result)
{
  AST *params = NULL;
  AST *body = NULL;
  AST *param_cursor;
  AST *arg_cursor = arg_exprs;
  VarBinding *callee_vars = NULL;
  const char *func_name = NULL;
  int status;

  if (parse_function_definition(func, &func_name, &params, &body, NULL) < 0)
    return -1;

  for (param_cursor = params; param_cursor; param_cursor = CDR(param_cursor))
  {
    AST *param = CAR(param_cursor);
    int32_t arg_value;

    if (!param || param->type != NAME)
      break;
    if (!arg_cursor)
    {
      fprintf(stderr, "Fatal: call to '%s' has too few arguments\n", func_name);
      status = -1;
      goto cleanup;
    }
    if (eval_expr(CAR(arg_cursor), caller_vars, func_tab, &arg_value) < 0)
    {
      status = -1;
      goto cleanup;
    }
    if (set_var(&callee_vars, param->value, arg_value) < 0)
    {
      status = -1;
      goto cleanup;
    }
    arg_cursor = CDR(arg_cursor);
  }

  if (arg_cursor)
  {
    fprintf(stderr, "Fatal: call to '%s' has too many arguments\n", func_name);
    status = -1;
    goto cleanup;
  }
  if (!body)
  {
    fprintf(stderr, "Fatal: function '%s' has no body\n", func_name);
    status = -1;
    goto cleanup;
  }

  status = interpret_cfg(body, func_tab, &callee_vars, result);

cleanup:
  free_var_bindings(callee_vars);
  return status;
}

static int register_functions(AST *module_forms, HTable *func_tab, AST **main_func)
{
  for (AST *item = module_forms; item; item = CDR(item))
  {
    AST *func = CAR(item);
    AST *existing = NULL;
    const char *name = NULL;
    int param_count = 0;

    if (parse_function_definition(func, &name, NULL, NULL, &param_count) < 0)
      return -1;

    if (ht_get(func_tab, name, (void **)&existing))
      return fprintf(stderr, "Fatal: duplicate function '%s'\n", name), -1;

    if (ht_set(func_tab, name, func) < 0)
      return -1;

    if (!strcmp(name, "main"))
    {
      if (param_count != 0)
        return fprintf(stderr, "Fatal: function 'main' must not have arguments\n"), -1;
      *main_func = func;
    }
  }

  return 0;
}

static int interpret_module(AST *module_forms, int32_t *result)
{
  HTable *func_tab = ht_init(0);
  AST *main_func = NULL;
  int status;

  if (!module_forms || module_forms->type != SEXP)
    return ht_destroy(func_tab), fprintf(stderr, "Fatal: program must be a list of functions\n"), -1;

  status = register_functions(module_forms, func_tab, &main_func);
  if (status == 0)
  {
    if (!main_func)
    {
      fprintf(stderr, "Fatal: function 'main' not found\n");
      status = -1;
    }
    else
      status = interpret_function(main_func, NULL, NULL, func_tab, result);
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
  int status;
  FILE *fp;

  if (!filename)
    return -1;

  fp = fopen(filename, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", filename), -1;

  status = interpret_stream(fp);
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
