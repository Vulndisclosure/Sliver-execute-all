#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "pe.h"
#endif

class ObjectFile;
class ObjectSection;
class ObjectSymbol;
class ObjectRelocation;

// Represents a COFF object file.
class ObjectFile {
  IMAGE_FILE_HEADER header_;
  std::vector<std::unique_ptr<ObjectSection>> sections_;
  std::vector<std::unique_ptr<ObjectSymbol>> symbols_;
  std::vector<char> strings_;

 public:
  ObjectFile() : header_{} {}
  ~ObjectFile() = default;

  ObjectFile(const ObjectFile &) = delete;
  ObjectFile &operator=(const ObjectFile &) = delete;
  ObjectFile(ObjectFile &&) = default;
  ObjectFile &operator=(ObjectFile &&) = default;

  // Returns the COFF file header.
  const IMAGE_FILE_HEADER &header() const { return header_; }

  // Gets/sets the machine type of the object file.
  uint16_t machine() const { return header_.Machine; }
  void set_machine(uint16_t machine) { header_.Machine = machine; }

  // Gets/sets the characteristics of the object file.
  uint16_t characteristics() const { return header_.Characteristics; }
  void set_characteristics(uint16_t characteristics) {
    header_.Characteristics = characteristics;
  }

  // Returns the sections in the object file. Use AddSection to add a new
  // section.
  std::vector<std::unique_ptr<ObjectSection>> &sections() { return sections_; }
  const std::vector<std::unique_ptr<ObjectSection>> &sections() const {
    return sections_;
  }

  // Returns the symbols in the object file. Use AddSymbol to add a new symbol.
  std::vector<std::unique_ptr<ObjectSymbol>> &symbols() { return symbols_; }
  const std::vector<std::unique_ptr<ObjectSymbol>> &symbols() const {
    return symbols_;
  }

  // Returns the string table.
  std::vector<char> &strings() { return strings_; }
  const std::vector<char> &strings() const { return strings_; }

  // Loads an object file from memory.
  bool Load(const void *data);

  // Loads an object file from memory.
  bool Load(std::span<const uint8_t> data) { return Load(data.data()); }

  // Saves the object file to memory.
  bool Save(std::vector<uint8_t> &data);

  // Finds a section by name.
  ObjectSection *FindSection(std::string_view name);

  // Finds a symbol by name.
  ObjectSymbol *FindSymbol(std::string_view name);

  // Finds a symbol by real index.
  ObjectSymbol *FindSymbol(size_t real_index);

  // Adds a new section to the object file.
  ObjectSection &AddSection(std::string_view name);
  ObjectSection &AddSection(std::string_view name,
                            std::span<const uint8_t> data);

  // Adds a new symbol to the object file.
  ObjectSymbol &AddSymbol(std::string_view name);
  ObjectSymbol &AddSymbol(std::string_view name, uint32_t value,
                          int16_t section, uint16_t type, uint8_t storage);
  ObjectSymbol &AddSymbol(std::string_view name, ObjectSection &section,
                          std::span<const uint8_t> data, uint16_t type,
                          uint8_t storage);

  // Adds a new string to the string table.
  size_t AddString(std::string_view s);
};

// Represents a section in a COFF object file.
class ObjectSection {
  friend class ObjectFile;

  ObjectFile *file_;
  size_t index_;
  IMAGE_SECTION_HEADER header_;
  std::string name_;
  std::vector<uint8_t> data_;
  std::vector<std::unique_ptr<ObjectRelocation>> relocs_;

 public:
  ObjectSection() : file_(nullptr), index_(0), header_{} {}
  ObjectSection(ObjectFile *file, size_t index,
                const IMAGE_SECTION_HEADER &header, std::string &&name,
                std::vector<uint8_t> &&data,
                std::vector<std::unique_ptr<ObjectRelocation>> &&relocs)
      : file_(file),
        index_(index),
        header_(header),
        name_(std::move(name)),
        data_(std::move(data)),
        relocs_(std::move(relocs)) {}
  ~ObjectSection() = default;

  ObjectSection(const ObjectSection &) = delete;
  ObjectSection &operator=(const ObjectSection &) = delete;
  ObjectSection(ObjectSection &&) = default;
  ObjectSection &operator=(ObjectSection &&) = default;

  // Returns the object file that contains the section.
  ObjectFile *file() { return file_; }
  const ObjectFile *file() const { return file_; }

  // Returns the index of the section.
  size_t index() const { return index_; }

  // Returns the section header.
  const IMAGE_SECTION_HEADER &header() const { return header_; }

  // Gets/sets the offset of the section in the object file.
  uint32_t offset() const { return header_.PointerToRawData; }
  void set_offset(uint32_t offset) { header_.PointerToRawData = offset; }

  // Gets/sets the characteristics of the section.
  uint32_t characteristics() const { return header_.Characteristics; }
  void set_characteristics(uint32_t characteristics) {
    header_.Characteristics = characteristics;
  }

  // Returns the name of the section.
  const std::string &name() const { return name_; }

  // Returns the raw data of the section.
  std::vector<uint8_t> &data() { return data_; }
  const std::vector<uint8_t> &data() const { return data_; }

  // Returns the size of the section.
  uint32_t size() const { return (uint32_t)data_.size(); }

  // Returns the relocations of the section.
  const std::vector<std::unique_ptr<ObjectRelocation>> &relocs() const {
    return relocs_;
  }

  // Returns the count of relocations.
  uint16_t reloc_count() const { return (uint16_t)relocs_.size(); }

  // Resizes the raw data of the section to a new size and returns the old size.
  size_t Resize(size_t new_size);

