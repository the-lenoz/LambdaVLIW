#ifndef SSA_DUMP_GRAPHVIZ_H
#define SSA_DUMP_GRAPHVIZ_H

#include "SSA.h"
#include <stdio.h>

int SSA_dump_func_graphviz(const SSAModule *module, SSAFuncName func, FILE *out_fp);
int SSA_dump_module_graphviz(const SSAModule *module, FILE *out_fp);
int SSA_dump_func_cfg_info_graphviz(SSAModule *module, SSAFuncName func, FILE *out_fp);
int SSA_dump_module_cfg_info_graphviz(SSAModule *module, FILE *out_fp);

#endif
