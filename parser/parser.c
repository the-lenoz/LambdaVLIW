#include "parser.h"
#include "AST.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#define DEFAULT_BUFFER_SIZE 64

static inline int is_namechar(int ch)
{
  return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' ||
         ch == '_' || ch == '-' || ch == '.' || ch == ':' || ch == '+' || ch == '/' ||
         ch == '%' || ch == '&' || ch == '|' || ch == '^' || ch == '!' || ch == '@' ||
         ch == '#' || ch == '*' || ch == '<' || ch == '=' || ch == '>' || ch == '\'';
}

static AST *parse_expr(const char **cursor, const char *end);

static int skip_spaces(const char **cursor, const char *end)
{
  if (!cursor || !end || !*cursor)
    return 0;

  for (; *cursor < end && (**cursor == ' ' || **cursor == '\n' || **cursor == '\t'); ++*cursor)
    ;
  return *cursor < end;
}

static AST *parse_list(const char **cursor, const char *end)
{
  if (!skip_spaces(cursor, end))
    return NULL;

  if (**cursor == ')')
    return NULL;

  const char *start = *cursor;

  AST *lchild = parse_expr(cursor, end);
  if (!lchild)
    return NULL;
  AST *rchild = parse_list(cursor, end);

  if (!lchild)
    return NULL;

  return N_AST(SEXP, *cursor - start, start, lchild, rchild);
}

static AST *parse_sexp(const char **cursor, const char *end)
{
  if (!skip_spaces(cursor, end))
    return NULL;
  AST *result = N_AST(SEXP, 0, NULL, NULL, NULL);
  const char *start = *cursor;
  ++*cursor;
  result->lchild = parse_expr(cursor, end);
  skip_spaces(cursor, end);
  result->rchild = parse_list(cursor, end);
  if (!skip_spaces(cursor, end) || **cursor != ')')
    return D_AST(result), NULL;
  ++*cursor;
  result->value_len = *cursor - start;
  result->value = strndup(start, result->value_len);
  return result;
}

static AST *parse_num(const char **cursor, const char *end)
{
  skip_spaces(cursor, end);
  const char *start = *cursor;
  for (; **cursor >= '0' && **cursor <= '9'; ++*cursor)
    ;
  return *cursor > start ? N_AST(CONST_NUM, *cursor - start, start, NULL, NULL) : NULL;
}

static AST *parse_string(const char **cursor, const char *end)
{
  if (!skip_spaces(cursor, end))
    return NULL;
  const char *start = *cursor;
  ++*cursor;
  for (int escaped = 0; (escaped || **cursor != '"') && *cursor < end; escaped = ('\\' == *((*cursor)++) && !escaped))
    ;
  ++*cursor;
  return *cursor > start ? N_AST(CONST_STR, *cursor - start, start, NULL, NULL) : NULL;
}

static AST *parse_name(const char **cursor, const char *end)
{
  if (!skip_spaces(cursor, end))
    return NULL;
  const char *start = *cursor;
  for (; is_namechar(**cursor); ++*cursor)
    ;
  return *cursor > start ? N_AST(NAME, *cursor - start, start, NULL, NULL) : NULL;
}

AST *parse_expr(const char **cursor, const char *end)
{
  if (!skip_spaces(cursor, end))
    return NULL;
  if (**cursor == '(')
  {
    return parse_sexp(cursor, end);
  }
  else if (**cursor >= '0' && **cursor <= '9')
  {
    return parse_num(cursor, end);
  }
  else if (**cursor == '"')
  {
    return parse_string(cursor, end);
  }
  else
    return parse_name(cursor, end);
}

static AST *parse_code_block(const char **cursor, const char *end)
{
  if (!skip_spaces(cursor, end))
    return NULL;
  const char *start = *cursor;

  AST *lchild = parse_expr(cursor, end);
  if (!lchild)
    return NULL;

  AST *rchild = parse_code_block(cursor, end);
  return N_AST(CODE_BLOCK, *cursor - start, start, lchild, rchild);
}
static AST *parse_program(const char **cursor, const char *end)
{
  if (!cursor || !end || !*cursor)
    return NULL;

  const char *start = *cursor;

  AST *lchild = parse_code_block(cursor, end);
  return N_AST(PROGRAM, *cursor - start, start, lchild, NULL);
}

AST *parse_buffer(size_t buffer_size, const char *buffer)
{
  const char *cursor = buffer;
  const char *end = buffer + buffer_size;

  return parse_program(&cursor, end);
}

AST *parse_stream(FILE *fp, int close)
{
  if (!fp)
    return NULL;

  size_t remaining_buffer = DEFAULT_BUFFER_SIZE;
  char *buffer = (char *)calloc(remaining_buffer, sizeof(char));

  size_t readed_chunk = 0, readed = 0;
  while ((readed_chunk = fread(buffer + readed, sizeof(char), remaining_buffer, fp)))
  {
    readed += readed_chunk;
    remaining_buffer -= readed_chunk;
    buffer = realloc(buffer, readed * 2);
    remaining_buffer += readed;
  }
  buffer = realloc(buffer, readed);

  AST *result = parse_buffer(readed, buffer);

  free(buffer);

  if (close)
    fclose(fp);
  return result;
}
