/* The MIT License

   Copyright (c) 2008, by Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/*
  An example:

#include "kvec.h"
int main() {
        kvec_t(int) array;
        kv_init(array);
        kv_push(int, array, 10); // append
        kv_a(int, array, 20) = 5; // dynamic
        kv_A(array, 20) = 4; // static
        kv_destroy(array);
        return 0;
}
*/

/*
  2008-09-22 (0.1.0):

        * The initial version.

*/

#include <stdlib.h>

static void *safe_realloc(void *p, size_t size) {
  void *q = realloc(p, size);
  if (!q) {
    abort();
  }
  return q;
}

static void *safe_malloc(size_t size) {
  void *p = malloc(size);
  if (!p) {
    abort();
  }
  return p;
}

#ifndef AC_KVEC_H
#define AC_KVEC_H

#include <stdlib.h>

#define kv_roundup32(x)                                                       \
  (--(x), (x) |= (x) >> 1, (x) |= (x) >> 2, (x) |= (x) >> 4, (x) |= (x) >> 8, \
   (x) |= (x) >> 16, ++(x))

#define kvec_t(type) \
  struct {           \
    size_t n, m;     \
    type *a;         \
  }
#define kv_init(v) ((v).n = (v).m = 0, (v).a = 0)
#define kv_destroy(v) free((v).a)
#define kv_A(v, i) ((v).a[(i)])
#define kv_pop(v) ((v).a[--(v).n])
#define kv_size(v) ((v).n)
#define kv_max(v) ((v).m)

#define kv_resize(type, v, s) \
  ((v).m = (s), (v).a = (type *)safe_realloc((v).a, sizeof(type) * (v).m))

#define kv_copy(type, v1, v0)                         \
  do {                                                \
    if ((v1).m < (v0).n) kv_resize(type, v1, (v0).n); \
    (v1).n = (v0).n;                                  \
    memcpy((v1).a, (v0).a, sizeof(type) * (v0).n);    \
  } while (0)

#define kv_push(type, v, x)                                      \
  do {                                                           \
    if ((v).n == (v).m) {                                        \
      (v).m = (v).m ? (v).m << 1 : 2;                            \
      (v).a = (type *)safe_realloc((v).a, sizeof(type) * (v).m); \
    }                                                            \
    (v).a[(v).n++] = (x);                                        \
  } while (0)

#define kv_pushp(type, v)                                               \
  (((v).n == (v).m)                                                     \
       ? ((v).m = ((v).m ? (v).m << 1 : 2),                             \
          (v).a = (type *)safe_realloc((v).a, sizeof(type) * (v).m), 0) \
       : 0),                                                            \
      ((v).a + ((v).n++))

#define kv_a(type, v, i)                                                 \
  (((v).m <= (size_t)(i)                                                 \
        ? ((v).m = (v).n = (i) + 1, kv_roundup32((v).m),                 \
           (v).a = (type *)safe_realloc((v).a, sizeof(type) * (v).m), 0) \
    : (v).n <= (size_t)(i) ? (v).n = (i) + 1                             \
                           : 0),                                         \
   (v).a[(i)])

// Mod

#define kv_pusha(type, v, x, xn)                                 \
  do {                                                           \
    size_t __xn = (xn);                                          \
    if ((v).n + __xn > (v).m) {                                  \
      (v).m = (v).n + __xn;                                      \
      kv_roundup32((v).m);                                       \
      (v).a = (type *)safe_realloc((v).a, sizeof(type) * (v).m); \
    }                                                            \
    memcpy((v).a + (v).n, (x), sizeof(type) * __xn);             \
    (v).n += __xn;                                               \
  } while (0)

#endif

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "pe.h"
#endif

/*==============================================================================
 * Utility
 *============================================================================*/

typedef kvec_t(uint8_t) buffer_t;
static void buf_add(buffer_t *v, uint8_t x) { kv_push(uint8_t, *v, x); }
static void buf_addall(buffer_t *v, void *x, size_t xn) {
  kv_pusha(uint8_t, *v, x, xn);
}
static void buf_free(buffer_t *v) {
  if (v->a) {
    kv_destroy(*v);
  }
}
static void buf_init(buffer_t *v) {
  if (!v->a) {
    kv_resize(uint8_t, *v, 4);
  }
}
static void buf_reset(buffer_t *v) { v->n = 0; }
static void buf_sprintf(buffer_t *v, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  size_t min_size = v->n + n + 1;
  if (v->m < min_size) {
    kv_roundup32(min_size);
    kv_resize(uint8_t, *v, min_size);
  }
  va_start(args, fmt);
  vsnprintf((char *)v->a + v->n, n + 1, fmt, args);
  va_end(args);
  v->n += n;
  v->a[v->n] = 0;
}

