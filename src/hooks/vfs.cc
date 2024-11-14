#include <shlwapi.h>
#include <string.h>

#include "hooks.h"

/*==============================================================================
 * Virtual File System
 *============================================================================*/

// Represents a opened virtual file backed by HookVirtualFile
typedef struct {
  HookVirtualFile *File;
  DWORD Position;
  volatile LONG Lock;
} VirtualFileObject;

static SimpleList VirtualFileList = {};  // HookVirtualFile *
static volatile LONG VirtualFileListLock = 0;
// For output file with pattern. It is not a file and is read-only.
static SimpleList VirtualFilePatternList = {};  // HookVirtualFile
static SimpleList VirtualFileObjectList = {};   // VirtualFileObject *
static volatile LONG VirtualFileObjectListLock = 0;

// Find the HookVirtualFile by file name
static HookVirtualFile *VfsLookupByName(LPCWSTR fileName) {
  // Check if the file is in the virtual file list
  HookVirtualFile *file = NULL;
  SpinLock(VirtualFileListLock, {
    SimpleListForEach(&VirtualFileList, HookVirtualFile *, f, {
      if (_wcsicmp(fileName, (*f)->FileName) == 0) {
        file = *f;
        break;
      }
    });
  });
  return file;
}

// Find the HookVirtualFile by file name pattern
static HookVirtualFile *VfsLookupByPattern(LPCWSTR fileName) {
  // Check if the file is in the virtual file pattern list
  HookVirtualFile *file = NULL;
  SimpleListForEach(&VirtualFilePatternList, HookVirtualFile, f, {
    if (PathMatchSpecW(fileName, f->FileName)) {
      file = f;
      break;
    }
  });
  return file;
}

// Create a empty virtual file with the specified file name
static HookVirtualFile *VfsAllocateFile(LPCWSTR fileName, DWORD fileNameCch,
                                        HookVirtualFileType type) {
  HookVirtualFile *file = (HookVirtualFile *)malloc(sizeof(HookVirtualFile));
  if (!file) {
    TRACEF("VfsAllocateFile: failed to allocate memory for HookVirtualFile.");
    return NULL;
  }
  *file = {};
  if (!SimpleListAlloc(&file->Data, 4)) {
    TRACEF("VfsAllocateFile: failed to allocate memory for file data.");
    free(file);
    return NULL;
  }
  memcpy(file->FileName, fileName, fileNameCch * sizeof(WCHAR));
  TRACEF("VfsAllocateFile: HookVirtualFile was created for %ws.", fileName);
  file->Type = type;
  SpinLock(VirtualFileListLock,
           SimpleListAdd(&VirtualFileList, HookVirtualFile *, file));
  TRACEF(
      "VfsAllocateFile: HookVirtualFile was added to the virtual file list.");
  return file;
}

static void VfsFreeFile(HookVirtualFile *file) {
  if (file) {
    SimpleListFree(&file->Data);
    free(file);
  }
}

// Create a virtual file object for the specified virtual file
static VirtualFileObject *VfsAllocateObject(HookVirtualFile *file) {
  VirtualFileObject *object =
      (VirtualFileObject *)malloc(sizeof(VirtualFileObject));
  if (!object) {
    TRACEF(
        "VfsAllocateObject: failed to allocate memory for VirtualFileObject.");
    return NULL;
  }
  *object = {};
  object->File = file;
  TRACEF("VfsAllocateObject: VirtualFileObject was created for %ws.",
         file->FileName);
  SpinLock(VirtualFileObjectListLock, {
    BOOL inserted = FALSE;
    SimpleListForEach(&VirtualFileObjectList, VirtualFileObject *, o, {
      if (!*o) {
        *o = object;
        inserted = TRUE;
        break;
      }
    });
    if (!inserted) {
      SimpleListAdd(&VirtualFileObjectList, VirtualFileObject *, object);
    }
  });
  TRACEF("VfsAllocateObject: VirtualFileObject was added to the object list.");
  return object;
}

