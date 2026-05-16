#include "../C_backend/backend.h"
#include "../L_defun/frontend.h"
#ifdef DEBUG
#include "../SSA/SSA_dump_graphviz.h"
#include <stdlib.h>
#endif
#include "../parser/parser.h"
#include "../optimizer/optimizer.h"
#include <stdio.h>

int compile(const char *source_path, const char *out_path)
{
  FILE *src_fp = fopen(source_path, "r");
  if (!src_fp)
    return fprintf(stderr, "Fatal: can't open file '%s'.\n", source_path), -1;

  FILE *out_fp = fopen(out_path, "w");
  if (!out_fp)
    return fprintf(stderr, "Fatal: can't open file '%s'.\n", out_path), -1;

  AST *program = parse_stream(src_fp, 1);
  if (!program)
    return fprintf(stderr, "Fatal: syntax error.\n"), fclose(out_fp), -1;

  SSAModule *module = build_program(program);
  D_AST(program);
  if (!module)
    return fprintf(stderr, "Fatal: semanthic error.\n"), fclose(out_fp), -1;

  optimize_module(module);
  
#ifdef DEBUG
  char *debug_path = calloc(strlen(out_path) + 5, sizeof(char));
  strcpy(debug_path, out_path);
  strcat(debug_path, ".dot");

  FILE *debug_fp = fopen(debug_path, "w");
  if (debug_fp)
  {
    SSA_dump_module_graphviz(module, debug_fp);
    fclose(debug_fp);
  }
  free(debug_path);
#endif
  
  if (!dummy_SSA_module_to_C(module, out_fp))
  {
    fclose(out_fp);
    fprintf(stderr, "Fatal: invalid SSA produced.\n");
    return -1;
  }
  destroy_module(module);

  fclose(out_fp);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc < 3)
    return fprintf(stderr, "Usage: %s input.lisp output.c\n", argv[0]);

  return compile(argv[1], argv[2]);
}
