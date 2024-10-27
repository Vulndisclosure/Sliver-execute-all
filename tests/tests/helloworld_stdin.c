#include <stdio.h>
#include <windows.h>

int main(int argc, char **argv) {
  printf("BEGIN|");
  char buf[1024];
  int len = 0;
  while ((len = fread(buf, 1, sizeof(buf), stdin)) > 0)
    fwrite(buf, 1, len, stdout);
  printf("|END\n");
  return 0;
}
