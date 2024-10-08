#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#define _PUSHPOP_SUPPORTED
#define UNALIGNED
#define DUMMYUNIONNAME
#define DUMMYSTRUCTNAME
#include "pe.h"
#endif

#ifdef _DEBUG
#define tracef printf
#else
#define tracef
#endif // _DEBUG

// MSVC
static const char chkstk_name[] = "__chkstk";
static const uint8_t chkstk_32[43] = {
    0x51, 0x8D, 0x4C, 0x24, 0x04, 0x2B, 0xC8, 0x1B, 0xC0, 0xF7, 0xD0,
    0x23, 0xC8, 0x8B, 0xC4, 0x25, 0x00, 0xF0, 0xFF, 0xFF, 0x3B, 0xC8,
    0x72, 0x0A, 0x8B, 0xC1, 0x59, 0x94, 0x8B, 0x00, 0x89, 0x04, 0x24,
    0xC3, 0x2D, 0x00, 0x10, 0x00, 0x00, 0x85, 0x00, 0xEB, 0xE9,
};
static const uint8_t chkstk_64[78] = {
    0x48, 0x83, 0xEC, 0x10, 0x4C, 0x89, 0x14, 0x24, 0x4C, 0x89, 0x5C, 0x24,
    0x08, 0x4D, 0x33, 0xDB, 0x4C, 0x8D, 0x54, 0x24, 0x18, 0x4C, 0x2B, 0xD0,
    0x4D, 0x0F, 0x42, 0xD3, 0x65, 0x4C, 0x8B, 0x1C, 0x25, 0x10, 0x00, 0x00,
    0x00, 0x4D, 0x3B, 0xD3, 0x73, 0x16, 0x66, 0x41, 0x81, 0xE2, 0x00, 0xF0,
    0x4D, 0x8D, 0x9B, 0x00, 0xF0, 0xFF, 0xFF, 0x41, 0xC6, 0x03, 0x00, 0x4D,
    0x3B, 0xD3, 0x75, 0xF0, 0x4C, 0x8B, 0x14, 0x24, 0x4C, 0x8B, 0x5C, 0x24,
    0x08, 0x48, 0x83, 0xC4, 0x10, 0xC3,
};

// GCC
static const char chkstk_ms_name[] = "___chkstk_ms";
static const uint8_t chkstk_ms_32[42] = {
    0x51, 0x50, 0x3D, 0x00, 0x10, 0x00, 0x00, 0x8D, 0x4C, 0x24, 0x0C,
    0x72, 0x15, 0x81, 0xE9, 0x00, 0x10, 0x00, 0x00, 0x83, 0x09, 0x00,
    0x2D, 0x00, 0x10, 0x00, 0x00, 0x3D, 0x00, 0x10, 0x00, 0x00, 0x77,
    0xEB, 0x29, 0xC1, 0x83, 0x09, 0x00, 0x58, 0x59, 0xC3,
};
static const uint8_t chkstk_ms_64[50] = {
    0x51, 0x50, 0x48, 0x3D, 0x00, 0x10, 0x00, 0x00, 0x48, 0x8D,
    0x4C, 0x24, 0x18, 0x72, 0x19, 0x48, 0x81, 0xE9, 0x00, 0x10,
    0x00, 0x00, 0x48, 0x83, 0x09, 0x00, 0x48, 0x2D, 0x00, 0x10,
    0x00, 0x00, 0x48, 0x3D, 0x00, 0x10, 0x00, 0x00, 0x77, 0xE7,
    0x48, 0x29, 0xC1, 0x48, 0x83, 0x09, 0x00, 0x58, 0x59, 0xC3,
};

const char *preferred_modules[] = {
    "kernel32.dll", "kernelbase.dll", "advapi32.dll", "bcrypt.dll",
    "cabinet.dll",  "crypt32.dll",    "cryptdll.dll", "dnsapi.dll",
    "fltlib.dll",   "mpr.dll",        "msxml2.dll",   "ncrypt.dll",
    "netapi32.dll", "ntdsapi.dll",    "odbc32.dll",   "ole32.dll",
    "oleaut32.dll", "rpcrt4.dll",     "shlwapi.dll",  "samlib.dll",
    "secur32.dll",  "shell32.dll",    "user32.dll",   "userenv.dll",
    "version.dll",  "hid.dll",        "setupapi.dll", "winscard.dll",
    "winsta.dll",   "wbemuuid.dll",   "wldap32.dll",  "wtsapi32.dll",
    "msasn1.dll",   "msvcrt.dll",     "ntdll.dll",    NULL,
};

