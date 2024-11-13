#include <windows.h>

#include "hookspub.h"

// Initialize the virtual file system
BOOL VfsInitialize(HookVirtualFile *files, DWORD count);
// Uninitialize the virtual file system and return the output files
void VfsUninitialize(HookVirtualFile **files, PDWORD count);

// The VFS version of CreateFileA
HANDLE VfsCreateFileA(LPCSTR fileName);
// The VFS version of CreateFileW
HANDLE VfsCreateFileW(LPCWSTR fileName);
// The VFS version of CloseHandle
BOOL VfsCloseHandle(HANDLE hFile);

// The VFS version of ReadFile
BOOL VfsReadFile(HANDLE hFile, LPVOID buffer, DWORD size, LPDWORD bytesRead);
// The VFS version of WriteFile
BOOL VfsWriteFile(HANDLE hFile, LPCVOID buffer, DWORD size,
                  LPDWORD bytesWritten);

// The VFS version of GetFileSize
DWORD VfsGetFileSize(HANDLE hFile);
// The VFS version of SetFilePointer
DWORD VfsSetFilePointer(HANDLE hFile, LONG distance, DWORD method);