static void *ReadFileBytes(const char *file_name, size_t *size) {
  if (!file_name) {
    return NULL;
  }

  FILE *file = NULL;
  size_t file_size = 0;
  void *data = NULL;

  // Open the file
  file = fopen(file_name, "rb");
  if (!file) goto err;
  fseek(file, 0, SEEK_END);
  file_size = ftell(file);
  if (file_size == -1) goto err;
  rewind(file);

  // Read the file
  data = malloc(file_size);
  if (!data || fread(data, 1, file_size, file) != file_size) goto err;

  if (size) *size = file_size;
  fclose(file);
  return data;

err:
  if (size) *size = 0;
  if (data) free(data);
  if (file) fclose(file);
  return NULL;
}

static bool WriteFileBytes(const char *file_name, void *data, size_t size) {
  if (!file_name || !data || size <= 0) {
    return false;
  }

  FILE *file = NULL;
  size_t written = 0;

  // Open the file
  file = fopen(file_name, "wb");
  if (!file) return false;

  // Write the data
  written = fwrite(data, 1, size, file);
  if (written != size) {
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

static const char *GetFileName(const char *path) {
  const char *name = strrchr(path, '\\');
  if (!name) {
    name = strrchr(path, '/');
  }
  if (name) {
    return name + 1;
  }
  return path;
}

static bool RemoveExtension(char *path) {
  char *ext = strrchr(path, '.');
  if (ext) {
    *ext = 0;
    return true;
  }
  return false;
}

static PIMAGE_NT_HEADERS GetNtHeaders(void *pe_image) {
  PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)pe_image;
  if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
    return NULL;
  }
  return (PIMAGE_NT_HEADERS)((char *)pe_image + dos_header->e_lfanew);
}

static PIMAGE_DATA_DIRECTORY GetDataDirectory(void *pe_image) {
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  if (!nt_headers) {
    return NULL;
  }
  if (nt_headers->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return ((PIMAGE_NT_HEADERS32)nt_headers)->OptionalHeader.DataDirectory;
  } else {
    return ((PIMAGE_NT_HEADERS64)nt_headers)->OptionalHeader.DataDirectory;
  }
}

static uint32_t RvaToFileOffset(PIMAGE_NT_HEADERS nt_headers, uint32_t rva) {
  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);
  for (uint16_t i = 0; i < nt_headers->FileHeader.NumberOfSections;
       i++, section++) {
    if (rva >= section->VirtualAddress &&
        rva < section->VirtualAddress + section->SizeOfRawData) {
      return rva - section->VirtualAddress + section->PointerToRawData;
    }
  }
  return 0;
}

static void *GetDataDirectoryData(void *pe_image, int index, uint32_t *size) {
  PIMAGE_DATA_DIRECTORY data_dirs = GetDataDirectory(pe_image);
  if (size) {
    *size = data_dirs[index].Size;
  }
  uint32_t rva = data_dirs[index].VirtualAddress;
  if (!rva) {
    return NULL;
  }
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  return (char *)pe_image + RvaToFileOffset(nt_headers, rva);
}

/*==============================================================================
 * Compressed UInt32
 *============================================================================*/

#ifdef _MSC_VER
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#else
#define bswap16(x) __builtin_bswap16((x))
#define bswap32(x) __builtin_bswap32((x))
#endif

static bool WriteCompressedUInt32(buffer_t *array, uint32_t value) {
  if (value < 0x80) {
    buf_add(array, (uint8_t)value);
  } else if (value < 0x4000) {
    uint16_t v = bswap16(value) | 0x80;
    buf_addall(array, &v, 2);
  } else if (value < 0x20000000) {
    uint32_t v = bswap32(value) | 0xC0;
    buf_addall(array, &v, 4);
  } else {
    printf("Value too large: 0x%x\n", value);
    return false;
  }
  return true;
}

static uint32_t ReadCompressedUInt32(uint8_t **p) {
  uint8_t *q = *p;
  uint32_t a = *q;
  if (!(a & 0x80)) {
    *p = q + 1;
    return a;
  } else if (!(a & 0x40)) {
    a = bswap16(*(uint16_t *)q & 0xFF3F);
    *p = q + 2;
    return a;
  } else {
    a = bswap32(*(uint32_t *)q & 0xFFFFFF1F);
    *p = q + 4;
    return a;
  }
}