  // Adds the specified data to the section and returns the offset.
  size_t AddData(std::span<const uint8_t> data);

  // Adds a relocation to the section.
  ObjectRelocation &AddReloc(uint32_t offset, uint32_t symbol, uint16_t type);
  ObjectRelocation &AddReloc(uint32_t offset, const ObjectSymbol &symbol,
                             uint16_t type);
};

// Represents a symbol in a COFF object file.
class ObjectSymbol {
  friend class ObjectFile;

  ObjectFile *file_;
  size_t index_;
  size_t real_index_;
  IMAGE_SYMBOL symbol_;
  std::vector<IMAGE_AUX_SYMBOL> aux_symbols_;
  std::string name_;

 public:
  ObjectSymbol() : file_(nullptr), index_(0), real_index_(0), symbol_{} {}
  ObjectSymbol(ObjectFile *file, size_t index, size_t real_index,
               const IMAGE_SYMBOL &symbol,
               std::vector<IMAGE_AUX_SYMBOL> &&aux_symbols, std::string &&name)
      : file_(file),
        index_(index),
        real_index_(real_index),
        symbol_(symbol),
        aux_symbols_(std::move(aux_symbols)),
        name_(std::move(name)) {}
  ~ObjectSymbol() = default;

  ObjectSymbol(const ObjectSymbol &) = delete;
  ObjectSymbol &operator=(const ObjectSymbol &) = delete;
  ObjectSymbol(ObjectSymbol &&) = default;
  ObjectSymbol &operator=(ObjectSymbol &&) = default;

  // Returns the object file that contains the symbol.
  ObjectFile *file() { return file_; }
  const ObjectFile *file() const { return file_; }

  // Returns the index of the symbol in the symbol table.
  size_t index() const { return index_; }

  // Returns the real index of the symbol in the symbol table. (including
  // auxiliary symbols)
  size_t real_index() const { return real_index_; }

  // Returns the  real index of the next symbol in the symbol table. (including
  // auxiliary symbols)
  size_t next_real_index() const {
    return real_index_ + 1 + aux_symbols_.size();
  }

  // Returns the raw symbol.
  const IMAGE_SYMBOL &symbol() const { return symbol_; }

  // Gets/sets the value of the symbol.
  uint32_t value() const { return symbol_.Value; }
  void set_value(uint32_t value) { symbol_.Value = value; }

  // Gets/sets the section that contains the symbol.
  ObjectSection *section() {
    return symbol_.SectionNumber > IMAGE_SYM_UNDEFINED
               ? file_->sections()[symbol_.SectionNumber - 1].get()
               : nullptr;
  }
  const ObjectSection *section() const {
    return const_cast<ObjectSymbol *>(this)->section();
  }
  void set_section(const ObjectSection &section) {
    symbol_.SectionNumber = (SHORT)(USHORT)(section.index() + 1);
  }
  void set_section(int16_t section_number) {
    symbol_.SectionNumber = section_number;
  }

  // Gets/sets the type of the symbol.
  uint16_t type() const { return symbol_.Type; }
  void set_type(uint16_t type) { symbol_.Type = type; }

  // Gets/sets the storage class of the symbol.
  uint8_t storage() const { return symbol_.StorageClass; }
  void set_storage(uint8_t storage) { symbol_.StorageClass = storage; }

  // Returns the auxiliary symbols.
  const std::vector<IMAGE_AUX_SYMBOL> &aux_symbols() const {
    return aux_symbols_;
  }

  // Returns the name of the symbol. Use UpdateName to update the name.
  const std::string &name() const { return name_; }

  // Updates the data of the symbol.
  void UpdateData(ObjectSection &section, std::span<const uint8_t> data);

  // Updates the name of the symbol.
  void UpdateName(std::string_view name);
};

// Represents a relocation in a COFF object file.
class ObjectRelocation {
  friend class ObjectFile;
  friend class ObjectSection;

  ObjectSection *section_;
  size_t index_;
  IMAGE_RELOCATION reloc_;

 public:
  ObjectRelocation() : section_(nullptr), index_(0), reloc_{} {}
  ObjectRelocation(ObjectSection *section, size_t index,
                   const IMAGE_RELOCATION &reloc)
      : section_(section), index_(index), reloc_(reloc) {}
  ~ObjectRelocation() = default;

  ObjectRelocation(const ObjectRelocation &) = delete;
  ObjectRelocation &operator=(const ObjectRelocation &) = delete;
  ObjectRelocation(ObjectRelocation &&) = default;
  ObjectRelocation &operator=(ObjectRelocation &&) = default;

  // Returns the section that contains the relocation.
  ObjectSection *section() { return section_; }
  const ObjectSection *section() const { return section_; }

  // Returns the index of the relocation in the relocation table.
  size_t index() const { return index_; }

  // Returns the raw relocation.
  const IMAGE_RELOCATION &reloc() const { return reloc_; }

  // Gets/sets the offset in the section that the relocation refers to.
  uint32_t offset() const { return reloc_.VirtualAddress; }
  void set_offset(uint32_t offset) { reloc_.VirtualAddress = offset; }

  // Gets/sets the symbol that the relocation refers to.
  ObjectSymbol *symbol() {
    return section_->file()->FindSymbol(reloc_.SymbolTableIndex);
  }
  const ObjectSymbol *symbol() const {
    return const_cast<ObjectRelocation *>(this)->symbol();
  }
  void set_symbol(const ObjectSymbol &symbol) {
    reloc_.SymbolTableIndex = (DWORD)symbol.real_index();
  }

  // Gets/sets the relocation type.
  uint16_t type() const { return reloc_.Type; }
  void set_type(uint16_t type) { reloc_.Type = type; }
};
