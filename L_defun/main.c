#include "frontend.h"
#include "../parser/parser.h"
#include "../SSA/SSA_to_L_tri_call.h"

#ifndef NDEBUG
#include "../SSA/SSA_dump_graphviz.h"
#endif 

int compile_file(const char *path, const char *out_path)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", path), -1;

  AST *program = parse_stream(fp, 1); // close inside

  SSAModule *module = build_program(program);
  D_AST(program);

  fp = fopen(out_path, "w");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", out_path), -1;

  SSA_to_L_tri_call_module(module, fp);
#ifndef NDEBUG
  FILE *dump_fp = fopen("/tmp/dump.dot", "w");
  SSA_dump_func_graphviz(module, 0, dump_fp);
  fclose(dump_fp);
#endif
  fclose(fp);

  destroy_module(module);

  return 0;
}

int main(int argc, const char **argv)
{
  if (argc != 3)
    return fprintf(stderr, "Usage: %s input_file output_file\n", argv[0]), -1;

  return compile_file(argv[1], argv[2]);
}
