#ifndef SSA_D
#define SSA_D
#include <stdio.h>

typedef struct
{
  int global_name;
  
} SSA_val;

typedef struct 
{

} SSA_if_goto;

typedef struct
{

} SSA_goto;    

typedef struct
{
  
} SSA_BasicBlock;

typedef struct
{
  int basic_blocks_count;
  SSA_BasicBlock *basic_blocks;
  int entry_BB_index;
} SSA_Func;

int generate_L_tri_if(SSA_Func f, FILE *out_fp);

#endif
