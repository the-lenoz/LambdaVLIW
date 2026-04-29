#include <stdio.h>
#include "../parser/parser.h"
#include "../L_defun/frontend.h"
#include "../C_backend/backend.h"



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
  if (!module)
    return fprintf(stderr, "Fatal: semanthic error.\n"), fclose(out_fp), -1;

  if (!dummy_SSA_module_to_C(module, out_fp))
  {
    fclose(out_fp);
    fprintf(stderr, "Fatal: invalid SSA produced.\n");
    return -1;
  }    
  
  fclose(out_fp);  
  return 0;
}    

int main(int argc, char **argv)
{
  if (argc < 3)
    return fprintf(stderr, "Usage: %s input.lisp output.c\n", argv[0]);

  return compile(argv[1], argv[2]);
}