#define OBJECT_FILE_SECTION_MAX 128

typedef struct {
  IMAGE_SECTION_HEADER header;
  size_t index;
  char *name;
  size_t offset;
  void *data;
  size_t data_size; // Don't modify this field
  IMAGE_RELOCATION *relocs;
  size_t reloc_count;
} object_section;

typedef struct {
  IMAGE_FILE_HEADER coff_header;
  object_section sections[OBJECT_FILE_SECTION_MAX];
  size_t section_count;
  IMAGE_SYMBOL *symbols;
  size_t symbol_count;
  char *strings;
  size_t strings_size;
} object_file;

typedef enum { CompilerUnknown, CompilerMSVC, CompilerGCC } Compiler;

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

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

static bool writefile(const char *filename, void *data, size_t size) {
  if (!filename || !data || size <= 0)
    return false;

  FILE *file = NULL;
  size_t written = 0;

  // Open the file
  file = fopen(filename, "wb");
  if (!file) {
    printf("Can't open file for writing: %s\n", filename);
    return false;
  }

  // Write the data
  written = fwrite(data, 1, size, file);
  if (written != size) {
    printf("Failed to write full data to file: %s\n", filename);
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

const char *getfilename(const char *path) {
  const char *filename = strrchr(path, '\\');
  if (!filename)
    filename = strrchr(path, '/');
  if (filename)
    return filename + 1;
  return path;
}

void change_extension(char *filename, const char *extension) {
  char *dot = strrchr(filename, '.');
  if (dot)
    strcpy(dot, ".obj");
  else
    strcat(filename, ".obj");
}

#define starts_with(s, prefix) (strncmp((s), (prefix), strlen(prefix)) == 0)
#define ends_with(s, suffix)                                                   \
  (strlen(s) >= strlen(suffix) &&                                              \
   strcmp((s) + strlen(s) - strlen(suffix), (suffix)) == 0)

void *safe_malloc(size_t n) {
  void *p = malloc(n);
  if (!p)
    abort();
  return p;
}

void *safe_realloc(void *p, size_t n) {
  void *q = realloc(p, n);
  if (!q)
    abort();
  return q;
}

char *safe_strdup(const char *s) {
  char *p = strdup(s);
  if (!p)
    abort();
  return p;
}

bool parse_obj(const void *objdata, object_file *obj) {
  if (!objdata || !obj)
    return false;

  memset(obj, 0, sizeof(object_file));

  // Parse the COFF header
  IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)objdata;
  obj->coff_header = *coff_header;
  tracef("COFF Header: Machine: 0x%04x, Sections: %u, Symbols: %u\n",
         coff_header->Machine, coff_header->NumberOfSections,
         (uint32_t)coff_header->NumberOfSymbols);

  // Parse the sections
  size_t size;
  IMAGE_SECTION_HEADER *sections =
      (IMAGE_SECTION_HEADER *)((char *)objdata + sizeof(IMAGE_FILE_HEADER));
  if (coff_header->NumberOfSections > OBJECT_FILE_SECTION_MAX) {
    printf("Too many sections: %u\n", coff_header->NumberOfSections);
    return false;
  }
  obj->section_count = coff_header->NumberOfSections;
  for (size_t i = 0; i < obj->section_count; i++) {
    IMAGE_SECTION_HEADER *hdr = &sections[i];
    char *name = (char *)hdr->Name;
    object_section *sct = &obj->sections[i];
    sct->header = *hdr;
    sct->index = i;
    size = strnlen(name, IMAGE_SIZEOF_SHORT_NAME) + 1;
    sct->name = safe_malloc(size);
    strncpy(sct->name, name, size);
    sct->offset = hdr->PointerToRawData;
    if (hdr->SizeOfRawData &&
        !(hdr->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)) {
      sct->data_size = hdr->SizeOfRawData;
      sct->data = safe_malloc(hdr->SizeOfRawData);
      void *data = (char *)objdata + hdr->PointerToRawData;
      memcpy(sct->data, data, hdr->SizeOfRawData);
    }
    if (hdr->NumberOfRelocations) {
      sct->reloc_count = hdr->NumberOfRelocations;
      size = hdr->NumberOfRelocations * sizeof(IMAGE_RELOCATION);
      sct->relocs = safe_malloc(size);
      memcpy(sct->relocs, (char *)objdata + hdr->PointerToRelocations, size);
    }
    tracef("Section %.8s: Offset: %u, Size: %u, Relocs: %u\n", name,
           hdr->PointerToRawData, hdr->SizeOfRawData, hdr->NumberOfRelocations);
  }

  // Parse the symbol table
  IMAGE_SYMBOL *symbols =
      (IMAGE_SYMBOL *)((char *)objdata + coff_header->PointerToSymbolTable);
  uint32_t symbol_count = (uint32_t)coff_header->NumberOfSymbols;
  obj->symbol_count = symbol_count;
  size = symbol_count * sizeof(IMAGE_SYMBOL);
  obj->symbols = (IMAGE_SYMBOL *)safe_malloc(size);
  memcpy(obj->symbols, symbols, size);

  // Parse the string table
  char *string_table = (char *)symbols + symbol_count * sizeof(IMAGE_SYMBOL);
  size = *(uint32_t *)string_table;
  obj->strings_size = size;
  obj->strings = (char *)safe_malloc(size);
  memcpy(obj->strings, string_table, size);

  return true;
}

bool save_obj(const object_file *obj, void **objdata, size_t *objsize) {
  if (!obj || !objdata || !objsize)
    return false;

  // Calculate the size of the object file
  *objsize = 0;
  size_t relocs_offset = 0;
  size_t relocs_size = 0;
  for (size_t i = 0; i < obj->section_count; i++) {
    if (obj->sections[i].data)
      *objsize =
          max(obj->sections[i].offset + obj->sections[i].data_size, *objsize);
    relocs_size += obj->sections[i].reloc_count * sizeof(IMAGE_RELOCATION);
  }
  relocs_offset = *objsize;
  *objsize += relocs_size + obj->symbol_count * sizeof(IMAGE_SYMBOL) +
              obj->strings_size;

  // Allocate memory for the object file
  *objdata = safe_malloc(*objsize);
  memset(*objdata, 0, *objsize);

  // Copy the COFF header
  IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)*objdata;
  memcpy(coff_header, &obj->coff_header, sizeof(IMAGE_FILE_HEADER));

  // Copy the sections
  IMAGE_SECTION_HEADER *section_headers =
      (IMAGE_SECTION_HEADER *)((char *)*objdata + sizeof(IMAGE_FILE_HEADER));
  for (size_t i = 0; i < obj->section_count; i++) {
    const object_section *sct = &obj->sections[i];
    memcpy(&section_headers[i], &sct->header, sizeof(IMAGE_SECTION_HEADER));
    if (sct->data) {
      void *data = (char *)*objdata + sct->offset;
      memcpy(data, sct->data, sct->data_size);
      section_headers[i].PointerToRawData = (DWORD)sct->offset;
      section_headers[i].SizeOfRawData = (DWORD)sct->data_size;
    }
    if (sct->relocs) {
      void *relocs = (char *)*objdata + relocs_offset;
      memcpy(relocs, sct->relocs, sct->reloc_count * sizeof(IMAGE_RELOCATION));
      section_headers[i].PointerToRelocations = (DWORD)relocs_offset;
      section_headers[i].NumberOfRelocations = (WORD)sct->reloc_count;
      relocs_offset += sct->reloc_count * sizeof(IMAGE_RELOCATION);
    }
  }

  // Copy the symbol table
  size_t symbols_offset = relocs_offset;
  coff_header->PointerToSymbolTable = (DWORD)symbols_offset;
  coff_header->NumberOfSymbols = (DWORD)obj->symbol_count;
  IMAGE_SYMBOL *symbols = (IMAGE_SYMBOL *)((char *)*objdata + symbols_offset);
  memcpy(symbols, obj->symbols, obj->symbol_count * sizeof(IMAGE_SYMBOL));

  // Copy the string table
  size_t string_table_offset =
      symbols_offset + obj->symbol_count * sizeof(IMAGE_SYMBOL);
  memcpy((char *)*objdata + string_table_offset, obj->strings,
         obj->strings_size);
  *(uint32_t *)((char *)*objdata + string_table_offset) =
      (uint32_t)obj->strings_size;
  assert(string_table_offset + obj->strings_size == *objsize);

  return true;
}

