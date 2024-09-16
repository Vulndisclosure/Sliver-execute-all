#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  printf("Hello, World!\n");
  printf("你好，世界！\n");

  if (argc > 1) {
    printf("Arguments passed:");
    for (int i = 1; i < argc; i++) {
      printf(" %s", argv[i]);
    }
    printf("\n");
  }

  char path[1024];
  char *dirEnd;
  if (!_fullpath(path, argv[0], sizeof(path)) ||
      !(dirEnd = strrchr(path, '\\'))) {
    printf("Error getting directory.\n");
    return 1;
  }
  *(++dirEnd) = '\0';

  strcpy_s(dirEnd, path + sizeof(path) - dirEnd, "helloworld.txt");
  FILE *file = fopen(path, "w");
  if (!file) {
    printf("Error creating file.\n");
    return 1;
  }
  char content[4096] = "Hello, World!\n你好，世界！\n";
  for (int i = 1; i < argc; i++) {
    strncat(content, argv[i], sizeof(content) - strlen(content) - 1);
    strncat(content, " ", sizeof(content) - strlen(content) - 1);
  }
  if (fputs(content, file) == EOF) {
    printf("Error writing to file.\n");
    fclose(file);
    return 1;
  }
  fclose(file);

  *strrchr(path, '\\') = '\0';
  printf("helloworld.txt has been written in %s\n", path);
  return 0;
}