static void TestCompressedUInt32() {
  buffer_t array = {0};
  bool b = WriteCompressedUInt32(&array, 0x7F);
  assert(b);
  b = WriteCompressedUInt32(&array, 0x80);
  assert(b);
  b = WriteCompressedUInt32(&array, 0x3FFF);
  assert(b);
  b = WriteCompressedUInt32(&array, 0x4000);
  assert(b);
  b = WriteCompressedUInt32(&array, 0x1FFFFFFF);
  assert(b);
  b = WriteCompressedUInt32(&array, 0x20000000);
  assert(!b);
  b = WriteCompressedUInt32(&array, 0x7FFFFFFF);
  assert(!b);

  uint8_t *p = array.a;
  uint32_t v = ReadCompressedUInt32(&p);
  assert(v == 0x7F);
  v = ReadCompressedUInt32(&p);
  assert(v == 0x80);
  v = ReadCompressedUInt32(&p);
  assert(v == 0x3FFF);
  v = ReadCompressedUInt32(&p);
  assert(v == 0x4000);
  v = ReadCompressedUInt32(&p);
  assert(v == 0x1FFFFFFF);
  buf_free(&array);
}

/*==============================================================================
 * Import Table
 *============================================================================*/

static bool IsEmptyThunk(void *thunk, uint16_t hdr_magic) {
  if (hdr_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return ((PIMAGE_THUNK_DATA32)thunk)->u1.AddressOfData == 0;
  } else {
    return ((PIMAGE_THUNK_DATA64)thunk)->u1.AddressOfData == 0;
  }
}

static void *NextThunk(void *thunk, uint16_t hdr_magic) {
  if (hdr_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return (void *)((PIMAGE_THUNK_DATA32)thunk + 1);
  } else {
    return (void *)((PIMAGE_THUNK_DATA64)thunk + 1);
  }
}

static bool IsOrdinalThunk(void *thunk, uint16_t hdr_magic) {
  if (hdr_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return IMAGE_SNAP_BY_ORDINAL32(((PIMAGE_THUNK_DATA32)thunk)->u1.Ordinal);
  } else {
    return IMAGE_SNAP_BY_ORDINAL64(((PIMAGE_THUNK_DATA64)thunk)->u1.Ordinal);
  }
}

static uint16_t GetImportOrdinal(void *thunk, uint16_t hdr_magic) {
  if (hdr_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return IMAGE_ORDINAL32(((PIMAGE_THUNK_DATA32)thunk)->u1.Ordinal);
  } else {
    return IMAGE_ORDINAL64(((PIMAGE_THUNK_DATA64)thunk)->u1.Ordinal);
  }
}

static char *GetImportName(void *thunk, void *pe_image, uint16_t hdr_magic) {
  uint32_t rva;
  if (hdr_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    rva = ((PIMAGE_THUNK_DATA32)thunk)->u1.AddressOfData;
  } else {
    rva = (uint32_t)((PIMAGE_THUNK_DATA64)thunk)->u1.AddressOfData;
  }
  PIMAGE_IMPORT_BY_NAME name =
      (PIMAGE_IMPORT_BY_NAME)((char *)pe_image +
                              RvaToFileOffset(GetNtHeaders(pe_image), rva));
  return name->Name;
}

// Write the import table in a more compact format
static bool WriteImports(buffer_t *new_imports, void *pe_image,
                         PIMAGE_IMPORT_DESCRIPTOR import_descriptors,
                         uint32_t rva_base) {
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  uint16_t hdr_magic = nt_headers->OptionalHeader.Magic;

  // Write the import table
  while (import_descriptors->Name) {
    // Write the module name
    char *module_name = (char *)pe_image +
                        RvaToFileOffset(nt_headers, import_descriptors->Name);
    buf_addall(new_imports, module_name, strlen(module_name) + 1);

    // Write the function names and IATs
    DWORD thunk_rva = import_descriptors->OriginalFirstThunk;
    if (!thunk_rva) {
      thunk_rva = import_descriptors->FirstThunk;
    }
    void *thunk = (char *)pe_image + RvaToFileOffset(nt_headers, thunk_rva);
    void *iat = (char *)pe_image +
                RvaToFileOffset(nt_headers, import_descriptors->FirstThunk);
    while (!IsEmptyThunk(thunk, hdr_magic)) {
      // Write the function name/ordinal
      if (IsOrdinalThunk(thunk, hdr_magic)) {
        // Import by ordinal
        uint16_t ordinal = GetImportOrdinal(thunk, hdr_magic);
        buf_add(new_imports, 0xFF);  // Mark as ordinal
        buf_addall(new_imports, &ordinal, 2);
      } else {
        // Import by name
        char *name = GetImportName(thunk, pe_image, hdr_magic);
        buf_addall(new_imports, name, strlen(name) + 1);
      }

      // Write the IAT
      uint32_t iat_rva = (uint32_t)((char *)iat - (char *)pe_image) - rva_base;
      WriteCompressedUInt32(new_imports, iat_rva);

      thunk = NextThunk(thunk, hdr_magic);
      iat = NextThunk(iat, hdr_magic);
    }

    // Mark the end of the IATs
    buf_add(new_imports, 0);

    import_descriptors++;
  }

  // Mark the end of the import table
  buf_add(new_imports, 0);

  return true;
}

