#ifndef INTERPRET_H
#define INTERPRET_H
#include "../parser/AST.h"


int interpret_program(AST* program, AST *arg_data);
int print_call_trace(FILE *fp);
int print_err(FILE *fp);


#endif // INTERPRET_H
