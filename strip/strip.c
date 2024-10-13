#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#ifdef _DEBUG
#define tracef printf
#else
#define tracef
#endif // _DEBUG

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

#define starts_with(s, prefix) (strncmp((s), (prefix), strlen(prefix)) == 0)
#define ends_with(s, suffix)                                                   \
  (strlen(s) >= strlen(suffix) &&                                              \
   strcmp((s) + strlen(s) - strlen(suffix), (suffix)) == 0)

bool strip(const char *filename) {
  size_t size = 0;
  void *data = readfile(filename, &size);
  if (!data)
    return false;

  IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)data;
  tracef("COFF Header: Machine: 0x%04x, Sections: %u, Symbols: %u\n",
         coff_header->Machine, coff_header->NumberOfSections,
         coff_header->NumberOfSymbols);
  int section_count = coff_header->NumberOfSections;
  IMAGE_SECTION_HEADER *section_headers =
      (IMAGE_SECTION_HEADER *)((char *)data + sizeof(IMAGE_FILE_HEADER));

  for (int i = 0; i < section_count; i++) {
    IMAGE_SECTION_HEADER *section_header = &section_headers[i];
    char *section_name = (char *)section_header->Name;
    uint32_t section_offset = (uint32_t)section_header->PointerToRawData;
    uint32_t section_size = (uint32_t)section_header->SizeOfRawData;
    tracef("Section Header %d: Name: %.8s, Offset: %u, Size: %u\n", i,
           section_name, section_offset, section_size);

    if (section_offset >= size || section_size > size ||
        (size_t)section_offset + section_size > size) {
      tracef("Section overflow.");
      continue;
    }

    if (starts_with(section_name, ".debug")) {
      printf("Zeroing section %.8s.\n", section_name);
      // memset((char *)data + section_offset, 0, section_size);
      char *p = (char *)data + section_offset;
      for (uint32_t j = 0; j < section_size; j++) {
        if (!ends_with(p + j, filename))
          continue;
        memset(p + j, 0, strlen(p + j));
      }
    }
  }

  bool b = writefile(filename, data, size);
  free(data);
  return b;
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    const char *filename = argv[i];
    printf("Stripping %s\n", filename);
    bool b = strip(filename);
    printf(b ? "Success\n" : "Failure\n");
  }
  return 0;
}
