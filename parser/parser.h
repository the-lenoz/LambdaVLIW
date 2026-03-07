#ifndef PARSER_D
#define PARSER_D
#include "AST.h"
#include <stdio.h>

AST *parse_stream(FILE *fp, int close);

#endif // PARSER_D
