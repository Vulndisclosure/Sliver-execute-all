#include "obj.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>

/*==============================================================================
 * ObjectFile
 *============================================================================*/

bool ObjectFile::Load(const void *data) {
  if (!data) {
    return false;
  }

  // Parse the COFF header
  auto coff_header = reinterpret_cast<const IMAGE_FILE_HEADER *>(data);
  header_ = *coff_header;

  // Parse the sections
  auto p = reinterpret_cast<const uint8_t *>(data);
  auto raw_sections = reinterpret_cast<const IMAGE_SECTION_HEADER *>(
      p + sizeof(IMAGE_FILE_HEADER));
  size_t section_count = header_.NumberOfSections;
  sections_.resize(section_count);
  for (size_t i = 0; i < section_count; i++) {
    // Parse the section name
    const IMAGE_SECTION_HEADER *raw_section = &raw_sections[i];
    auto raw_name = reinterpret_cast<const char *>(raw_section->Name);
    // TODO: name can also be a pointer to a string in the string table
    std::string name(raw_name, strnlen(raw_name, IMAGE_SIZEOF_SHORT_NAME));

    // Parse the section data
    std::vector<uint8_t> data_;
    if (raw_section->SizeOfRawData &&
        !(raw_section->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)) {
      auto t = reinterpret_cast<const uint8_t *>(data) +
               raw_section->PointerToRawData;
      data_.assign(t, t + raw_section->SizeOfRawData);
    }

    // Add the section to the section table
    sections_[i] = std::make_unique<ObjectSection>(
        this, i, *raw_section, std::move(name), std::move(data_),
        std::vector<std::unique_ptr<ObjectRelocation>>());

    // Parse the relocations
    size_t reloc_count = raw_section->NumberOfRelocations;
    if (!reloc_count) {
      continue;
    }
    auto raw_relocs = reinterpret_cast<const IMAGE_RELOCATION *>(
        p + raw_section->PointerToRelocations);
    auto &relocs = sections_[i]->relocs_;
    relocs.resize(reloc_count);
    for (size_t j = 0; j < reloc_count; j++) {
      relocs[j] = std::make_unique<ObjectRelocation>(sections_[i].get(), j,
                                                     raw_relocs[j]);
    }
  }

  // Parse the symbol table
  auto symbols = reinterpret_cast<const IMAGE_SYMBOL *>(
      p + coff_header->PointerToSymbolTable);
  auto strings =
      reinterpret_cast<const char *>(symbols + header_.NumberOfSymbols);
  auto symbol_count = (uint32_t)header_.NumberOfSymbols;
  for (uint32_t i = 0; i < symbol_count; i++) {
    // Parse the auxiliary symbols
    const IMAGE_SYMBOL *raw_symbol = &symbols[i];
    std::vector<IMAGE_AUX_SYMBOL> aux_symbols;
    if (raw_symbol->NumberOfAuxSymbols) {
      aux_symbols.resize(raw_symbol->NumberOfAuxSymbols);
      auto t = reinterpret_cast<const IMAGE_AUX_SYMBOL *>(raw_symbol + 1);
      std::copy(t, t + raw_symbol->NumberOfAuxSymbols, aux_symbols.begin());
    }

    // Parse the name of the symbol
    std::string name;
    if (raw_symbol->N.Name.Short == 0) {
      name.assign(strings + raw_symbol->N.Name.Long);
    } else {
      auto t = reinterpret_cast<const char *>(raw_symbol->N.ShortName);
      name.assign(t, strnlen(t, IMAGE_SIZEOF_SHORT_NAME));
    }

    // Add the symbol to the symbol table
    symbols_.push_back(std::make_unique<ObjectSymbol>(
        this, symbols_.size(), i, *raw_symbol, std::move(aux_symbols),
        std::move(name)));
    i += raw_symbol->NumberOfAuxSymbols;
  }

  // Parse the string table
  strings_.assign(strings, strings + *(uint32_t *)strings);

  return true;
}

