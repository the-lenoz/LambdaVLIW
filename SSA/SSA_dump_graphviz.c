#include "SSA_dump_graphviz.h"

static const SSAFunc *gv_get_func(const SSAModule *module, SSAFuncName func)
{
  if (!module || func >= module->functions_count)
    return NULL;
  return &module->functions[func];
}

static int gv_print_dot_escaped(FILE *out_fp, const char *text)
{
  if (!out_fp || !text)
    return -1;

  for (const char *c = text; *c; ++c)
  {
    if (*c == '\\' || *c == '"')
    {
      if (fputc('\\', out_fp) == EOF || fputc(*c, out_fp) == EOF)
        return -1;
    }
    else if (*c == '\n' || *c == '\r')
    {
      if (fputs("\\n", out_fp) == EOF)
        return -1;
    }
    else
    {
      if (fputc(*c, out_fp) == EOF)
        return -1;
    }
  }

  return 0;
}

static int gv_print_html_escaped(FILE *out_fp, const char *text)
{
  if (!out_fp || !text)
    return -1;

  for (const char *c = text; *c; ++c)
  {
    if (*c == '&')
    {
      if (fputs("&amp;", out_fp) == EOF)
        return -1;
    }
    else if (*c == '<')
    {
      if (fputs("&lt;", out_fp) == EOF)
        return -1;
    }
    else if (*c == '>')
    {
      if (fputs("&gt;", out_fp) == EOF)
        return -1;
    }
    else if (*c == '"')
    {
      if (fputs("&quot;", out_fp) == EOF)
        return -1;
    }
    else if (*c == '\n' || *c == '\r')
    {
      if (fputs("&#10;", out_fp) == EOF)
        return -1;
    }
    else
    {
      if (fputc(*c, out_fp) == EOF)
        return -1;
    }
  }

  return 0;
}

static int gv_begin_line_row(FILE *out_fp)
{
  return fputs("        <TR><TD ALIGN=\"LEFT\"><FONT FACE=\"Menlo\">", out_fp) == EOF ? -1 : 0;
}

static int gv_end_line_row(FILE *out_fp)
{
  return fputs("</FONT></TD></TR>\n", out_fp) == EOF ? -1 : 0;
}

static int gv_print_val_ref(FILE *out_fp, SSAValName name)
{
  if (name == SSA_VAL_VOID)
    return fputs("nil", out_fp) == EOF ? -1 : 0;
  return fprintf(out_fp, "v%u", name) < 0 ? -1 : 0;
}

static int gv_print_arg_list(FILE *out_fp, const ArgList *args)
{
  int printed_any = 0;

  if (fputc('(', out_fp) == EOF)
    return -1;

  for (const ArgList *it = args; it; it = it->next)
  {
    if (it->name == SSA_INVALID_VAL)
      return -1;

    if (printed_any && fputs(", ", out_fp) == EOF)
      return -1;

    if (gv_print_val_ref(out_fp, it->name) < 0)
      return -1;

    printed_any = 1;
  }

  return fputc(')', out_fp) == EOF ? -1 : 0;
}

static int gv_print_phi_options(FILE *out_fp, const PhiList *options)
{
  int printed_any = 0;
  
  if (fputc('(', out_fp) == EOF)
    return -1;

  for (const PhiList *it = options; it; it = it->next)
  {
    if (it->pair.previous_block_name == SSA_INVALID_BB || it->pair.value_name == SSA_INVALID_VAL)
      return -1;

    if (printed_any && fputs(", ", out_fp) == EOF)
      return -1;

    if (fprintf(out_fp, "bb%u:", it->pair.previous_block_name) < 0 || gv_print_val_ref(out_fp, it->pair.value_name) < 0)
      return -1;

    printed_any = 1;
  }

  if (!printed_any)
  {
    if (fputs("<empty>", out_fp) == EOF)
      return -1;
  }

  return fputc(')', out_fp) == EOF ? -1 : 0;
}

static int gv_emit_value_row(FILE *out_fp, const SSAModule *module, const SSAFunc *func, SSAValName value_name)
{
  const SSAValue *value;

  if (!module || !func || value_name >= func->values_count)
    return -1;

  value = &func->values[value_name];
  if (gv_begin_line_row(out_fp) < 0)
    return -1;

  if (fprintf(out_fp, "v%u = ", value_name) < 0)
    return -1;

  if (value->type == SSA_VALUE_PHI)
  {
    if (fputs("phi", out_fp) == EOF || gv_print_phi_options(out_fp, value->expr.phi.options) < 0)
      return -1;
  }
  else if (value->type == SSA_VALUE_CONST)
  {
    if (fprintf(out_fp, "const %d", value->expr.cnst.value) < 0)
      return -1;
  }
  else if (value->type == SSA_VALUE_CALL)
  {
    const SSAFunc *callee = gv_get_func(module, value->expr.call.calee_name);

    if (!callee || !callee->name)
      return -1;

    if (fputs("call ", out_fp) == EOF || gv_print_html_escaped(out_fp, callee->name) < 0 ||
        gv_print_arg_list(out_fp, value->expr.call.args) < 0)
      return -1;

    if (value->is_const && fputs(" [const]", out_fp) == EOF)
      return -1;
  }
  else
    return -1;

  return gv_end_line_row(out_fp);
}

