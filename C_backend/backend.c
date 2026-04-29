#include "../SSA/SSA.h"
#include "../parser/AST.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int is_c_name(const char *name)
{
  if (!name)
    return 0;
  if (isdigit(*name))
    return 0;
  for (const char *c = name; *c; ++c)
  {
    if (!(*c >= 'A' && *c <= 'Z' || *c >= 'a' && *c <= 'z' || *c >= '0' && *c <= '9' || *c == '_'))
      return 0;
  }
  return 1;
}

int is_bin_operator(SSAModule *module, SSAFuncName fn)
{
  if (!module || fn >= module->functions_count)
    return 0;
  const char *name = module->functions[fn].name;

  STR_MATCH(name)
  STR_CASE("+")
  return 1;
  STR_CASE("-")
  return 1;
  STR_CASE("*")
  return 1;
  STR_CASE("/")
  return 1;
  STR_CASE(">")
  return 1;
  STR_CASE("<")
  return 1;
  STR_CASE("=")
  return 1;
  STR_DEFAULT
  return 0;
  STR_MATCH_END
}

int dummy_do_phi(FILE *fp, SSAFunc *fn, SSABasicBlockName src_BB, SSABasicBlockName dst_BB, int indent)
{
  if (!fp || !fn || src_BB >= fn->basic_blocks_count || dst_BB >= fn->basic_blocks_count)
    return 0;

  int *used_vals_mask = calloc(fn->values_count, sizeof(int));
  for (SSAValName i = 0; i < fn->values_count; ++i)
  {
    if (fn->values[i].parent_name != dst_BB || fn->values[i].type != SSA_VALUE_PHI)
      continue;
    // Only phi values from dst_BB
    for (PhiList *option = fn->values[i].expr.phi.options; option; option = option->next)
    {
      if (option->pair.previous_block_name != src_BB ||
          option->pair.value_name == SSA_INVALID_VAL || option->pair.value_name == SSA_VAL_VOID)
        continue;
      for (int _ = 0; _ < indent; ++_)
        putc(' ', fp);
      fprintf(fp, "__swp_v%d = v%d;\n", i, option->pair.value_name);
      used_vals_mask[i] = 1;
      break;
    }
  }
  for (SSAValName i = 0; i < fn->values_count; ++i)
  {
    if (used_vals_mask[i])
    {
      for (int _ = 0; _ < indent; ++_)
        putc(' ', fp);
      fprintf(fp, "v%d = __swp_v%d;\n", i, i);
    }
  }
  return 1;
}

int dummy_SSA_module_to_C(SSAModule *module, FILE *fp)
{
  if (!module || !fp)
    return 0;

  fprintf(fp, "#include <stdio.h>\n\n\n");

  for (SSAFunc *fn = module->functions;
       fn < module->functions + module->functions_count; ++fn)
  {
    if (!is_c_name(fn->name)) // Skip functions with invalid for C names (e. g. arithmetic)
      continue;

    fprintf(fp, "int %s(", fn->name);
    for (int i = 0; i < fn->args_count; ++i)
    {
      fprintf(fp, "int v%d", i);
      if (i < fn->args_count - 1)
        fprintf(fp, ", ");
    }
    fprintf(fp, "){\n");
    for (SSAValName i = fn->args_count; i < fn->values_count; ++i)
      fprintf(fp, "  int v%d, __swp_v%d;\n", i, i);
    fprintf(fp, "  int __next_BB = %d;\n", fn->entry_block);
    fprintf(fp, "  while (1) {\n");
    fprintf(fp, "    switch (__next_BB) {\n");

    for (SSABasicBlockName i = 0; i < fn->basic_blocks_count; ++i)
    {
      fprintf(fp, "      case %d:\n", i);
      for (int j = 0; j < fn->values_count; ++j)
      {
        if (fn->values[j].parent_name != i)
          continue;
        switch (fn->values[j].type)
        {
        case SSA_VALUE_CONST:
          fprintf(fp, "        v%d = ", j);
          fprintf(fp, "%d;\n", fn->values[j].expr.cnst.value);
          break;
        case SSA_VALUE_CALL:
          fprintf(fp, "        v%d = ", j);
          if (is_bin_operator(module, fn->values[j].expr.call.calee_name))
          {
            if (!strcmp(module->functions[fn->values[j].expr.call.calee_name].name, "="))
              fprintf(fp, "v%d == v%d;\n",
                      ARG_FIRST(fn->values[j].expr.call.args), ARG_SECOND(fn->values[j].expr.call.args));
            else
              fprintf(fp, "v%d %s v%d;\n", ARG_FIRST(fn->values[j].expr.call.args),
                      module->functions[fn->values[j].expr.call.calee_name].name,
                      ARG_SECOND(fn->values[j].expr.call.args));
          }
          else
          {
            fprintf(fp, "%s(", module->functions[fn->values[j].expr.call.calee_name].name);
            for (ArgList *args = fn->values[j].expr.call.args; args; args = args->next)
            {
              fprintf(fp, "v%d", args->name);
              if (args->next)
                fprintf(fp, ", ");
            }
            fprintf(fp, ");\n");
          }
          break;
        default:
          break;
        }
      }
      switch (fn->basic_blocks[i].terminator.type)
      {
      case SSA_TERM_RETURN:
        fprintf(fp, "        return v%d;\n", fn->basic_blocks[i].terminator.ret_val);
        break;
      case SSA_TERM_NONE:
        break; // SMTH STRANGE
      case SSA_TERM_GOTO:
        dummy_do_phi(fp, fn, i, fn->basic_blocks[i].terminator.true_dst, 8);
        fprintf(fp, "        __next_BB = %d;\n", fn->basic_blocks[i].terminator.true_dst);
        break;
      case SSA_TERM_COND_GOTO:
        fprintf(fp, "        if (v%d) {\n", fn->basic_blocks[i].terminator.cond);
        dummy_do_phi(fp, fn, i, fn->basic_blocks[i].terminator.true_dst, 10);
        fprintf(fp, "          __next_BB = %d;\n      } else {\n", fn->basic_blocks[i].terminator.true_dst);
        dummy_do_phi(fp, fn, i, fn->basic_blocks[i].terminator.false_dst, 10);
        fprintf(fp, "          __next_BB = %d;\n      }\n", fn->basic_blocks[i].terminator.false_dst);
        break;
      }
      fprintf(fp, "        break;\n");
    }

    fprintf(fp, "    }\n  }\n  return 0;\n}\n\n");
  }
  return 1;
}
