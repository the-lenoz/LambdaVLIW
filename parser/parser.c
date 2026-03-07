#include "parser.h"
#include "AST.h"
#include <malloc.h>
#include <stdio.h>

#define DEFAULT_BUFFER_SIZE 64

AST *parse_buffer(size_t buffer_size, const char *buffer)
{
  return NULL;
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

  printf("%s\n", buffer);

  AST *result = parse_buffer(readed, buffer);

  free(buffer);

  if (close)
    fclose(fp);
  return result;
}