void free_obj(object_file *obj) {
  if (!obj)
    return;

  for (size_t i = 0; i < obj->section_count; i++) {
    object_section *section = &obj->sections[i];
    if (section->name)
      free(section->name);
    if (section->data)
      free(section->data);
    if (section->relocs)
      free(section->relocs);
  }
  if (obj->symbols)
    free(obj->symbols);
  if (obj->strings)
    free(obj->strings);
  memset(obj, 0, sizeof(object_file));
}

char *get_symbol_name(const object_file *obj, IMAGE_SYMBOL *symbol) {
  static char shortname[9] = {0};
  if (symbol->N.Name.Short == 0)
    return obj->strings + symbol->N.Name.Long;
  if (symbol->N.ShortName[7] == 0)
    return (char *)symbol->N.ShortName;
  memcpy(shortname, symbol->N.ShortName, 8);
  return shortname;
}

IMAGE_SYMBOL *find_symbol(const object_file *obj, const char *name,
                          size_t *index) {
  if (!obj || !name)
    return NULL;
  for (size_t i = 0; i < obj->symbol_count; i++) {
    IMAGE_SYMBOL *symbol = &obj->symbols[i];
    char *symbol_name = get_symbol_name(obj, symbol);
    if (strcmp(symbol_name, name) == 0) {
      if (index)
        *index = i;
      return symbol;
    }
    i += symbol->NumberOfAuxSymbols;
  }
  return NULL;
}