static void VfsFreeObject(VirtualFileObject *object) {
  if (object) {
    free(object);
  }
}

#ifdef _WIN64
#define HANDLE_MARK 0x8000000000000000
#else
#define HANDLE_MARK 0x80000000
#endif

#define DEREF(x) ((x) ? *(x) : NULL)

// Try to get the VirtualFileObject from the handle
static VirtualFileObject **VfsHandleToObject(HANDLE hFile) {
  // Check if the handle is marked
  if (((SIZE_T)hFile & HANDLE_MARK) != HANDLE_MARK) {
    return NULL;
  }
  hFile = (HANDLE)((SIZE_T)hFile & ~HANDLE_MARK);

  // Check if the handle is in the virtual file object list
  VirtualFileObject **object = NULL;
  SpinLock(VirtualFileObjectListLock, {
    SimpleListForEach(&VirtualFileObjectList, VirtualFileObject *, o, {
      if ((HANDLE)*o == hFile) {
        object = o;
        break;
      }
    });
  });
#ifdef _DEBUG
  if (object) {
    VirtualFileObject *o = *object;
    if (!o) {
      TRACEF("VfsHandleToObject: found NULL object for %p.", hFile);
      __debugbreak();
    }
    if (!o->File) {
      TRACEF("VfsHandleToObject: found object with NULL file for %p.", hFile);
      __debugbreak();
    }
    if (!o->File->Data.Buffer) {
      TRACEF("VfsHandleToObject: found object with NULL file data for %p.",
             hFile);
      __debugbreak();
    }
    // TRACEF("VfsHandleToObject: found object for %ws.", o->File->FileName);
  }
#endif
  return object;
}

// Convert the VirtualFileObject to the handle
static HANDLE VfsObjectToHandle(VirtualFileObject *object) {
  if (!object) {
    return INVALID_HANDLE_VALUE;
  }
  return (HANDLE)((SIZE_T)object | HANDLE_MARK);
}

#ifdef _DEBUG
static DWORD VfsCountAliveObjects() {
  DWORD count = 0;
  SpinLock(VirtualFileObjectListLock, {
    SimpleListForEach(&VirtualFileObjectList, VirtualFileObject *, o, {
      if (*o) {
        count++;
      }
    });
  });
  return count;
}
#endif

BOOL VfsInitialize(HookVirtualFile *files, DWORD count) {
  if (!files || !count) {
    return FALSE;
  }

  // Initialize the virtual file system
  if (!SimpleListAlloc(&VirtualFileList, sizeof(HookVirtualFile *) * 8)) {
    TRACEF("VfsInitialize: failed to allocate memory for VirtualFileList.");
    return FALSE;
  }
  if (!SimpleListAlloc(&VirtualFilePatternList, sizeof(HookVirtualFile) * 8)) {
    TRACEF(
        "VfsInitialize: failed to allocate memory for VirtualFilePatternList.");
    return FALSE;
  }
  if (!SimpleListAlloc(&VirtualFileObjectList,
                       sizeof(VirtualFileObject *) * 8)) {
    TRACEF(
        "VfsInitialize: failed to allocate memory for VirtualFileObjectList.");
    return FALSE;
  }

  // Add the virtual files
  for (DWORD i = 0; i < count; i++) {
    HookVirtualFile *file = &files[i];
    if (file->Type == HookVirtualFileOutputPattern) {
      SimpleListAdd(&VirtualFilePatternList, HookVirtualFile, *file);
      continue;
    }

    file = (HookVirtualFile *)malloc(sizeof(HookVirtualFile));
    if (file) {
      *file = files[i];
      files[i].Data = {};  // Prevent from freeing the input file
      SimpleListAdd(&VirtualFileList, HookVirtualFile *, file);
    } else {
      TRACEF("VfsInitialize: failed to allocate memory for HookVirtualFile.");
    }
  }
  return TRUE;
}

