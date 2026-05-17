#ifndef BACKEND_D
#define BACKEND_D

#include "../SSA/SSA.h"
#include <stdio.h>

int SSAFunc_to_C(SSAModule *module, SSAFuncName func, FILE *fp);

int SSA_module_to_C(SSAModule *module, FILE *fp, int dummy);

#endif