bool ObjectFile::Save(std::vector<uint8_t> &data) {
  // Calculate the size of the object file
  uint32_t sections_size = 0;
  size_t relocs_size = 0;
  for (auto &&section : sections_) {
    if (!section->data_.empty()) {
      sections_size =
          std::max(section->offset() + section->size(), sections_size);
    }
    relocs_size += section->relocs_.size() * sizeof(IMAGE_RELOCATION);
  }
  size_t symbols_size =
      symbols_.back()->next_real_index() * sizeof(IMAGE_SYMBOL);
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
  auto raw_sections = reinterpret_cast<IMAGE_SECTION_HEADER *>(
      data.data() + sizeof(IMAGE_FILE_HEADER));
  size_t index = 0;
  for (auto &&section : sections_) {
    // Copy the section header
    IMAGE_SECTION_HEADER *raw_section = &raw_sections[index];
    *raw_section = section->header_;

    // Copy the section data
    auto &data2 = section->data_;
    if (!data2.empty()) {
      auto t = data.data() + section->offset();
      std::copy(data2.begin(), data2.end(), t);
      raw_section->PointerToRawData = section->offset();
      raw_section->SizeOfRawData = (DWORD)data2.size();
    }

    // Copy the relocations
    auto &relocs = section->relocs_;
    if (!relocs.empty()) {
      auto t =
          reinterpret_cast<IMAGE_RELOCATION *>(data.data() + relocs_offset);
      for (auto &&reloc : relocs) *(t++) = reloc->reloc_;
      raw_section->PointerToRelocations = (DWORD)relocs_offset;
      raw_section->NumberOfRelocations = (WORD)relocs.size();
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
    assert(index == symbol->real_index());
    symbols[index++] = symbol->symbol();
    const std::vector<IMAGE_AUX_SYMBOL> &aux_symbols = symbol->aux_symbols();
    if (aux_symbols.size()) {
      auto t = reinterpret_cast<IMAGE_AUX_SYMBOL *>(symbols + index);
      std::copy(aux_symbols.begin(), aux_symbols.end(), t);
      index += aux_symbols.size();
    }
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
    if (section->name_ == name) {
      return section.get();
    }
  }
  return nullptr;
}

ObjectSymbol *ObjectFile::FindSymbol(std::string_view name) {
  for (auto &&symbol : symbols_) {
    if (symbol->name() == name) {
      return symbol.get();
    }
  }
  return nullptr;
}

ObjectSymbol *ObjectFile::FindSymbol(size_t real_index) {
  for (auto &&symbol : symbols_) {
    if (symbol->real_index() == real_index) {
      return symbol.get();
    }
  }
  return nullptr;
}

ObjectSection &ObjectFile::AddSection(std::string_view name) {
  auto &section = sections_.emplace_back(std::make_unique<ObjectSection>());
  section->file_ = this;
  section->index_ = sections_.size() - 1;
  // TODO: offset, name_size>=8 like symbol
  size_t name_size = std::min(name.size(), (size_t)IMAGE_SIZEOF_SHORT_NAME);
  std::copy_n(name.begin(), name_size, section->header_.Name);
  section->name_ = name.substr(0, name_size);
  return *section;
}

ObjectSection &ObjectFile::AddSection(std::string_view name,
                                      std::span<const uint8_t> data) {
  auto &section = AddSection(name);
  section.AddData(data);
  return section;
}

ObjectSymbol &ObjectFile::AddSymbol(std::string_view name) {
  auto &last_symbol = *symbols_.back();
  auto &symbol = symbols_.emplace_back(std::make_unique<ObjectSymbol>());
  symbol->file_ = this;
  symbol->index_ = symbols_.size() - 1;
  symbol->real_index_ = last_symbol.next_real_index();
  symbol->UpdateName(name);
  return *symbol;
}

ObjectSymbol &ObjectFile::AddSymbol(std::string_view name, uint32_t value,
                                    int16_t section, uint16_t type,
                                    uint8_t storage) {
  auto &symbol = AddSymbol(name);
  symbol.set_value(value);
  symbol.set_section(section);
  symbol.set_type(type);
  symbol.set_storage(storage);
  return symbol;
}

ObjectSymbol &ObjectFile::AddSymbol(std::string_view name,
                                    ObjectSection &section,
                                    std::span<const uint8_t> data,
                                    uint16_t type, uint8_t storage) {
  uint32_t value = (uint32_t)section.AddData(data);
  int16_t section_number = (int16_t)(uint16_t)section.index() + 1;
  return AddSymbol(name, value, section_number, type, storage);
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
  if (new_size == old_size) {
    return old_size;
  }

  // Resize the section
  uint32_t diff_size = (uint32_t)(new_size - old_size);
  data_.resize(new_size);
  if ((int32_t)diff_size > 0) {
    int value = header_.Characteristics & IMAGE_SCN_MEM_EXECUTE ? 0xCC : 0;
    std::fill_n(data_.data() + old_size, diff_size, value);
  }

  // Update the offsets of other sections
  for (auto &&section : file_->sections()) {
    if (section->offset() > offset()) {
      section->set_offset(section->offset() + diff_size);
    }
  }

  return old_size;
}

size_t ObjectSection::AddData(std::span<const uint8_t> data) {
  size_t offset = Resize(data_.size() + data.size());
  std::copy(data.begin(), data.end(), data_.data() + offset);
  return offset;
}

ObjectRelocation &ObjectSection::AddReloc(uint32_t offset, uint32_t symbol,
                                          uint16_t type) {
  auto &reloc = relocs_.emplace_back(std::make_unique<ObjectRelocation>());
  reloc->section_ = this;
  reloc->index_ = relocs_.size() - 1;
  reloc->reloc_ = IMAGE_RELOCATION{
      .VirtualAddress = offset,
      .SymbolTableIndex = symbol,
      .Type = type,
  };
  return *reloc;
}

ObjectRelocation &ObjectSection::AddReloc(uint32_t offset,
                                          const ObjectSymbol &symbol,
                                          uint16_t type) {
  return AddReloc(offset, (uint32_t)symbol.real_index(), type);
}

/*==============================================================================
 * ObjectSymbol
 *============================================================================*/

void ObjectSymbol::UpdateData(ObjectSection &section,
                              std::span<const uint8_t> data) {
  uint32_t value = (uint32_t)section.AddData(data);
  set_value(value);
  set_section(section);
}

void ObjectSymbol::UpdateName(std::string_view name) {
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