static void PrintNewImports(uint8_t *p) {
  uint8_t *start = p;
  while (*p) {
    // Read the module name
    char *module_name = (char *)p;
    size_t n = strlen(module_name);
    printf("Module %s\n", module_name);
    p += n + 1;

    // Read the function names and IATs
    while (*p) {
      if (*p == 0xFF) {
        uint16_t ordinal = *(uint16_t *)(p + 1);
        printf("  Ordinal %d\n", ordinal);
        p += 3;
      } else {
        char *name = (char *)p;
        n = strlen(name);
        printf("  Name %s\n", name);
        p += n + 1;
      }

      uint32_t iat_rva = ReadCompressedUInt32(&p);
      printf("  IAT RVA 0x%x\n", iat_rva);
    }
    p++;
  }
  p++;
  printf("Total size %d\n", (int)(p - start));
}

/*==============================================================================
 * Relocation Table
 *============================================================================*/

static void InitializeRelocGroup(buffer_t *relocs, uint8_t type) {
  uint32_t type_and_count = type << 24;
  buf_addall(relocs, &type_and_count, 4);
}

static void AppendRelocGroup(buffer_t *new_relocs, buffer_t *relocs) {
  if (relocs->n > 4) {
    buf_addall(new_relocs, relocs->a, relocs->n);
  }
}

// Write the relocation table in a more compact format
static bool WriteRelocs(buffer_t *new_relocs,
                        PIMAGE_BASE_RELOCATION base_relocs, uint32_t total_size,
                        uint16_t machine, uint32_t rva_base) {
  // Group relocations by type
  buffer_t relocs_highlow = {0};
  InitializeRelocGroup(&relocs_highlow, IMAGE_REL_BASED_HIGHLOW);
  uint32_t relocs_highlow_rva = rva_base;
  buffer_t relocs_dir64 = {0};
  InitializeRelocGroup(&relocs_dir64, IMAGE_REL_BASED_DIR64);
  uint32_t relocs_dir64_rva = rva_base;

  // Walk the relocation table
  while (total_size) {
    uint16_t *reloc = (uint16_t *)(base_relocs + 1);
    uint32_t base_rva = base_relocs->VirtualAddress;
    total_size -= base_relocs->SizeOfBlock;
    base_relocs = (void *)((char *)base_relocs + base_relocs->SizeOfBlock);

    // Walk the relocation block
    while ((void *)reloc != (void *)base_relocs) {
      // Get the relocation type and rva
      uint32_t rva = base_rva + (*reloc & 0xFFF);
      uint8_t type = *reloc >> 12;

      // Check if the relocation type is supported
      buffer_t *relocs = NULL;
      uint32_t *previous_rva = NULL;
      if (type == IMAGE_REL_BASED_HIGHLOW) {
        if (machine == IMAGE_FILE_MACHINE_I386) {
          relocs = &relocs_highlow;
          previous_rva = &relocs_highlow_rva;
        }
      } else if (type == IMAGE_REL_BASED_DIR64) {
        if (machine == IMAGE_FILE_MACHINE_AMD64) {
          relocs = &relocs_dir64;
          previous_rva = &relocs_dir64_rva;
        }
      } else if (type == IMAGE_REL_BASED_ABSOLUTE) {
        goto next;
      }
      if (!relocs || !previous_rva) {
        printf("Not supported reloc type %d\n", type);
        goto err;
      }

      // Write the relocation
      uint32_t delta_rva = rva - *previous_rva;
      *previous_rva = rva;
      WriteCompressedUInt32(relocs, delta_rva);
      (*(uint32_t *)relocs->a)++;

    next:
      reloc++;
    }
  }

  // Combine the grouped relocations
  AppendRelocGroup(new_relocs, &relocs_highlow);
  AppendRelocGroup(new_relocs, &relocs_dir64);
  uint32_t end = 0;
  buf_addall(new_relocs, &end, 4);

  buf_free(&relocs_highlow);
  buf_free(&relocs_dir64);
  return true;

err:
  buf_free(&relocs_highlow);
  buf_free(&relocs_dir64);
  return false;
}

