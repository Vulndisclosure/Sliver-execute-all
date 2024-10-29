#include <stdio.h>
#include <windows.h>

DWORD seh_filter(DWORD code) {
  printf("seh_filter: %x\n", code);
  return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv) {
  int a = 0;
  __try {
    while (argc) argc--;
    a = 123 / argc;
  } __except (seh_filter(GetExceptionCode())) {
    printf("__except: %x\n", GetExceptionCode());
  }
  printf("Hello World!\n");
  return a;
}