void resize_section(object_file *obj, object_section *changed_section,
                    size_t new_size) {
  if (!obj || !changed_section || new_size == changed_section->data_size ||
      !changed_section->data)
    return;

  // Resize the section
  int32_t diff_size = (int32_t)(new_size - changed_section->data_size);
  changed_section->data = safe_realloc(changed_section->data, new_size);
  if (diff_size > 0) {
    int value = changed_section->header.Characteristics & IMAGE_SCN_MEM_EXECUTE
                    ? 0xCC
                    : 0;
    memset((char *)changed_section->data + changed_section->data_size, value,
           diff_size);
  }
  changed_section->data_size = new_size;
  changed_section->header.SizeOfRawData = (DWORD)new_size;

  // Update other sections
  for (size_t i = 0; i < obj->section_count; i++) {
    object_section *section = &obj->sections[i];
    if (section->offset > changed_section->offset)
      section->offset += diff_size;
  }

  /*
  llvm-16.0.4/lld/COFF/Chunks.cpp:

  void SectionChunk::applyRelX64(uint8_t *off, uint16_t type, OutputSection *os,
                                 uint64_t s, uint64_t p,
                                 uint64_t imageBase) const {
    switch (type) {
    case IMAGE_REL_AMD64_ADDR32:
      add32(off, s + imageBase);
      break;
    case IMAGE_REL_AMD64_ADDR64:
      add64(off, s + imageBase);
      break;
    case IMAGE_REL_AMD64_ADDR32NB: add32(off, s); break;
    case IMAGE_REL_AMD64_REL32:    add32(off, s - p - 4); break;
    case IMAGE_REL_AMD64_REL32_1:  add32(off, s - p - 5); break;
    case IMAGE_REL_AMD64_REL32_2:  add32(off, s - p - 6); break;
    case IMAGE_REL_AMD64_REL32_3:  add32(off, s - p - 7); break;
    case IMAGE_REL_AMD64_REL32_4:  add32(off, s - p - 8); break;
    case IMAGE_REL_AMD64_REL32_5:  add32(off, s - p - 9); break;
    case IMAGE_REL_AMD64_SECTION:
      applySecIdx(off, os, file->ctx.outputSections.size());
      break;
    case IMAGE_REL_AMD64_SECREL:   applySecRel(this, off, os, s); break;
    default:
      error("unsupported relocation type 0x" + Twine::utohexstr(type) + " in " +
            toString(file));
    }
  }

  void SectionChunk::applyRelX86(uint8_t *off, uint16_t type, OutputSection *os,
                                 uint64_t s, uint64_t p,
                                 uint64_t imageBase) const {
    switch (type) {
    case IMAGE_REL_I386_ABSOLUTE: break;
    case IMAGE_REL_I386_DIR32:
      add32(off, s + imageBase);
      break;
    case IMAGE_REL_I386_DIR32NB:  add32(off, s); break;
    case IMAGE_REL_I386_REL32:    add32(off, s - p - 4); break;
    case IMAGE_REL_I386_SECTION:
      applySecIdx(off, os, file->ctx.outputSections.size());
      break;
    case IMAGE_REL_I386_SECREL:   applySecRel(this, off, os, s); break;
    default:
      error("unsupported relocation type 0x" + Twine::utohexstr(type) + " in " +
            toString(file));
    }
  }
  */

  /*
  // Update the relocations
  for (size_t i = 0; i < obj->section_count; i++) {
    object_section *reloc_section = &obj->sections[i];
    for (size_t j = 0; j < reloc_section->reloc_count; j++) {
      IMAGE_RELOCATION *reloc = &reloc_section->relocs[j];

      IMAGE_SYMBOL *target = &obj->symbols[reloc->SymbolTableIndex];
      if (target->SectionNumber <= IMAGE_SYM_UNDEFINED)
        continue; // Skip undefined symbols

      object_section *target_section =
          &obj->sections[target->SectionNumber - 1];
      if (target_section <= changed_section)
        continue; // Skip sections before the changed section

      switch (obj->coff_header.Machine) {
      case IMAGE_FILE_MACHINE_I386:
        switch (reloc->Type) {
          break;
        case IMAGE_REL_I386_DIR32:
        case IMAGE_REL_I386_DIR32NB:
        case IMAGE_REL_I386_REL32:
        case IMAGE_REL_I386_SECREL:
          *(int32_t *)((char *)reloc_section->data + reloc->VirtualAddress) +=
              diff_size;
          break;
        case IMAGE_REL_I386_ABSOLUTE:
        case IMAGE_REL_I386_SECTION:
          break;
        default:
          printf("unsupported relocation type 0x%04x in %s\n", reloc->Type,
                 get_symbol_name(obj, target));
        }
        break;
      case IMAGE_FILE_MACHINE_AMD64:
        switch (reloc->Type) {
        case IMAGE_REL_AMD64_ADDR32:
        case IMAGE_REL_AMD64_ADDR32NB:
        case IMAGE_REL_AMD64_REL32:
        case IMAGE_REL_AMD64_REL32_1:
        case IMAGE_REL_AMD64_REL32_2:
        case IMAGE_REL_AMD64_REL32_3:
        case IMAGE_REL_AMD64_REL32_4:
        case IMAGE_REL_AMD64_REL32_5:
        case IMAGE_REL_AMD64_SECREL:
          *(int32_t *)((char *)reloc_section->data + reloc->VirtualAddress) +=
              diff_size;
          break;
        case IMAGE_REL_AMD64_ADDR64:
          *(int64_t *)((char *)reloc_section->data + reloc->VirtualAddress) +=
              diff_size;
          break;
        case IMAGE_REL_AMD64_ABSOLUTE:
        case IMAGE_REL_AMD64_SECTION:
          break;
        default:
          printf("unsupported relocation type 0x%04x in %s\n", reloc->Type,
                 get_symbol_name(obj, target));
        }
        break;
      }
    }
  }
  */
}

