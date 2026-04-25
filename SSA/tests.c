#include "SSA.h"
#include <stdio.h>

typedef int (*ssa_builder_fn)(SSA_Func *f);

static void free_phi_lists_in_func(SSA_Func *f)
{
  if (!f)
    return;

  for (int i = 0; i < f->basic_blocks_count; ++i)
  {
    SSA_BasicBlock *bb = &f->basic_blocks[i];
    for (int j = 0; j < bb->statements_count; ++j)
    {
      SSA_stmt *stmt = &bb->statements[j];
      if (stmt->type == SSA_VAL && stmt->variant.val.is_phi_node)
      {
        SSA_PhiList_destroy(stmt->variant.val.phi_list);
        stmt->variant.val.phi_list = NULL;
      }
    }
  }
}

static int cleanup_failed_build(SSA_Func *f, PhiList **phi_lists, int phi_lists_count)
{
  for (int i = 0; i < phi_lists_count; ++i)
  {
    SSA_PhiList_destroy(phi_lists[i]);
    phi_lists[i] = NULL;
  }

  SSA_Func_destroy(f);
  return -1;
}

static int build_loop_accumulator(SSA_Func *f)
{
  PhiList *phi_i = NULL;
  PhiList *phi_acc = NULL;
  PhiList *phi_lists[2] = {NULL, NULL};

  if (SSA_Func_init(f) < 0)
    return -1;

  phi_i = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(0, 0), SSA_PHI_PAIR(2, 5));
  phi_acc = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(0, 1), SSA_PHI_PAIR(2, 6));
  phi_lists[0] = phi_i;
  phi_lists[1] = phi_acc;

  if (!phi_i || !phi_acc)
    return cleanup_failed_build(f, phi_lists, 2);

  SSA_BLOCK(bb0,
            SSA_STMT_CONST(0, 0),
            SSA_STMT_CONST(1, 0),
            SSA_STMT_GOTO(1));

  SSA_BLOCK(bb1,
            SSA_STMT_PHI(2, phi_i),
            SSA_STMT_PHI(3, phi_acc),
            SSA_STMT_LT(4, 2, SSA_IMM(6)),
            SSA_STMT_IF(4, 2, 3));

  SSA_BLOCK(bb2,
            SSA_STMT_ADD(5, 2, SSA_IMM(1)),
            SSA_STMT_ADD(6, 3, 2),
            SSA_STMT_GOTO(1));

  SSA_BLOCK(bb3,
            SSA_STMT_MOV(SSA_RESULT_GLOBAL_NAME, 3));

  if (SSA_Func_add_BB_expect(f, 0, bb0, SSA_ARRAY_LEN(bb0)) < 0 ||
      SSA_Func_add_BB_expect(f, 1, bb1, SSA_ARRAY_LEN(bb1)) < 0 ||
      SSA_Func_add_BB_expect(f, 2, bb2, SSA_ARRAY_LEN(bb2)) < 0 ||
      SSA_Func_add_BB_expect(f, 3, bb3, SSA_ARRAY_LEN(bb3)) < 0)
    return cleanup_failed_build(f, phi_lists, 2);

  return 0;
}

static int build_branch_chain(SSA_Func *f)
{
  PhiList *phi_merge = NULL;
  PhiList *phi_out = NULL;
  PhiList *phi_lists[2] = {NULL, NULL};

  if (SSA_Func_init(f) < 0)
    return -1;

  phi_merge = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(1, 11), SSA_PHI_PAIR(2, 12));
  phi_out = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(4, 16), SSA_PHI_PAIR(5, 17));
  phi_lists[0] = phi_merge;
  phi_lists[1] = phi_out;

  if (!phi_merge || !phi_out)
    return cleanup_failed_build(f, phi_lists, 2);

  SSA_BLOCK(bb0,
            SSA_STMT_CONST(0, 9),
            SSA_STMT_CONST(1, 4),
            SSA_STMT_CONST(2, 2),
            SSA_STMT_CONST(3, 3),
            SSA_STMT_CONST(4, 10),
            SSA_STMT_CONST(5, 2),
            SSA_STMT_CONST(6, 3),
            SSA_STMT_GT(10, 0, 1),
            SSA_STMT_IF(10, 1, 2));

  SSA_BLOCK(bb1,
            SSA_STMT_SUB(11, 0, 2),
            SSA_STMT_GOTO(3));

  SSA_BLOCK(bb2,
            SSA_STMT_ADD(12, 1, 3),
            SSA_STMT_GOTO(3));

  SSA_BLOCK(bb3,
            SSA_STMT_PHI(13, phi_merge),
            SSA_STMT_GTE(14, 13, 4),
            SSA_STMT_IF(14, 4, 5));

  SSA_BLOCK(bb4,
            SSA_STMT_MUL(16, 13, 5),
            SSA_STMT_GOTO(6));

  SSA_BLOCK(bb5,
            SSA_STMT_DIV(17, 13, 6),
            SSA_STMT_GOTO(6));

  SSA_BLOCK(bb6,
            SSA_STMT_PHI(18, phi_out),
            SSA_STMT_MOV(SSA_RESULT_GLOBAL_NAME, 18));

  if (SSA_Func_add_BB_expect(f, 0, bb0, SSA_ARRAY_LEN(bb0)) < 0 ||
      SSA_Func_add_BB_expect(f, 1, bb1, SSA_ARRAY_LEN(bb1)) < 0 ||
      SSA_Func_add_BB_expect(f, 2, bb2, SSA_ARRAY_LEN(bb2)) < 0 ||
      SSA_Func_add_BB_expect(f, 3, bb3, SSA_ARRAY_LEN(bb3)) < 0 ||
      SSA_Func_add_BB_expect(f, 4, bb4, SSA_ARRAY_LEN(bb4)) < 0 ||
      SSA_Func_add_BB_expect(f, 5, bb5, SSA_ARRAY_LEN(bb5)) < 0 ||
      SSA_Func_add_BB_expect(f, 6, bb6, SSA_ARRAY_LEN(bb6)) < 0)
    return cleanup_failed_build(f, phi_lists, 2);

  return 0;
}