static int gv_emit_block_values(FILE *out_fp, const SSAModule *module, const SSAFunc *func, SSABasicBlockName bb,
                                SSAValueType type)
{
  for (SSAValName v = 0; v < func->values_count; ++v)
  {
    const SSAValue *value = &func->values[v];

    if (value->parent_name != bb || value->type != type)
      continue;

    if (gv_emit_value_row(out_fp, module, func, v) < 0)
      return -1;
  }

  return 0;
}

static int gv_emit_terminator_row(FILE *out_fp, const SSABlockTerminator *term)
{
  if (!term || gv_begin_line_row(out_fp) < 0)
    return -1;

  switch (term->type)
  {
  case SSA_TERM_NONE:
    if (fputs("<no terminator>", out_fp) == EOF)
      return -1;
    break;
  case SSA_TERM_GOTO:
    if (term->true_dst == SSA_INVALID_BB || fprintf(out_fp, "goto bb%u", term->true_dst) < 0)
      return -1;
    break;
  case SSA_TERM_COND_GOTO:
    if (term->cond == SSA_INVALID_VAL || term->true_dst == SSA_INVALID_BB || term->false_dst == SSA_INVALID_BB ||
        fprintf(out_fp, "if v%u then bb%u else bb%u", term->cond, term->true_dst, term->false_dst) < 0)
      return -1;
    break;
  case SSA_TERM_RETURN:
    if (term->ret_val == SSA_INVALID_VAL || fputs("return ", out_fp) == EOF || gv_print_val_ref(out_fp, term->ret_val) < 0)
      return -1;
    break;
  default:
    return -1;
  }

  return gv_end_line_row(out_fp);
}

static int gv_emit_block_node(FILE *out_fp, const SSAModule *module, const SSAFunc *func, SSAFuncName fn,
                              SSABasicBlockName bb)
{
  const SSABasicBlock *block;
  const char *header_bg = "#eceff1";
  const char *body_bg = "#f8fafb";

  if (!module || !func || bb >= func->basic_blocks_count)
    return -1;

  block = &func->basic_blocks[bb];
  if (bb == func->entry_block)
  {
    header_bg = "#c8e6c9";
    body_bg = "#edf7ee";
  }
  else if (bb == func->exit_block)
  {
    header_bg = "#ffe0b2";
    body_bg = "#fff8ef";
  }

  if (fprintf(out_fp, "    f%u_bb%u [shape=plain, margin=0, label=<\n", fn, bb) < 0 ||
      fprintf(out_fp, "      <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"6\" COLOR=\"#455a64\" BGCOLOR=\"%s\">\n", body_bg) < 0 ||
      fprintf(out_fp, "        <TR><TD ALIGN=\"LEFT\" BGCOLOR=\"%s\"><B>bb%u", header_bg, bb) < 0)
    return -1;

  if (bb == func->entry_block && fputs(" [entry]", out_fp) == EOF)
    return -1;
  if (bb == func->exit_block && fputs(" [exit]", out_fp) == EOF)
    return -1;

  if (fputs("</B></TD></TR>\n", out_fp) == EOF)
    return -1;

  if (gv_emit_block_values(out_fp, module, func, bb, SSA_VALUE_PHI) < 0)
    return -1;
  if (gv_emit_block_values(out_fp, module, func, bb, SSA_VALUE_CONST) < 0)
    return -1;
  if (gv_emit_block_values(out_fp, module, func, bb, SSA_VALUE_CALL) < 0)
    return -1;
  if (gv_emit_terminator_row(out_fp, &block->terminator) < 0)
    return -1;

  return fputs("      </TABLE>\n    >];\n", out_fp) == EOF ? -1 : 0;
}