Compiler get_compiler(const object_file *obj) {
  for (size_t i = 0; i < obj->section_count; i++) {
    const object_section *section = &obj->sections[i];
    if (strcmp(section->name, ".drectve") == 0)
      return CompilerMSVC;
  }
  return CompilerGCC;
}

bool remove_fileinfo(const object_file *obj, const char *objpath) {
  // Remove the source file path
  Compiler compiler = get_compiler(obj);
  switch (compiler) {
  case CompilerMSVC: {
    char objname[256] = {0};
    strncpy(objname, getfilename(objpath), sizeof(objname) - 5);
    change_extension(objname, ".obj");
    for (size_t i = 0; i < obj->section_count; i++) {
      const object_section *section = &obj->sections[i];
      if (starts_with(section->name, ".debug")) {
        char *p = (char *)section->data;
        for (size_t j = 0; j < section->data_size; j++) {
          if (ends_with(p + j, objname)) {
            printf("Remove original obj path %s.\n", p + j);
            memset(p + j, 0, strlen(p + j));
            return true;
          }
        }
      }
    }
    return false;
  }
  case CompilerGCC:
    return false;
  default:
    printf("Unsupported compiler.\n");
    return false;
  }
}

bool get_chkstk_patch_info(Compiler compiler, uint16_t machine,
                           const void **data, size_t *size) {
  if (compiler == CompilerMSVC) {
    if (machine == IMAGE_FILE_MACHINE_I386) {
      *data = chkstk_32;
      *size = sizeof(chkstk_32);
      return true;
    } else if (machine == IMAGE_FILE_MACHINE_AMD64) {
      *data = chkstk_64;
      *size = sizeof(chkstk_64);
      return true;
    }
  } else if (compiler == CompilerGCC) {
    if (machine == IMAGE_FILE_MACHINE_I386) {
      *data = chkstk_ms_32;
      *size = sizeof(chkstk_ms_32);
      return true;
    } else if (machine == IMAGE_FILE_MACHINE_AMD64) {
      *data = chkstk_ms_64;
      *size = sizeof(chkstk_ms_64);
      return true;
    }
  }
  return false;
}