static int build_loop_with_post_check(SSA_Func *f)
{
  PhiList *phi_x = NULL;
  PhiList *phi_y = NULL;
  PhiList *phi_res = NULL;
  PhiList *phi_lists[3] = {NULL, NULL, NULL};

  if (SSA_Func_init(f) < 0)
    return -1;

  phi_x = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(0, 8), SSA_PHI_PAIR(3, 14));
  phi_y = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(0, 9), SSA_PHI_PAIR(3, 15));
  phi_res = SSA_PHI_LIST_NEW(SSA_PHI_PAIR(5, 19), SSA_PHI_PAIR(6, 20));
  phi_lists[0] = phi_x;
  phi_lists[1] = phi_y;
  phi_lists[2] = phi_res;

  if (!phi_x || !phi_y || !phi_res)
    return cleanup_failed_build(f, phi_lists, 3);

  SSA_BLOCK(bb0,
            SSA_STMT_CONST(8, 1),
            SSA_STMT_CONST(9, 2),
            SSA_STMT_CONST(0, 5),
            SSA_STMT_CONST(1, 1),
            SSA_STMT_CONST(2, 2),
            SSA_STMT_CONST(3, 42),
            SSA_STMT_CONST(4, 2),
            SSA_STMT_CONST(5, 3),
            SSA_STMT_GOTO(1));

  SSA_BLOCK(bb1,
            SSA_STMT_PHI(10, phi_x),
            SSA_STMT_PHI(11, phi_y),
            SSA_STMT_LT(12, 10, 0),
            SSA_STMT_IF(12, 2, 4));

  SSA_BLOCK(bb2,
            SSA_STMT_ADD(13, 11, 10),
            SSA_STMT_ADD(14, 10, 1),
            SSA_STMT_GOTO(3));

  SSA_BLOCK(bb3,
            SSA_STMT_MUL(15, 13, 2),
            SSA_STMT_GOTO(1));

  SSA_BLOCK(bb4,
            SSA_STMT_EQ(16, 11, 3),
            SSA_STMT_IF(16, 5, 6));

  SSA_BLOCK(bb5,
            SSA_STMT_DIV(19, 11, 4),
            SSA_STMT_GOTO(7));

  SSA_BLOCK(bb6,
            SSA_STMT_SUB(20, 11, 5),
            SSA_STMT_GOTO(7));

  SSA_BLOCK(bb7,
            SSA_STMT_PHI(21, phi_res),
            SSA_STMT_MOV(SSA_RESULT_GLOBAL_NAME, 21));

  if (SSA_Func_add_BB_expect(f, 0, bb0, SSA_ARRAY_LEN(bb0)) < 0 ||
      SSA_Func_add_BB_expect(f, 1, bb1, SSA_ARRAY_LEN(bb1)) < 0 ||
      SSA_Func_add_BB_expect(f, 2, bb2, SSA_ARRAY_LEN(bb2)) < 0 ||
      SSA_Func_add_BB_expect(f, 3, bb3, SSA_ARRAY_LEN(bb3)) < 0 ||
      SSA_Func_add_BB_expect(f, 4, bb4, SSA_ARRAY_LEN(bb4)) < 0 ||
      SSA_Func_add_BB_expect(f, 5, bb5, SSA_ARRAY_LEN(bb5)) < 0 ||
      SSA_Func_add_BB_expect(f, 6, bb6, SSA_ARRAY_LEN(bb6)) < 0 ||
      SSA_Func_add_BB_expect(f, 7, bb7, SSA_ARRAY_LEN(bb7)) < 0)
    return cleanup_failed_build(f, phi_lists, 3);

  return 0;
}

static FILE *open_samples_file(const char *filename, char *resolved_path, size_t resolved_path_size)
{
  const char *roots[] = {"../samples", "samples"};

  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
  {
    if (snprintf(resolved_path, resolved_path_size, "%s/%s", roots[i], filename) >= (int)resolved_path_size)
      continue;

    FILE *fp = fopen(resolved_path, "w");
    if (fp)
      return fp;
  }

  return NULL;
}

static int build_and_dump(const char *filename, ssa_builder_fn builder)
{
  SSA_Func f;
  char path[256];
  FILE *out_fp;

  if (builder(&f) < 0)
    return fprintf(stderr, "Failed to build SSA function for '%s'\n", filename), -1;

  out_fp = open_samples_file(filename, path, sizeof(path));
  if (!out_fp)
  {
    free_phi_lists_in_func(&f);
    SSA_Func_destroy(&f);
    return fprintf(stderr, "Failed to open output file for '%s'\n", filename), -1;
  }

  if (generate_L_tri_if(f, out_fp) < 0)
  {
    fclose(out_fp);
    free_phi_lists_in_func(&f);
    SSA_Func_destroy(&f);
    return fprintf(stderr, "Failed to dump '%s'\n", filename), -1;
  }

  fclose(out_fp);
  printf("Wrote %s\n", path);

  free_phi_lists_in_func(&f);
  SSA_Func_destroy(&f);
  return 0;
}

int main(void)
{
  int status = 0;

  status |= build_and_dump("ssa_loop_accumulator.trif", build_loop_accumulator);
  status |= build_and_dump("ssa_branch_chain.trif", build_branch_chain);
  status |= build_and_dump("ssa_loop_with_post_check.trif", build_loop_with_post_check);

  return status ? 1 : 0;
}
