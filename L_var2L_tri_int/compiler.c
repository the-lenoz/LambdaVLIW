#include <stdio.h>
#include <stdlib.h>

int compile_buffer(const char *source_buffer, char **result_buffer, size_t *result_size)
{
  
  return 0;
}    

int compile_file(const char *path, const char *out_path)
{
  FILE *fp = fopen(path, "r");

  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", path), -1;

  fseek(fp, 0L, SEEK_END);
  size_t file_size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  char *buffer = malloc(file_size * sizeof(char));

  fread(buffer, sizeof(char), file_size, fp);
  fclose(fp);

  char *result;
  size_t result_size;
  int status = compile_buffer(buffer, &result, &result_size);
  free(buffer);

  if (status != 0)
    return status;

  if (!result)
    return fprintf(stderr, "No compilation result, exiting.\n"), -1;

  fp = fopen(out_path, "w");
  if (!fp)
    return fprintf(stderr, "Fatal: can't open file '%s'\n", path), -1;
  fwrite(result, sizeof(char), result_size, fp);
  fclose(fp);
  free(result);
  return 0;
}

int main(int argc, const char **argv)
{
  if (argc != 3)
    return fprintf(stderr, "Usage: %s input_file output_file\n", argv[0]), -1;

  return compile_file(argv[1], argv[2]);
}
