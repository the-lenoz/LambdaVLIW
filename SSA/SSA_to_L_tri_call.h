#ifndef SSA_TO_L_TRI_CALL_H
#define SSA_TO_L_TRI_CALL_H

#include "SSA.h"
#include <stdio.h>

int SSA_to_L_tri_call_func(const SSAModule *module, SSAFuncName func, FILE *out_fp);
int SSA_to_L_tri_call_module(const SSAModule *module, FILE *out_fp);

#endif