static void PrintNewRelocs(uint8_t *p) {
  while (TRUE) {
    // Read the count and type
    uint32_t count = *(uint32_t *)p;
    p += 4;
    if (!count) {
      break;
    }
    uint8_t type = (uint8_t)(count >> 24);
    count &= 0xFFFFFF;
    printf("Reloc type %d count %d\n", type, count);

    // Read the relocations
    uint32_t rva = 0;
    while (count--) {
      uint32_t delta_rva = ReadCompressedUInt32(&p);
      rva += delta_rva;
      printf("  rva 0x%x\n", rva);
    }
  }
}

/*==============================================================================
 * Main
 *============================================================================*/

// Check if the PE image has C/C++ global variable initialization, TLS
// callbacks, etc.
static bool CheckPEImage(void *pe_image) {
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  uint64_t image_base;
  if (nt_headers->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    image_base = ((PIMAGE_NT_HEADERS32)nt_headers)->OptionalHeader.ImageBase;
  } else {
    image_base = ((PIMAGE_NT_HEADERS64)nt_headers)->OptionalHeader.ImageBase;
  }
  if (image_base != 0) {
    printf("Image base must be 0\n");
    return true;
  }

  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);
  for (uint16_t i = 0; i < nt_headers->FileHeader.NumberOfSections;
       i++, section++) {
    if (strncmp((char *)section->Name, ".CRT", 4) == 0) {
      printf("Global variable initialization is not supported\n");
      return true;
    }
  }

  PIMAGE_DATA_DIRECTORY data_dirs = GetDataDirectory(pe_image);
  if (data_dirs[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress) {
    printf("Static TLS and TLS callbacks are not supported\n");
    return true;
  }

  return false;
}

static bool IsMSVC(void *pe_image) {
  // The MSVC artifacts have a rich header after the dos stub
  return ((PIMAGE_DOS_HEADER)pe_image)->e_lfanew > 0x80;
}

// Copy the .text, .rdata and .data sections into the shellcode
static bool CopySections(buffer_t *shellcode, void *pe_image) {
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);

  // Copy the .text, .rdata and .data sections
  int text = -1;
  int rdata = -1;
  int data = -1;
  for (uint16_t i = 0; i < nt_headers->FileHeader.NumberOfSections;
       i++, section++) {
    // The file layout of the PE image must be consistent with the memory layout
    if (section->VirtualAddress != section->PointerToRawData) {
      printf(
          "VirtualAddress != PointerToRawData (maybe incorrect section "
          "alignment)\n");
      return false;
    }

    // Check the section name
    if (strncmp((char *)section->Name, ".text", 5) == 0) {
      if (text != -1) {
        printf("Multiple .text sections found\n");
        return false;
      }
      text = i;
    } else if (strncmp((char *)section->Name, ".rdata", 6) == 0) {
      if (rdata != -1) {
        printf("Multiple .rdata sections found\n");
        return false;
      }
      rdata = i;
      if (rdata != data + 1 && rdata != text + 1) {
        printf("The .rdata section must be after the .data or .text section\n");
        return false;
      }
    } else if (strncmp((char *)section->Name, ".data", 5) == 0) {
      if (data != -1) {
        printf("Multiple .data sections found\n");
        return false;
      }
      data = i;
      if (data != rdata + 1 && data != text + 1) {
        printf("The .data section must be after the .rdata or .text section\n");
        return false;
      }
    } else {
      continue;
    }

    // Copy the section data
    void *raw_data = (char *)pe_image + section->PointerToRawData;
    buf_addall(shellcode, raw_data, section->SizeOfRawData);
  }

  return true;
}

