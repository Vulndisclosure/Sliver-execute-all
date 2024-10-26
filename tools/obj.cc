#include "obj.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>

/*==============================================================================
 * ObjectFile
 *============================================================================*/

bool ObjectFile::Load(const void *data) {
  if (!data)
    return false;

  // Parse the COFF header
  auto coff_header = reinterpret_cast<const IMAGE_FILE_HEADER *>(data);
  header_ = *coff_header;

  // Parse the sections
  auto p = reinterpret_cast<const uint8_t *>(data);
  auto section_headers = reinterpret_cast<const IMAGE_SECTION_HEADER *>(
      p + sizeof(IMAGE_FILE_HEADER));
  size_t section_count = header_.NumberOfSections;
  sections_.resize(section_count);
  for (size_t i = 0; i < section_count; i++) {
    const IMAGE_SECTION_HEADER *section_header = &section_headers[i];
    auto name = reinterpret_cast<const char *>(section_header->Name);
    auto &section = sections_[i];
    section.file_ = this;
    section.header_ = *section_header;
    section.name_.assign(name, strnlen(name, IMAGE_SIZEOF_SHORT_NAME));
    section.offset_ = section_header->PointerToRawData;
    if (section_header->SizeOfRawData &&
        !(section_header->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)) {
      auto t = reinterpret_cast<const uint8_t *>(data) +
               section_header->PointerToRawData;
      section.data_.assign(t, t + section_header->SizeOfRawData);
    }
    if (section_header->NumberOfRelocations) {
      auto t = reinterpret_cast<const IMAGE_RELOCATION *>(
          p + section_header->PointerToRelocations);
      section.relocs_.assign(t, t + section_header->NumberOfRelocations);
    }
  }

  // Parse the symbol table
  auto symbols = reinterpret_cast<const IMAGE_SYMBOL *>(
      p + coff_header->PointerToSymbolTable);
  auto strings =
      reinterpret_cast<const char *>(symbols + header_.NumberOfSymbols);
  auto symbol_count = (uint32_t)header_.NumberOfSymbols;
  for (uint32_t i = 0; i < symbol_count; i++) {
    auto sym = &symbols[i];
    auto &symbol = symbols_.emplace_back();
    symbol.file_ = this;
    symbol.symbol_ = *sym;
    if (sym->N.Name.Short == 0)
      symbol.name_.assign(strings + sym->N.Name.Long);
    else {
      auto t = reinterpret_cast<const char *>(sym->N.ShortName);
      symbol.name_.assign(t, strnlen(t, IMAGE_SIZEOF_SHORT_NAME));
    }
    for (uint32_t j = 0; j < sym->NumberOfAuxSymbols; j++)
      symbol.aux_symbols_.push_back(
          *reinterpret_cast<const IMAGE_AUX_SYMBOL *>(&symbols[i + j + 1]));
    i += sym->NumberOfAuxSymbols;
  }

  // Parse the string table
  strings_.assign(strings, strings + *(uint32_t *)strings);

  return true;
}

bool ObjectFile::Save(std::vector<uint8_t> &data) {
  // Calculate the size of the object file
  size_t sections_size = 0;
  size_t relocs_size = 0;
  for (auto &&section : sections_) {
    if (!section.data_.empty())
      sections_size =
          std::max(section.offset_ + section.data_.size(), sections_size);
    relocs_size += section.relocs_.size() * sizeof(IMAGE_RELOCATION);
  }
  size_t symbols_size = 0;
  for (auto &&symbol : symbols_)
    symbols_size += sizeof(IMAGE_SYMBOL) +
                    symbol.aux_symbols_.size() * sizeof(IMAGE_AUX_SYMBOL);
  size_t size = sections_size + relocs_size + symbols_size + strings_.size();

  // Allocate memory for the object file
  data.clear();
  data.resize(size);

  // Copy the COFF header
  auto header = reinterpret_cast<IMAGE_FILE_HEADER *>(data.data());
  *header = header_;

  // Copy the sections
  size_t relocs_offset = sections_size;
  header->NumberOfSections = (WORD)sections_.size();
  auto section_headers = reinterpret_cast<IMAGE_SECTION_HEADER *>(
      data.data() + sizeof(IMAGE_FILE_HEADER));
  size_t index = 0;
  for (auto &&section : sections_) {
    auto section_header = &section_headers[index];
    *section_header = section.header_;
    auto &data2 = section.data_;
    if (!data2.empty()) {
      auto t = data.data() + section.offset_;
      std::copy(data2.begin(), data2.end(), t);
      section_header->PointerToRawData = (DWORD)section.offset_;
      section_header->SizeOfRawData = (DWORD)data2.size();
    }
    auto &relocs = section.relocs_;
    if (!relocs.empty()) {
      auto t =
          reinterpret_cast<IMAGE_RELOCATION *>(data.data() + relocs_offset);
      std::copy(relocs.begin(), relocs.end(), t);
      section_header->PointerToRelocations = (DWORD)relocs_offset;
      section_header->NumberOfRelocations = (WORD)relocs.size();
      relocs_offset += relocs.size() * sizeof(IMAGE_RELOCATION);
    }
    index++;
  }
  assert(relocs_offset < data.size());

  // Copy the symbol table
  size_t symbols_offset = relocs_offset;
  assert(symbols_offset + symbols_size < data.size());
  auto symbols = reinterpret_cast<IMAGE_SYMBOL *>(data.data() + symbols_offset);
  index = 0;
  for (auto &&symbol : symbols_) {
    symbols[index++] = symbol.symbol_;
    for (auto &&aux_symbol : symbol.aux_symbols_)
      symbols[index++] = *reinterpret_cast<IMAGE_SYMBOL *>(&aux_symbol);
  }
  header->PointerToSymbolTable = (DWORD)symbols_offset;
  header->NumberOfSymbols = (DWORD)index;

  // Copy the string table
  size_t strings_offset = symbols_offset + symbols_size;
  assert(strings_offset + strings_.size() == data.size());
  auto strings = data.data() + strings_offset;
  std::copy(strings_.begin(), strings_.end(), strings);
  *(uint32_t *)strings = (uint32_t)strings_.size();

  return true;
}

