#ifndef FRONTEND_D
#define FRONTEND_D

#include "../SSA/SSA.h"
#include "../parser/AST.h"

SSAModule *build_program(AST *program);

#endif
