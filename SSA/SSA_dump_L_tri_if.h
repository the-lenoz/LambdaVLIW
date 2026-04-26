#ifndef SSA_DUMP_L_TRI_IF_H
#define SSA_DUMP_L_TRI_IF_H

#include "SSA.h"
#include <stdio.h>

int SSA_dump_func_L_tri_if(const SSAModule *module, SSAFuncName func, FILE *out_fp);
int SSA_dump_module_L_tri_if(const SSAModule *module, FILE *out_fp);

#endif