void VfsUninitialize(HookVirtualFile **files, PDWORD count) {
  // Return the output files
  if (files && count) {
    // Count the number of output files
    DWORD n = 0;
    SimpleListForEach(&VirtualFileList, HookVirtualFile *, f, {
      if ((*f)->Type == HookVirtualFileOutput) {
        n++;
      }
    });

    // Copy the output files
    HookVirtualFile *buffer =
        (HookVirtualFile *)malloc(sizeof(HookVirtualFile) * n);
    if (buffer) {
      DWORD i = 0;
      SimpleListForEach(&VirtualFileList, HookVirtualFile *, f, {
        if ((*f)->Type == HookVirtualFileOutput) {
          buffer[i++] = **f;
          (*f)->Data = {};  // Prevent from freeing the output file
        }
      });
      *files = buffer;
      *count = n;
    } else {
      *files = NULL;
      *count = 0;
      TRACEF("VfsUninitialize: failed to allocate memory for output files.");
    }
  }

  // Free the virtual file system
  SimpleListForEach(&VirtualFileObjectList, VirtualFileObject *, o,
                    VfsFreeObject(*o));
  SimpleListFree(&VirtualFileObjectList);
  SimpleListFree(&VirtualFilePatternList);
  SimpleListForEach(&VirtualFileList, HookVirtualFile *, f, VfsFreeFile(*f));
  SimpleListFree(&VirtualFileList);
}

HANDLE VfsCreateFileW(LPCWSTR fileName) {
  // Get full path of the file
  WCHAR fullPath[MAX_PATH];
  if (!fileName || !GetFullPathNameW(fileName, MAX_PATH, fullPath, NULL)) {
    return INVALID_HANDLE_VALUE;
  }

  // Check if the file is in the virtual file list
  HookVirtualFile *file = VfsLookupByName(fullPath);
  if (file) {
    TRACEF("VfsCreateFileW: %ws was found in the virtual file list.", fullPath);
  }

  // Check if the file is in the virtual file pattern list
  if (VfsLookupByPattern(fullPath)) {
    TRACEF("VfsCreateFileW: %ws was found in the virtual file pattern list.",
           fullPath);
    file = VfsAllocateFile(fullPath, MAX_PATH, HookVirtualFileOutput);
  }

  // Allocate object for the virtual file
  if (file) {
    HANDLE hFile = VfsObjectToHandle(VfsAllocateObject(file));
#ifdef _DEBUG
    TRACEF("VfsCreateFileW: %ws was opened at %p. (%d objects alive)", fullPath,
           hFile, VfsCountAliveObjects());
#else
    TRACEF("VfsCreateFileW: %ws was opened at %p.", fullPath, hFile);
#endif
    return hFile;
  }

  // Not a virtual file
  return INVALID_HANDLE_VALUE;
}

HANDLE VfsCreateFileA(LPCSTR fileName) {
  // Convert ANSI to Unicode
  WCHAR fileNameW[MAX_PATH];
  if (!fileName ||
      !MultiByteToWideChar(CP_ACP, 0, fileName, -1, fileNameW, MAX_PATH)) {
    return INVALID_HANDLE_VALUE;
  }

  // Forward to VfsCreateFileW
  return VfsCreateFileW(fileNameW);
}

BOOL VfsCloseHandle(HANDLE hFile) {
  // Get the virtual file object
  VirtualFileObject **object = VfsHandleToObject(hFile);
  if (!object) {
    return FALSE;
  }

  // Free the virtual file object
  HookVirtualFile *file = (*object)->File;
  VfsFreeObject(*object);
  *object = NULL;
#ifdef _DEBUG
  TRACEF("VfsCloseHandle: %ws was closed. (%d objects alive)", file->FileName,
         VfsCountAliveObjects());
#else
  TRACEF("VfsCloseHandle: %ws was closed.", file->FileName);
#endif
  return TRUE;
}