bool inject_chkstk(object_file *obj, object_section *text_section) {
  // Check if the __chkstk exists
  Compiler compiler = get_compiler(obj);
  if (compiler == CompilerUnknown) {
    printf("Unsupported compiler.\n");
    return false;
  }
  const char *name = compiler == CompilerMSVC ? chkstk_name : chkstk_ms_name;
  size_t index = 0;
  IMAGE_SYMBOL *chkstk = find_symbol(obj, name, &index);
  if (!chkstk) {
    tracef("Didn't find %s. Skip injection.\n", name);
    return false;
  }
  if (chkstk->SectionNumber > IMAGE_SYM_UNDEFINED) {
    tracef("%s is already defined in section %s.\n", name,
           obj->sections[chkstk->SectionNumber - 1].name);
    return false;
  }
  tracef("Found %s at %zu\n", name, index);

  // Inject the chkstk
  const void *data = NULL;
  size_t size = 0;
  if (!get_chkstk_patch_info(compiler, obj->coff_header.Machine, &data,
                             &size)) {
    printf("Can't get chkstk patch info.\n");
    return false;
  }
  chkstk->Value = (DWORD)text_section->data_size;
  chkstk->SectionNumber = (SHORT)(USHORT)(text_section->index + 1);
  chkstk->Type = IMAGE_SYM_DTYPE_FUNCTION << N_BTSHFT | IMAGE_SYM_TYPE_NULL;
  chkstk->StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
  resize_section(obj, text_section, text_section->data_size + size);
  memcpy((char *)text_section->data + chkstk->Value, data, size);

  return true;
}

const char **resolve_func(const char *apis, const char *func) {
  if (!apis || !func)
    return NULL;

  const char empty[] = "";
  const char *module_begin = empty, *module_end = empty;
  const char *line = apis;
  const char *next_line = NULL;

  // A buffer to hold the module names that match the function
  const char **matching_modules = NULL;
  size_t module_count = 0;
  size_t buffer_size = 10;

  while (line) {
    // Find the end of the current line
    next_line = strchr(line, '\n');
    if (!next_line) {
      // Handle last line if it doesn't end with '\n'
      next_line = line + strlen(line);
    }

    const char *begin = line;
    const char *end = next_line;
    if (begin == end)
      goto next; // Empty line, skip it

    // Check if it's a module line (e.g., [aaclient.dll])
    if (*begin == '[' && *(end - 1) == ']') {
      module_begin = begin + 1;
      module_end = end - 1;
      goto next;
    }

    // Check if it's a function line (e.g., 1=LoadClientAdapter)
    const char *equal_sign = strchr(begin, '=');
    if (equal_sign) {
      const char *func_name_begin = equal_sign + 1;
      const char *func_name_end = end;

      // Compare func_name with the passed 'func'
      if (strncmp(func_name_begin, func, func_name_end - func_name_begin) ==
              0 &&
          strlen(func) == (size_t)(func_name_end - func_name_begin)) {
        // If match, copy the module name to the result list
        size_t module_len = module_end - module_begin;
        char *module_name =
            (char *)safe_malloc((module_len + 1) * sizeof(char));
        strncpy(module_name, module_begin, module_len);
        module_name[module_len] = '\0'; // Null terminate

        // Add the module to the list
        if (!matching_modules)
          matching_modules =
              (const char **)safe_malloc(buffer_size * sizeof(char *));
        if (module_count >= buffer_size) {
          // Resize if necessary
          buffer_size *= 2;
          matching_modules = (const char **)safe_realloc(
              matching_modules, buffer_size * sizeof(char *));
        }
        if (matching_modules)
          matching_modules[module_count++] = module_name;
      }
    }

  next:
    // Move to the next line
    line = *next_line ? next_line + 1 : NULL;
  }

  // Null-terminate the result list
  if (matching_modules)
    matching_modules[module_count] = NULL;

  return matching_modules;
}

