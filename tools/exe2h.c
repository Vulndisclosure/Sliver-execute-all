#include <ctype.h>
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

const char *GetFileName(const char *path) {
  const char *name = strrchr(path, '\\');
  if (!name) {
    name = strrchr(path, '/');
  }
  if (name) {
    return name + 1;
  }
  return path;
}

char *exe2h(void *pe_image, const char *variable_name) {
  PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)pe_image;
  if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
    return NULL;
  }

  PIMAGE_NT_HEADERS nt_headers =
      (PIMAGE_NT_HEADERS)((char *)pe_image + dos_header->e_lfanew);
  if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return NULL;
  }

  // Check the relocation table
  PIMAGE_DATA_DIRECTORY reloc_directory = NULL;
  if (nt_headers->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    reloc_directory =
        &((PIMAGE_NT_HEADERS32)nt_headers)
             ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
  else
    reloc_directory =
        &((PIMAGE_NT_HEADERS64)nt_headers)
             ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
  if (reloc_directory->VirtualAddress) {
    printf("The .text section contains relocations\n");
    return NULL;
  }

  // Find the .text section
  PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(nt_headers);
  PIMAGE_SECTION_HEADER text_section = NULL;
  for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
    if (strncmp((char *)sections[i].Name, ".text", 8) == 0) {
      text_section = &sections[i];
      break;
    }
  }
  if (!text_section) {
    return NULL;
  }
  uint32_t text_offset = (uint32_t)text_section->PointerToRawData;
  uint32_t text_size = (uint32_t)text_section->Misc.VirtualSize;
  if (!text_offset || !text_size) {
    return NULL;
  }
  uint8_t *text_data = (uint8_t *)pe_image + text_offset;

  // Convert the .text section to a C array
  size_t buffer_size = (size_t)text_size * 8 + 200;
  char *buffer = (char *)malloc(buffer_size);
  if (!buffer) {
    return NULL;
  }
  char *p = buffer;
  size_t c = 0;
  p += snprintf(p, buffer_size - (p - buffer),
                "#pragma once\n\nunsigned char %s[] = {", variable_name);
  for (uint32_t i = 0; i < text_size; i++) {
    if (!(i % 12)) p += snprintf(p, buffer_size - (p - buffer), "\n  ");
    p += snprintf(p, buffer_size - (p - buffer), "0x%02x, ", text_data[i]);
  }
  p += snprintf(p, buffer_size - (p - buffer), "\n};\n");
  return buffer;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s file1 file2 ... fileN\n", argv[0]);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    // Read the PE image
    void *pe_image = ReadFileBytes(argv[i], NULL);
    if (!pe_image) {
      printf("Can't read file %s\n", argv[i]);
      continue;
    }

    // Get the PE image name
    char variable_name[128] = {0};
    char file_name[128] = {0};
    const char *pe_image_name = GetFileName(argv[i]);
    for (int i = 0; pe_image_name[i] != 0 && i < 96; i++) {
      char c = pe_image_name[i];
      if (c == '.') {
        variable_name[i] = file_name[i] = '_';
      } else {
        variable_name[i] = toupper(c);
        file_name[i] = tolower(c);
      }
    }
    strcat(file_name, ".h");

    // Convert the PE image to a C array
    char *buffer = exe2h(pe_image, variable_name);
    free(pe_image);
    if (!buffer) {
      printf("Can't convert file %s\n", argv[i]);
      continue;
    }

    // Save the C array to a header file
    WriteFileBytes(file_name, buffer, strlen(buffer));
    printf("Extracted .text section from %s to %s\n", argv[i], file_name);
    free(buffer);
  }
  return 0;
}