ObjectSection *ObjectFile::FindSection(std::string_view name) {
  for (auto &&section : sections_) {
    if (section.name_ == name)
      return &section;
  }
  return nullptr;
}

ObjectSymbol *ObjectFile::FindSymbol(std::string_view name) {
  for (auto &&symbol : symbols_) {
    if (symbol.name_ == name)
      return &symbol;
  }
  return nullptr;
}

ObjectSection &ObjectFile::AddSection(std::string_view name) {
  auto &section = sections_.emplace_back();
  section.file_ = this;
  size_t name_size = std::min(name.size(), (size_t)IMAGE_SIZEOF_SHORT_NAME);
  std::copy_n(name.begin(), name_size, section.header_.Name);
  section.name_ = name.substr(0, name_size);
  return section;
}

ObjectSection &ObjectFile::AddSection(std::string_view name,
                                      std::span<const uint8_t> data) {
  auto &section = AddSection(name);
  section.AddData(data);
  return section;
}

ObjectSymbol &ObjectFile::AddSymbol(std::string_view name) {
  auto &symbol = symbols_.emplace_back();
  symbol.file_ = this;
  symbol.SetName(name);
  return symbol;
}

ObjectSymbol &ObjectFile::AddSymbol(std::string_view name, uint32_t value,
                                    uint32_t section, uint16_t type,
                                    uint8_t storage) {
  auto &symbol = AddSymbol(name);
  symbol.symbol_.Value = value;
  symbol.symbol_.SectionNumber = (SHORT)section;
  symbol.symbol_.Type = type;
  symbol.symbol_.StorageClass = storage;
  return symbol;
}

ObjectSymbol &ObjectFile::AddSymbol(std::string_view name,
                                    ObjectSection &section,
                                    std::span<const uint8_t> data,
                                    uint16_t type, uint8_t storage) {
  return AddSymbol(name, (DWORD)section.AddData(data),
                   (uint16_t)section.index() + 1, type, storage);
}

size_t ObjectFile::AddString(std::string_view s) {
  size_t offset = strings_.size();
  strings_.insert(strings_.end(), s.begin(), s.end());
  strings_.push_back('\0');
  return offset;
}

/*==============================================================================
 * ObjectSection
 *============================================================================*/

size_t ObjectSection::Resize(size_t new_size) {
  size_t old_size = data_.size();
  if (new_size == old_size)
    return old_size;

  // Resize the section
  size_t diff_size = (size_t)(new_size - old_size);
  data_.resize(new_size);
  if ((intptr_t)diff_size > 0) {
    int value = header_.Characteristics & IMAGE_SCN_MEM_EXECUTE ? 0xCC : 0;
    std::fill_n(data_.data() + old_size, diff_size, value);
  }

  // Update the offsets of other sections
  for (auto &&section : file_->sections_) {
    if (section.offset_ > offset_)
      section.offset_ += diff_size;
  }

  return old_size;
}

size_t ObjectSection::AddData(std::span<const uint8_t> data) {
  size_t offset = Resize(data_.size() + data.size());
  std::copy(data.begin(), data.end(), data_.data() + offset);
  return offset;
}

IMAGE_RELOCATION &ObjectSection::AddReloc(uint32_t offset, uint32_t symbol,
                                          uint16_t type) {
  auto &reloc = relocs_.emplace_back(IMAGE_RELOCATION{
      .VirtualAddress = offset,
      .SymbolTableIndex = symbol,
      .Type = type,
  });
  return reloc;
}

/*==============================================================================
 * ObjectSymbol
 *============================================================================*/

size_t ObjectSymbol::GetRealIndex() const {
  size_t index = 0;
  for (auto &&symbol : file_->symbols_) {
    if (&symbol == this)
      return index;
    index += (size_t)symbol.symbol_.NumberOfAuxSymbols + 1;
  }
  assert(false);
  return index;
}

void ObjectSymbol::SetName(std::string_view name) {
  name_ = name;
  if (name.size() <= IMAGE_SIZEOF_SHORT_NAME) {
    std::fill_n(symbol_.N.ShortName, IMAGE_SIZEOF_SHORT_NAME, 0);
    std::copy(name.begin(), name.end(),
              reinterpret_cast<char *>(symbol_.N.ShortName));
  } else {
    symbol_.N.Name.Short = 0;
    symbol_.N.Name.Long = (DWORD)file_->AddString(name);
  }
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