// Zero import table and import address table
static void ZeroImports(uint8_t *p, void *pe_image) {
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  uint32_t offset_base = IMAGE_FIRST_SECTION(nt_headers)->PointerToRawData;
  bool isMSVC = IsMSVC(pe_image);
  if (isMSVC) {
    // Zero the import table and import address table
    uint16_t hdr_magic = nt_headers->OptionalHeader.Magic;
    uint32_t rva =
        GetDataDirectory(pe_image)[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    uint8_t *data = p + RvaToFileOffset(nt_headers, rva) - offset_base;
    for (PIMAGE_IMPORT_DESCRIPTOR i = (PIMAGE_IMPORT_DESCRIPTOR)data; i->Name;
         i++) {
      // Zero the Name
      data = p + RvaToFileOffset(nt_headers, i->Name) - offset_base;
      memset(data, 0, strlen((char *)data) + 1);

      // Zero the FirstThunk
      if (i->FirstThunk) {
        void **thunk =
            (void **)(p + RvaToFileOffset(nt_headers, i->FirstThunk) -
                      offset_base);
        void **thunk_start = thunk;
        while (!IsEmptyThunk(thunk, hdr_magic)) {
          thunk = NextThunk(thunk, hdr_magic);
        }
        memset(thunk_start, 0, (char *)thunk - (char *)thunk_start);
      }

      // Zero the OriginalFirstThunk
      if (i->OriginalFirstThunk) {
        void **thunk =
            (void **)(p + RvaToFileOffset(nt_headers, i->OriginalFirstThunk) -
                      offset_base);
        void **thunk_start = thunk;
        while (!IsEmptyThunk(thunk, hdr_magic)) {
          thunk = NextThunk(thunk, hdr_magic);
        }
        memset(thunk_start, 0, (char *)thunk - (char *)thunk_start);
      }

      // Zero the IMAGE_IMPORT_DESCRIPTOR
      memset(i, 0, sizeof(IMAGE_IMPORT_DESCRIPTOR));
    }
  } else {
    // Zero the import address table
    PIMAGE_DATA_DIRECTORY iat_dir =
        &GetDataDirectory(pe_image)[IMAGE_DIRECTORY_ENTRY_IAT];
    uint8_t *data =
        p + RvaToFileOffset(nt_headers, iat_dir->VirtualAddress) - offset_base;
    memset(data, 0, iat_dir->Size);
  }
}

// Copy the import table and import address table into the shellcode
static bool CopyImports(buffer_t *shellcode, buffer_t *extra, void *pe_image) {
  buffer_t *new_imports = extra ? extra : shellcode;
  size_t offset = new_imports->n;
  uint32_t size = 0;
  PIMAGE_IMPORT_DESCRIPTOR import_descriptors =
      GetDataDirectoryData(pe_image, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
  if (!import_descriptors) {
    // Mark the end of the import table
    buf_add(new_imports, 0);
    return true;
  }

  // Zero the import table and import address table
  ZeroImports(shellcode->a, pe_image);

  // Copy the import table in a more compact format
  uint32_t rva_base =
      IMAGE_FIRST_SECTION(GetNtHeaders(pe_image))->VirtualAddress;
  WriteImports(new_imports, pe_image, import_descriptors, rva_base);
  // PrintNewImports(new_imports->a + offset);

  return true;
}

// Rebase the absolute addresses
static void FixRelocsImageBase(uint8_t *p, uint8_t *relocs, uint32_t rva_base) {
  while (TRUE) {
    // Read the count and type
    uint32_t count = *(uint32_t *)relocs;
    relocs += 4;
    if (!count) {
      break;
    }
    uint8_t type = (uint8_t)(count >> 24);
    count &= 0xFFFFFF;

    // Read the relocations
    uint32_t rva = 0;
    while (count--) {
      uint32_t delta_rva = ReadCompressedUInt32(&relocs);
      rva += delta_rva;
      switch (type) {
        case IMAGE_REL_BASED_HIGHLOW:
          *(uint32_t *)(p + rva) -= rva_base;
          break;
        case IMAGE_REL_BASED_DIR64:
          *(uint64_t *)(p + rva) -= rva_base;
          break;
      }
    }
  }
}

// Copy the relocation table into the shellcode
static bool CopyRelocs(buffer_t *shellcode, buffer_t *extra, void *pe_image) {
  buffer_t *new_relocs = extra ? extra : shellcode;
  size_t offset = new_relocs->n;
  uint32_t size = 0;
  PIMAGE_BASE_RELOCATION base_relocs =
      GetDataDirectoryData(pe_image, IMAGE_DIRECTORY_ENTRY_BASERELOC, &size);
  if (!base_relocs) {
    // Mark the end of the relocation table
    uint32_t end = 0;
    buf_addall(extra ? extra : shellcode, &end, 4);
    return true;
  }

  // Copy the relocation table in a more compact format
  PIMAGE_NT_HEADERS nt_headers = GetNtHeaders(pe_image);
  uint16_t machine = nt_headers->FileHeader.Machine;
  uint32_t rva_base = IMAGE_FIRST_SECTION(nt_headers)->VirtualAddress;
  WriteRelocs(new_relocs, base_relocs, size, machine, rva_base);
  FixRelocsImageBase(shellcode->a, new_relocs->a + offset, rva_base);
  // printf("Old size vs new size: %d vs %d\n", size,
  //        (uint32_t)(new_relocs->n - offset));
  // PrintNewRelocs(new_relocs->a + offset);

  return true;
}

static bool PatchArguments(buffer_t *shellcode, uint32_t extra_offset) {
  // Find the __imageBase, __extra and call
  uint8_t *p = shellcode->a;
  uint32_t image_base = 0;
  uint32_t extra = 0;
  uint32_t call = 0;
  for (uint32_t i = 0; i < 50; i++) {
    if (*(uint32_t *)(p + i) == 0x11111111) {
      if (image_base) {
        printf("Multiple __imageBase arguments\n");
        return false;
      }
      image_base = i;
    } else if (*(uint32_t *)(p + i) == 0x22222222) {
      if (extra) {
        printf("Multiple __extra arguments\n");
        return false;
      }
      extra = i;
    } else if (image_base && extra && p[i] == 0xE8) {
      uint32_t offset = *(uint32_t *)(p + i + 1);
      offset += i + 5;
      if (offset > shellcode->n) {
        // Maybe not a call
        continue;
      }
      call = i;
      break;
    }
  }
  if (!image_base || !extra || !call) {
    printf("Can't find __imageBase, __extra or call\n");
    return false;
  }

  // Patch the __imageBase and __extra
  uint32_t retaddr = call + 5;
  *(uint32_t *)(p + image_base) = retaddr;
  *(uint32_t *)(p + extra) = extra_offset - retaddr;
  return true;
}

/*
File layout:

MSVC:
  .text
  .rdata
    +IAT
    +.rdata
    +IT (We can't move this to the end to save space)
  .data
  .pdata (x64)
  ...
  .reloc

GCC:
  .text:
    +.text
    +IAT
    +.rdata
    +.data
  .idata
    +IT
  ...
  .reloc
*/

static bool c2sc(buffer_t *shellcode, buffer_t *extra, void *pe_image,
                 uint32_t *extra_offset) {
  *extra_offset = 0;

  // Check the dos and nt headers
  if (!GetNtHeaders(pe_image) || !GetDataDirectory(pe_image)) {
    printf("Invalid PE image\n");
    return false;
  }

  // Check if the PE image has unsupported global stuff
  if (CheckPEImage(pe_image)) {
    return false;
  }

  // printf("Is MSVC: %d\n", IsMSVC(pe_image));

  // Copy the .text, .rdata and .data sections
  if (!CopySections(shellcode, pe_image)) {
    printf("Can't copy .text/.rdata.data sections\n");
    return false;
  }

  // One byte initialization mark
  if (extra) {
    *extra_offset = 0;
    buf_add(extra, 0);
  } else {
    *extra_offset = (uint32_t)shellcode->n;
    buf_add(shellcode, 0);
  }

  // Copy the import table and import address table
  if (!CopyImports(shellcode, extra, pe_image)) {
    printf("Can't copy imports\n");
    return false;
  }

  // Copy the relocation table
  if (!CopyRelocs(shellcode, extra, pe_image)) {
    printf("Can't copy relocations\n");
    return false;
  }

  // Patch 'Initialize' arguments
  if (!extra && !PatchArguments(shellcode, *extra_offset)) {
    printf("Can't patch arguments\n");
    return false;
  }

  return true;
}

#ifdef C2SC_TESTS
#define __Locator() 0
#define __DPRINTF(...) printf(__VA_ARGS__)
#define ReadCompressedUInt32 __ReadCompressedUInt32
#include "../src/startup.c"
#endif

static void GetOutputName(const char *input_name, char *output_name,
                          char *variable_name, bool save_header) {
  // Get file name without extension
  const char *t = GetFileName(input_name);
  char name[128] = {0};
  strncpy(name, t, 96);
  RemoveExtension(name);

  // Copy the name and convert it to a valid C variable name
  size_t len = strlen(name);
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (c == '.') {
      output_name[i] = variable_name[i] = '_';
    } else {
      variable_name[i] = toupper(c);
      output_name[i] = tolower(c);
    }
  }
  output_name[len] = variable_name[len] = 0;

  // Add the extension
  const char *ext = save_header ? ".h" : ".bin";
  strcat(output_name, ext);
}

static void FormatCArray(buffer_t *output, buffer_t *input,
                         const char *variable_name) {
  buf_sprintf(output, "unsigned char %s[%d] = {", variable_name, (int)input->n);
  for (uint32_t i = 0; i < input->n; i++) {
    if ((i % 12) == 0) {
      buf_sprintf(output, "\n  ");
    }
    buf_sprintf(output, "0x%02x, ", input->a[i]);
  }
  buf_sprintf(output, "\n};\n");
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s [modifiers] file1 file2 ... fileN\n", argv[0]);
    printf("  Modifiers:\n");
    printf("    b: Save as binary file\n");
    printf("    h: Save as C/C++ header file\n");
    printf("    s: Save imports and relocations separately\n");
    return 0;
  }

  // Parse the modifiers
  bool save_binary = false;
  bool save_header = false;
  bool standalone_extra = false;
  const char *modifiers = argv[1];
  for (int i = 0; modifiers[i]; i++) {
    switch (modifiers[i]) {
      case 'b':
        save_binary = true;
        break;
      case 'h':
        save_header = true;
        break;
      case 's':
        standalone_extra = true;
        break;
      default:
        printf("Unknown modifier %c\n", modifiers[i]);
        return 0;
    }
  }

  // Process the PE images
  char *output_name = (char *)malloc(128);
  char *variable_name = (char *)malloc(128);
  buffer_t shellcode = {0};
  buffer_t extra = {0};
  buffer_t header = {0};
  for (int i = 2; i < argc; i++) {
    // Read the PE image
    void *pe_image = ReadFileBytes(argv[i], NULL);
    if (!pe_image) {
      printf("Can't read file %s\n", argv[i]);
      continue;
    }

    // Convert the PE image to shellcode
    uint32_t extra_offset = 0;
    buffer_t *opt_extra = standalone_extra ? &extra : NULL;
    if (!c2sc(&shellcode, opt_extra, pe_image, &extra_offset)) {
      printf("Can't convert %s\n", argv[i]);
      free(pe_image);
      continue;
    }
    printf("%s (shellcode size: %d, extra size: %d)\n", GetFileName(argv[i]),
           (int)shellcode.n, (int)extra.n);

    // Tests
#ifdef C2SC_TESTS
    if (!standalone_extra) {
      PVOID imageBase = VirtualAlloc(
          NULL, shellcode.n, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
      memcpy(imageBase, shellcode.a, shellcode.n);
      Initialize(0 - (SIZE_T)imageBase, (SIZE_T)imageBase + extra_offset);
      ((void (*)(void *))imageBase)(NULL);
      VirtualFree(imageBase, 0, MEM_RELEASE);
    }
#endif

    // Save the binary file
    if (save_binary) {
      GetOutputName(argv[i], output_name, variable_name, false);
      if (!WriteFileBytes(output_name, shellcode.a, shellcode.n)) {
        printf("Can't write file %s\n", output_name);
      }
      if (standalone_extra) {
        output_name[strlen(output_name) - 4] = 0;
        strcat(output_name, "_extra.bin");
        if (!WriteFileBytes(output_name, extra.a, extra.n)) {
          printf("Can't write file %s\n", output_name);
        }
      }
    }

    // Save the C/C++ header file
    if (save_header) {
      GetOutputName(argv[i], output_name, variable_name, true);
      buf_sprintf(&header, "#pragma once\n\n");
      FormatCArray(&header, &shellcode, variable_name);
      if (standalone_extra) {
        strcat(variable_name, "_EXTRA");
        buf_sprintf(&header, "\n");
        FormatCArray(&header, &extra, variable_name);
      }
      if (!WriteFileBytes(output_name, header.a, header.n)) {
        printf("Can't write file %s\n", output_name);
      }
      buf_reset(&header);
    }

    free(pe_image);
    buf_reset(&extra);
    buf_reset(&shellcode);
  }
  buf_free(&extra);
  buf_free(&shellcode);
  free(output_name);
  free(variable_name);
  return 0;
}
