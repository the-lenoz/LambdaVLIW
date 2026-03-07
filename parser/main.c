#include "parser.h"
#include <stdio.h>

int main(int argc, char **argv)
{
  FILE *fp = stdin;
  int close = 0;
  if (argc == 2)
  {
    FILE *file_try = fopen(argv[1], "r");
    close = 1;
    if (file_try)
      fp = file_try;
    else
      return fprintf(stderr, "Error: can't open file \"%s\". Exiting.\nUsage: %s [code_file]\n"
                             "If code_file is not set, parsing from stdin.\n",
                     argv[1], argv[0]),
             -1;
  }

  parse_stream(fp, close);
  return 0;
}
