#include "../parser/AST.h"
#include "../parser/parser.h"
#include "interpret.h"
#include <stdio.h>

int run_script(const char *script_path, const int argc, char **argv)
{
  FILE *fp = fopen(script_path, "r");
  if (!fp)
    return fprintf(stderr, "Fatal: couldn't open file '%s'\n", script_path), -1;

  AST *program = parse_stream(fp, 1);

  AST *arg_data = NULL;

  if (argc == 1)
  {
    FILE *arg_file = fopen(argv[0], "w");
    if (arg_file)
      arg_data = parse_stream(arg_file, 1);
    else
      fprintf(stderr, "Failed to open file '%s', sipping.\n", argv[0]);
  }

  int status = interpret_program(program, arg_data);

  if (status == -1)
  {
    print_call_trace(stderr);
    print_err(stderr);
  }    
  
  D_AST(program);

  return status;
}

int main(int argc, char **argv)
{
  if (argc < 2)
    return fprintf(stderr, "Usage: %s script.lisp [...]\n", argv[0]), -1;

  return run_script(argv[1], argc - 2, argv + 2);
}
