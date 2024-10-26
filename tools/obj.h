#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define _PUSHPOP_SUPPORTED
#define UNALIGNED
#define DUMMYUNIONNAME
#define DUMMYSTRUCTNAME
#include "pe.h"
#endif

class ObjectSection;
class ObjectSymbol;
class ObjectFile;

class ObjectSection {
public:
  ObjectFile *file_;
  IMAGE_SECTION_HEADER header_;
  std::string name_;
  size_t offset_;
  std::vector<uint8_t> data_;
  std::vector<IMAGE_RELOCATION> relocs_;

  ObjectSection() : file_(nullptr), header_{}, offset_(0) {}
  ~ObjectSection() = default;

  ObjectSection(const ObjectSection &) = delete;
  ObjectSection &operator=(const ObjectSection &) = delete;
  ObjectSection(ObjectSection &&) = default;
  ObjectSection &operator=(ObjectSection &&) = default;

  inline size_t index() const;

  size_t Resize(size_t new_size);
  size_t AddData(std::span<const uint8_t> data);
  IMAGE_RELOCATION &AddReloc(uint32_t offset, uint32_t symbol, uint16_t type);
};

class ObjectSymbol {
public:
  ObjectFile *file_;
  IMAGE_SYMBOL symbol_;
  std::vector<IMAGE_AUX_SYMBOL> aux_symbols_;
  std::string name_;

  ObjectSymbol() : file_(nullptr), symbol_{} {}
  ~ObjectSymbol() = default;

  ObjectSymbol(const ObjectSymbol &) = delete;
  ObjectSymbol &operator=(const ObjectSymbol &) = delete;
  ObjectSymbol(ObjectSymbol &&) = default;
  ObjectSymbol &operator=(ObjectSymbol &&) = default;

  inline size_t index() const;

  size_t GetRealIndex() const;
  void SetName(std::string_view name);
};

class ObjectFile {
public:
  IMAGE_FILE_HEADER header_;
  std::vector<ObjectSection> sections_;
  std::vector<ObjectSymbol> symbols_;
  std::vector<char> strings_;

  ObjectFile() : header_{} {}
  ~ObjectFile() = default;

  ObjectFile(const ObjectFile &) = delete;
  ObjectFile &operator=(const ObjectFile &) = delete;
  ObjectFile(ObjectFile &&) = default;
  ObjectFile &operator=(ObjectFile &&) = default;

  bool Load(const void *data);
  bool Load(std::span<const uint8_t> data) { return Load(data.data()); }
  bool Save(std::vector<uint8_t> &data);

  ObjectSection *FindSection(std::string_view name);
  ObjectSymbol *FindSymbol(std::string_view name);

  ObjectSection &AddSection(std::string_view name);
  ObjectSection &AddSection(std::string_view name,
                            std::span<const uint8_t> data);
  ObjectSymbol &AddSymbol(std::string_view name);
  ObjectSymbol &AddSymbol(std::string_view name, uint32_t value,
                          uint32_t section, uint16_t type, uint8_t storage);
  ObjectSymbol &AddSymbol(std::string_view name, ObjectSection &section,
                          std::span<const uint8_t> data, uint16_t type,
                          uint8_t storage);
  size_t AddString(std::string_view s);
};

inline size_t ObjectSection::index() const {
  return this - file_->sections_.data();
}

inline size_t ObjectSymbol::index() const {
  return this - file_->symbols_.data();
}