const char *pick_module(const char **preferred_modules, const char **modules) {
  if (!preferred_modules || !modules)
    return NULL;

  for (size_t i = 0; preferred_modules[i]; i++) {
    for (size_t j = 0; modules[j]; j++) {
      if (strcmp(preferred_modules[i], modules[j]) == 0)
        return modules[j];
    }
  }

  return modules[0];
}

size_t auto_import(object_file *obj, object_section *text_section,
                   const char *apis_x86, const char *apis_x64) {
  size_t count = 0;
  IMAGE_SYMBOL *symbol = NULL;
  for (size_t i = 0; i < obj->symbol_count;
       i += (size_t)symbol->NumberOfAuxSymbols + 1) {
    symbol = &obj->symbols[i];
    if (symbol->SectionNumber > IMAGE_SYM_UNDEFINED ||
        symbol->StorageClass != IMAGE_SYM_CLASS_EXTERNAL ||
        strchr(get_symbol_name(obj, symbol), '$'))
      continue; // Skip defined symbols

    // Demangle the symbol name
    char *name = safe_strdup(get_symbol_name(obj, symbol));
    char *name_ = name;
    char *original_name = safe_strdup(name);
    bool is_import = false;
    if (starts_with(name, "__imp_")) {
      is_import = true;
      name += 6;
    }
    bool is_cdecl = false;
    bool is_stdcall = false;
    if (obj->coff_header.Machine == IMAGE_FILE_MACHINE_I386) {
      if (starts_with(name, "_")) {
        is_cdecl = true;
        name += 1;
      }

      char *t;
      if (is_cdecl && (t = strchr(name, '@'))) {
        *t = '\0';
        is_cdecl = false;
        is_stdcall = true;
      }
    }
    bool is_beacon_api =
        starts_with(name, "Beacon") || strcmp(name, "toWideChar") == 0;
    tracef("Symbol: %s, Import: %d, Cdecl: %d, Stdcall: %d, Beacon: %d\n", name,
           is_import, is_cdecl, is_stdcall, is_beacon_api);

    // Don't process beacon APIs
    if (is_beacon_api) {
      tracef("Skiped beacon API: %s\n", original_name);
      goto next;
    }

    // Resolve the module name
    const char *apis = obj->coff_header.Machine == IMAGE_FILE_MACHINE_I386
                           ? apis_x86
                           : apis_x64;
    const char **modules = resolve_func(apis, name);
    if (!modules) {
      printf("Can't resolve module for %s\n", name);
      goto next;
    }
    const char *module = pick_module(preferred_modules, modules);
    printf("Resolved modules for %s: ", original_name);
    for (size_t j = 0; modules[j]; j++) {
      printf(j != 0 ? ", %s" : "%s", modules[j]);
      if (modules[j] == module)
        printf(" (picked)");
    }
    printf("\n");

    // Create the new import name
    size_t new_import_name_size = 6 + strlen(module) - 4 + 1 + strlen(name) + 1;
    size_t new_import_name_offset = obj->strings_size;
    obj->strings = (char *)safe_realloc(obj->strings, obj->strings_size +
                                                          new_import_name_size);
    obj->strings_size += new_import_name_size;
    char *new_import_name = obj->strings + new_import_name_offset;
    snprintf(new_import_name, new_import_name_size, "__imp_%.*s$%s",
             (uint32_t)strlen(module) - 4, module, name);

    if (is_import) {
      // Update the symbol name
      symbol->N.Name.Short = 0;
      symbol->N.Name.Long = (DWORD)new_import_name_offset;
    } else {
      // Add the import thunk to the .text section
      symbol->Value = (DWORD)text_section->data_size;
      symbol->SectionNumber = (SHORT)(USHORT)(text_section->index + 1);
      resize_section(obj, text_section, text_section->data_size + 6);
      uint8_t *thunk = (uint8_t *)text_section->data + symbol->Value;
      thunk[0] = 0xFF;
      thunk[1] = 0x25;
      *(int32_t *)(thunk + 2) = 0;

      // Add the import symbol
      obj->symbols = (IMAGE_SYMBOL *)safe_realloc(
          obj->symbols, (obj->symbol_count + 1) * sizeof(IMAGE_SYMBOL));
      size_t import_symbol_index = obj->symbol_count++;
      IMAGE_SYMBOL *import_symbol = &obj->symbols[import_symbol_index];
      import_symbol->N.Name.Short = 0;
      import_symbol->N.Name.Long = (DWORD)new_import_name_offset;
      import_symbol->Value = 0;
      import_symbol->SectionNumber = IMAGE_SYM_UNDEFINED;
      import_symbol->Type = IMAGE_SYM_TYPE_NULL;
      import_symbol->StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
      import_symbol->NumberOfAuxSymbols = 0;

      // Reloc for the import thunk
      text_section->relocs = (IMAGE_RELOCATION *)safe_realloc(
          text_section->relocs,
          (text_section->reloc_count + 1) * sizeof(IMAGE_RELOCATION));
      IMAGE_RELOCATION *thunk_reloc =
          &text_section->relocs[text_section->reloc_count++];
      thunk_reloc->VirtualAddress = symbol->Value + 2;
      thunk_reloc->SymbolTableIndex = (WORD)import_symbol_index;
      thunk_reloc->Type = obj->coff_header.Machine == IMAGE_FILE_MACHINE_I386
                              ? IMAGE_REL_I386_DIR32
                              : IMAGE_REL_AMD64_REL32;

      printf("Added import symbol %s for %s at %zu\n", new_import_name,
             original_name, import_symbol_index);
    }
    count++;

  next:
    free(name_);
    free(original_name);
  }

  return count;
}

