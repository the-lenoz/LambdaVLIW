#include "../C_backend/backend.h"
#include "../L_defun/frontend.h"
#include "../SSA/SSA_dump_graphviz.h"
#include "../parser/parser.h"
#include "../optimizer/optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  const char *source_path;
  const char *out_path;
  const char *dot_path;
  int enable_optimizations;
  int use_dummy_backend;
} CompileOptions;

static int print_usage(const char *argv0)
{
  return fprintf(stderr,
                 "Usage: %s [--no-opt] [--dummy-backend|--system-backend] [--dump-dot path.dot] -o output.c input.lisp\n"
                 "  -o <path>         Output C file. Use '-' for stdout.\n"
                 "  --dump-dot <path> Dump SSA Graphviz DOT to file.\n"
                 "  --no-opt          Disable optimizer passes.\n"
                 "  --dummy-backend   Force dummy C backend.\n"
                 "  --system-backend  Force structured backend (default).\n",
                 argv0);
}

static int finish_compile(FILE *out_fp, int must_close_out, FILE *debug_fp, int result)
{
  if (debug_fp)
    fclose(debug_fp);
  if (must_close_out)
    fclose(out_fp);
  return result;
}

static int compile(const CompileOptions *options)
{
  FILE *src_fp;
  FILE *out_fp;
  FILE *debug_fp = NULL;
  AST *program;
  SSAModule *module;
  int must_close_out = 0;

  if (!options || !options->source_path || !options->out_path)
    return -1;

  src_fp = fopen(options->source_path, "r");
  if (!src_fp)
    return fprintf(stderr, "Fatal: can't open file '%s'.\n", options->source_path), -1;

  if (!strcmp(options->out_path, "-"))
    out_fp = stdout;
  else
  {
    out_fp = fopen(options->out_path, "w");
    if (!out_fp)
      return fclose(src_fp), fprintf(stderr, "Fatal: can't open file '%s'.\n", options->out_path), -1;
    must_close_out = 1;
  }

  program = parse_stream(src_fp, 1);
  if (!program)
    return fprintf(stderr, "Fatal: syntax error.\n"), finish_compile(out_fp, must_close_out, debug_fp, -1);

  module = build_program(program);
  D_AST(program);
  if (!module)
    return fprintf(stderr, "Fatal: semanthic error.\n"), finish_compile(out_fp, must_close_out, debug_fp, -1);

  if (options->enable_optimizations)
    optimize_module(module);

  if (options->dot_path)
    debug_fp = fopen(options->dot_path, "w");
  if (debug_fp)
  {
    SSA_dump_module_graphviz(module, debug_fp);
    fclose(debug_fp);
    debug_fp = NULL;
  }
  else if (options->dot_path)
  {
    fprintf(stderr, "Fatal: can't open file '%s'.\n", options->dot_path);
    destroy_module(module);
    return finish_compile(out_fp, must_close_out, debug_fp, -1);
  }

  if (!SSA_module_to_C(module, out_fp, options->use_dummy_backend))
  {
    fprintf(stderr, "Fatal: invalid SSA produced.\n");
    destroy_module(module);
    return finish_compile(out_fp, must_close_out, debug_fp, -1);
  }

  destroy_module(module);
  return finish_compile(out_fp, must_close_out, debug_fp, 0);
}

int main(int argc, char **argv)
{
  CompileOptions options = {
      .source_path = NULL,
      .out_path = NULL,
      .dot_path = NULL,
      .enable_optimizations = 1,
      .use_dummy_backend = 0,
  };

  for (int i = 1; i < argc; ++i)
  {
    if (!strcmp(argv[i], "-o"))
    {
      if (i + 1 >= argc)
        return print_usage(argv[0]);
      options.out_path = argv[++i];
    }
    else if (!strcmp(argv[i], "--dump-dot"))
    {
      if (i + 1 >= argc)
        return print_usage(argv[0]);
      options.dot_path = argv[++i];
    }
    else if (!strcmp(argv[i], "--no-opt"))
      options.enable_optimizations = 0;
    else if (!strcmp(argv[i], "--dummy-backend"))
      options.use_dummy_backend = 1;
    else if (!strcmp(argv[i], "--system-backend"))
      options.use_dummy_backend = 0;
    else if (argv[i][0] == '-')
      return print_usage(argv[0]);
    else if (!options.source_path)
      options.source_path = argv[i];
    else
      return print_usage(argv[0]);
  }

  if (!options.source_path || !options.out_path)
    return print_usage(argv[0]);

  return compile(&options);
}
