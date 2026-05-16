#ifndef OPTIMIZER_D
#define OPTIMIZER_D
#include "../SSA/SSA.h"



int self_TCO_module(SSAModule *module);
    
int optimize_module(SSAModule *module);

#endif 