static DWORD VfsReadFileNoLock(VirtualFileObject *object, LPVOID buffer,
                               DWORD size) {
  if (!size) {
    return 0;
  }

  // Get basic information
  HookVirtualFile *file = object->File;
  DWORD position = object->Position;
  DWORD fileSize = (DWORD)file->Data.Size;
  if (position >= fileSize) {
    return 0;
  }

  // Calculate the number of bytes to read
  DWORD read = fileSize - position;
  if (read > size) {
    read = size;
  }

  // Read data from the virtual file
  memcpy(buffer, (char *)file->Data.Buffer + position, read);
  object->Position += read;
  TRACEF("VfsReadFileNoLock: %ws was read (%d bytes at %d, total %d bytes).",
         file->FileName, read, position, fileSize);
  return read;
}

BOOL VfsReadFile(HANDLE hFile, LPVOID buffer, DWORD size, LPDWORD bytesRead) {
  if (!buffer) {
    return FALSE;
  }

  // Get the virtual file object
  VirtualFileObject *object = DEREF(VfsHandleToObject(hFile));
  if (!object) {
    return FALSE;
  }

  // Read data from the virtual file
  DWORD t = 0;
  SpinLock(object->Lock, SpinLock(object->File->Lock,
                                  t = VfsReadFileNoLock(object, buffer, size)));
  if (bytesRead) {
    *bytesRead = t;
  }
  return TRUE;
}

static DWORD VfsWriteFileNoLock(VirtualFileObject *object, LPCVOID buffer,
                                DWORD size) {
  if (!size) {
    return 0;
  }

  // Get basic information
  HookVirtualFile *file = object->File;
  DWORD position = object->Position;
  DWORD fileSize = (DWORD)file->Data.Size;

  // Calculate the number of bytes to overwrite and append
  DWORD overwrite = fileSize - position;
  if (overwrite > size) {
    overwrite = size;
  }
  DWORD append = size - overwrite;

  // Write data to the virtual file
  if (overwrite) {
    memcpy((char *)file->Data.Buffer + position, buffer, overwrite);
    object->Position += overwrite;
  }
  if (append) {
    if (!SimpleListAddBuffer(&file->Data, (char *)buffer + overwrite, append)) {
      TRACEF("VfsWriteFileNoLock: failed to append data to the virtual file.");
      return overwrite;
    }
    object->Position += append;
  }
  return size;
}

BOOL VfsWriteFile(HANDLE hFile, LPCVOID buffer, DWORD size,
                  LPDWORD bytesWritten) {
  if (!buffer) {
    return FALSE;
  }

  // Get the virtual file object
  VirtualFileObject *object = DEREF(VfsHandleToObject(hFile));
  if (!object) {
    return FALSE;
  }

  // Write data to the virtual file
  DWORD t = 0;
  SpinLock(object->Lock,
           SpinLock(object->File->Lock,
                    t = VfsWriteFileNoLock(object, buffer, size)));
  if (bytesWritten) {
    *bytesWritten = t;
  }
  return TRUE;
}

BOOL VfsQueryObject(HANDLE hFile, PDWORD size, PDWORD position) {
  // Get the virtual file object
  VirtualFileObject *object = DEREF(VfsHandleToObject(hFile));
  if (!object) {
    return FALSE;
  }

  // Get the file size and position
  if (size) {
    *size = (DWORD)object->File->Data.Size;
  }
  if (position) {
    *position = object->Position;
  }
  return TRUE;
}

BOOL VfsSetObject(HANDLE hFile, PDWORD position) {
  // Get the virtual file object
  VirtualFileObject *object = DEREF(VfsHandleToObject(hFile));
  if (!object) {
    return FALSE;
  }

  // Set the file position
  if (position) {
    object->Position = *position;
  }
  return TRUE;
}