static int gv_emit_function_cluster(FILE *out_fp, const SSAModule *module, SSAFuncName fn)
{
  const SSAFunc *func = gv_get_func(module, fn);
  int has_return = 0;

  if (!func || !func->name)
    return -1;

  if (fprintf(out_fp, "  subgraph cluster_f%u {\n", fn) < 0 ||
      fputs("    color=\"#90a4ae\"; style=\"rounded,filled\"; fillcolor=\"#f3f7fa\"; penwidth=1.4;\n", out_fp) == EOF ||
      fputs("    margin=16;\n", out_fp) == EOF ||
      fputs("    labelloc=t; label=\"", out_fp) == EOF ||
      gv_print_dot_escaped(out_fp, func->name) < 0 ||
      fputs("\";\n", out_fp) == EOF)
    return -1;

  if (fprintf(out_fp, "    f%u_order [shape=point, style=invis, width=0, height=0, label=\"\"];\n", fn) < 0)
    return -1;

  if (func->entry_block != SSA_INVALID_BB)
  {
    if (fprintf(out_fp, "    f%u_entry [shape=circle, fixedsize=true, width=0.34, label=\"\", style=filled, fillcolor=\"#81c784\", color=\"#2e7d32\"];\n", fn) < 0)
      return -1;
  }

  for (SSABasicBlockName bb = 0; bb < func->basic_blocks_count; ++bb)
    if (gv_emit_block_node(out_fp, module, func, fn, bb) < 0)
      return -1;

  if (func->entry_block != SSA_INVALID_BB)
    if (fprintf(out_fp, "    f%u_entry -> f%u_bb%u [label=\"entry\", color=\"#2e7d32\", penwidth=1.4];\n", fn, fn,
                func->entry_block) < 0)
      return -1;

  for (SSABasicBlockName bb = 0; bb < func->basic_blocks_count; ++bb)
  {
    const SSABlockTerminator *term = &func->basic_blocks[bb].terminator;

    if (term->type == SSA_TERM_GOTO)
    {
      if (term->true_dst == SSA_INVALID_BB ||
          fprintf(out_fp, "    f%u_bb%u -> f%u_bb%u [label=\"goto\", color=\"#1565c0\"];\n", fn, bb, fn,
                  term->true_dst) < 0)
        return -1;
    }
    else if (term->type == SSA_TERM_COND_GOTO)
    {
      if (term->cond == SSA_INVALID_VAL || term->true_dst == SSA_INVALID_BB || term->false_dst == SSA_INVALID_BB)
        return -1;

      if (fprintf(out_fp, "    f%u_bb%u -> f%u_bb%u [label=\"T\", color=\"#2e7d32\", penwidth=1.3];\n", fn, bb,
                  fn, term->true_dst) < 0 ||
          fprintf(out_fp, "    f%u_bb%u -> f%u_bb%u [label=\"F\", color=\"#c62828\", penwidth=1.3];\n", fn, bb,
                  fn, term->false_dst) < 0)
        return -1;
    }
    else if (term->type == SSA_TERM_RETURN)
      has_return = 1;
  }

  if (has_return)
  {
    if (fprintf(out_fp, "    f%u_ret [shape=doublecircle, label=\"return\", color=\"#6a1b9a\", penwidth=1.4];\n", fn) < 0)
      return -1;

    for (SSABasicBlockName bb = 0; bb < func->basic_blocks_count; ++bb)
    {
      const SSABlockTerminator *term = &func->basic_blocks[bb].terminator;

      if (term->type != SSA_TERM_RETURN)
        continue;

      if (term->ret_val == SSA_INVALID_VAL ||
          fprintf(out_fp, "    f%u_bb%u -> f%u_ret [style=dashed, color=\"#6a1b9a\", label=\"", fn, bb, fn) < 0 ||
          gv_print_val_ref(out_fp, term->ret_val) < 0 || fprintf(out_fp, "\"];\n") < 0)
        return -1;
    }
  }

  return fputs("  }\n", out_fp) == EOF ? -1 : 0;
}

int SSA_dump_module_graphviz(const SSAModule *module, FILE *out_fp)
{
  if (!module || !out_fp)
    return -1;

  if (fputs("digraph SSA {\n", out_fp) == EOF ||
      fputs("  rankdir=TB;\n", out_fp) == EOF ||
      fputs("  newrank=true;\n", out_fp) == EOF ||
      fputs("  ranksep=0.9;\n", out_fp) == EOF ||
      fputs("  nodesep=0.35;\n", out_fp) == EOF ||
      fputs("  splines=true;\n", out_fp) == EOF ||
      fputs("  fontname=\"Helvetica\";\n", out_fp) == EOF ||
      fputs("  node [fontname=\"Menlo\"];\n", out_fp) == EOF ||
      fputs("  edge [fontname=\"Menlo\"];\n", out_fp) == EOF)
    return -1;

  for (SSAFuncName fn = 0; fn < module->functions_count; ++fn)
    if (gv_emit_function_cluster(out_fp, module, fn) < 0)
      return -1;

  for (SSAFuncName fn = 1; fn < module->functions_count; ++fn)
    if (fprintf(out_fp, "  f%u_order -> f%u_order [style=invis, weight=200, minlen=2];\n", fn - 1, fn) < 0)
      return -1;

  return fputs("}\n", out_fp) == EOF ? -1 : 0;
}

int SSA_dump_func_graphviz(const SSAModule *module, SSAFuncName func, FILE *out_fp)
{
  if (!module || !out_fp || !gv_get_func(module, func))
    return -1;

  if (fputs("digraph SSA {\n", out_fp) == EOF ||
      fputs("  rankdir=TB;\n", out_fp) == EOF ||
      fputs("  newrank=true;\n", out_fp) == EOF ||
      fputs("  ranksep=0.9;\n", out_fp) == EOF ||
      fputs("  nodesep=0.35;\n", out_fp) == EOF ||
      fputs("  splines=true;\n", out_fp) == EOF ||
      fputs("  fontname=\"Helvetica\";\n", out_fp) == EOF ||
      fputs("  node [fontname=\"Menlo\"];\n", out_fp) == EOF ||
      fputs("  edge [fontname=\"Menlo\"];\n", out_fp) == EOF)
    return -1;

  if (gv_emit_function_cluster(out_fp, module, func) < 0)
    return -1;

  return fputs("}\n", out_fp) == EOF ? -1 : 0;
}
