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
#define _PUSHPOP_SUPPORTED
#define UNALIGNED
#define DUMMYUNIONNAME
#define DUMMYSTRUCTNAME
#include "pe.h"
#endif

static void *ReadFileBytes(const char *file_name, size_t *size) {
  if (!file_name)
    return NULL;

  FILE *file = NULL;
  size_t file_size = 0;
  void *data = NULL;

  // Open the file
  file = fopen(file_name, "rb");
  if (!file)
    goto err;
  fseek(file, 0, SEEK_END);
  file_size = ftell(file);
  if (file_size == -1)
    goto err;
  rewind(file);

  // Read the file
  data = malloc(file_size);
  if (!data || fread(data, 1, file_size, file) != file_size)
    goto err;

  if (size)
    *size = file_size;
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

static bool WriteFileBytes(const char *file_name, void *data, size_t size) {
  if (!file_name || !data || size <= 0)
    return false;

  FILE *file = NULL;
  size_t written = 0;

  // Open the file
  file = fopen(file_name, "wb");
  if (!file)
    return false;

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
  if (!name)
    name = strrchr(path, '/');
  if (name)
    return name + 1;
  return path;
}

#define starts_with(s, prefix) (strncmp((s), (prefix), strlen(prefix)) == 0)
#define ends_with(s, suffix)                                                   \
  (strlen(s) >= strlen(suffix) &&                                              \
   strcmp((s) + strlen(s) - strlen(suffix), (suffix)) == 0)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s file1 file2 ... fileN\n", argv[0]);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    // Read the object file
    size_t obj_size = 0;
    void *obj = ReadFileBytes(argv[i], &obj_size);
    if (!obj) {
      printf("Can't read file %s\n", argv[i]);
      continue;
    }

    // Strip
    const char *obj_name = GetFileName(argv[i]);
    IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)obj;
    IMAGE_SECTION_HEADER *sections =
        (IMAGE_SECTION_HEADER *)((char *)obj + sizeof(IMAGE_FILE_HEADER));
    int section_count = coff_header->NumberOfSections;
    for (int i = 0; i < section_count; i++) {
      char *section_name = (char *)sections[i].Name;
      uint32_t section_offset = (uint32_t)sections[i].PointerToRawData;
      uint32_t section_size = (uint32_t)sections[i].SizeOfRawData;
      if (starts_with(section_name, ".debug")) {
        printf("Removing original path from section %.8s\n", section_name);
        // memset((char *)obj + section_offset, 0, section_size);
        char *p = (char *)obj + section_offset;
        for (uint32_t j = 0; j < section_size; j++) {
          if (!ends_with(p + j, obj_name))
            continue;
          memset(p + j, 0, strlen(p + j));
        }
      }
    }

    // Save the object file
    WriteFileBytes(argv[i], obj, obj_size);
    free(obj);
  }
  return 0;
}