bool fix(const char *apis_x86, const char *apis_x64, const char *filename) {
  // Parse the object file
  size_t size = 0;
  void *data = readfile(filename, &size);
  if (!data) {
    printf("Can't read file: %s\n", filename);
    return false;
  }
  object_file obj = {0};
  bool b = parse_obj(data, &obj);
  free(data);
  if (!b) {
    printf("Can't parse object file: %s\n", filename);
    return false;
  }

  // Find the .text section
  object_section *text_section = NULL;
  for (size_t i = 0; i < obj.section_count; i++) {
    if (obj.sections[i].header.Characteristics & IMAGE_SCN_MEM_EXECUTE) {
      text_section = &obj.sections[i];
      break;
    }
  }
  if (!text_section) {
    printf("Can't find .text section in %s\n", filename);
    free_obj(&obj);
    return false;
  }
  size_t text_section_size = text_section->data_size;

  // Fix the object file
  remove_fileinfo(&obj, filename);
  inject_chkstk(&obj, text_section);
  auto_import(&obj, text_section, apis_x86, apis_x64);
  if (text_section->data_size != text_section_size) {
    // Align up to 0x10 bytes
    size_t new_size = (text_section->data_size + 0xF) & ~0xF;
    resize_section(&obj, text_section, new_size);
    printf("Resized .text section from 0x%zx to 0x%zx\n", text_section_size,
           new_size);
  }

  // Save the object file
  save_obj(&obj, &data, &size);
  free_obj(&obj);
  b = writefile(filename, data, size);
  free(data);
  return b;
}

int main(int argc, char **argv) {
  // argc = 4;
  // char *argv_[] = {
  //     "fixbof.exe",
  //     "..\\..\\fixbof\\apis_6.0.6000_x86.txt",
  //     "..\\..\\fixbof\\apis_6.0.6000_x64.txt",
  //     "..\\..\\bin\\mybof.x86.o",
  // };
  // argv = argv_;
  if (argc < 3) {
    printf("Usage: %s <apis_x86.txt> <apis_x64.txt> <file1> [file2] ...\n",
           argv[0]);
    return 1;
  }
  const char *apis_x86_filename = argv[1];
  const char *apis_x64_filename = argv[2];
  char *apis_x86 = (char *)readfile(apis_x86_filename, NULL);
  char *apis_x64 = (char *)readfile(apis_x64_filename, NULL);
  for (int i = 3; i < argc; i++) {
    const char *filename = argv[i];
    printf("Fixing %s\n", filename);
    bool b = fix(apis_x86, apis_x64, filename);
    printf(b ? "Success\n" : "Failure\n");
  }
  return 0;
}
