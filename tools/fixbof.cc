#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "log.h"
#include "obj.h"

namespace fs = std::filesystem;

namespace {
struct string_hash {
  using is_transparent = void;
  size_t operator()(const char *s) const {
    return std::hash<std::string_view>{}(s);
  }
  size_t operator()(std::string_view s) const {
    return std::hash<std::string_view>{}(s);
  }
  size_t operator()(const std::string &s) const {
    return std::hash<std::string_view>{}(s);
  }
};

bool iequals(std::string_view x, std::string_view y) {
  return std::ranges::equal(x, y, [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
  });
}

using ApiMap = std::unordered_map<std::string, std::vector<std::string>,
                                  string_hash, std::equal_to<>>;

using IntrinsicMap =
    std::unordered_map<std::string, std::unique_ptr<std::vector<uint8_t>>,
                       string_hash, std::equal_to<>>;

const std::unordered_set<std::string_view> kBeaconApis = {
    "LoadLibraryA",
    "FreeLibrary",
    "GetProcAddress",
    "GetModuleHandleA",
    "BeaconDataParse",
    "BeaconDataPtr",
    "BeaconDataInt",
    "BeaconDataShort",
    "BeaconDataLength",
    "BeaconDataExtract",
    "BeaconFormatAlloc",
    "BeaconFormatReset",
    "BeaconFormatAppend",
    "BeaconFormatPrintf",
    "BeaconFormatToString",
    "BeaconFormatFree",
    "BeaconFormatInt",
    "BeaconOutput",
    "BeaconPrintf",
    "BeaconErrorD",
    "BeaconErrorDD",
    "BeaconErrorNA",
    "BeaconUseToken",
    "BeaconRevertToken",
    "BeaconIsAdmin",
    "BeaconGetSpawnTo",
    "BeaconInjectProcess",
    "BeaconInjectTemporaryProcess",
    "BeaconSpawnTemporaryProcess",
    "BeaconCleanupProcess",
    "toWideChar",
    "BeaconInformation",
    "BeaconAddValue",
    "BeaconGetValue",
    "BeaconRemoveValue",
    "BeaconDataStoreGetItem",
    "BeaconDataStoreProtectItem",
    "BeaconDataStoreUnprotectItem",
    "BeaconDataStoreMaxEntries",
    "BeaconGetCustomUserData",
};

const std::array<std::string_view, 35> kPreferredModules = {
    "kernel32.dll", "kernelbase.dll", "advapi32.dll", "bcrypt.dll",
    "cabinet.dll",  "crypt32.dll",    "cryptdll.dll", "dnsapi.dll",
    "fltlib.dll",   "mpr.dll",        "msxml2.dll",   "ncrypt.dll",
    "netapi32.dll", "ntdsapi.dll",    "odbc32.dll",   "ole32.dll",
    "oleaut32.dll", "rpcrt4.dll",     "shlwapi.dll",  "samlib.dll",
    "secur32.dll",  "shell32.dll",    "user32.dll",   "userenv.dll",
    "version.dll",  "hid.dll",        "setupapi.dll", "winscard.dll",
    "winsta.dll",   "wbemuuid.dll",   "wldap32.dll",  "wtsapi32.dll",
    "msasn1.dll",   "msvcrt.dll",     "ntdll.dll",
};

std::vector<uint8_t> ReadFile(std::string_view path) {
  std::ifstream file(path.data(), std::ios::binary);
  if (!file) {
    return {};
  }

  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char *>(data.data()), size);
  return data;
}

void WriteFile(std::string_view path, std::span<const uint8_t> data) {
  std::ofstream file(path.data(), std::ios::binary);
  if (!file) {
    return;
  }

  file.write(reinterpret_cast<const char *>(data.data()), data.size());
}

ApiMap LoadApis(std::string_view path) {
  ApiMap apis;
  std::ifstream file(path.data());
  if (!file) {
    return apis;
  }

  // Parse the API file
  std::string module;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    // Extract the module name
    if (line[0] == '[') {
      module = line.substr(1, line.size() - 2);
      continue;
    }

    // Extract the function name
    auto pos = line.find('=');
    if (pos != std::string::npos && !module.empty()) {
      auto func = line.substr(pos + 1);
      auto it = apis.find(func);
      if (it == apis.end()) {
        apis[func] = {module};
      } else {
        it->second.push_back(module);
      }
    }
  }
  return apis;
}

IntrinsicMap LoadIntrinsics(std::string_view directory) {
  IntrinsicMap intrinsics;
  for (auto &&entry : fs::directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    auto ext = entry.path().extension().string();
    if (!iequals(ext, ".obj") && !iequals(ext, ".o")) {
      continue;
    }

    // Load the object file
    ObjectFile obj;
    obj.Load(ReadFile(entry.path().string()));

    // Extract intrinsics from the object file
    for (auto &&symbol : obj.symbols()) {
      // Skip undefined symbols and non-external symbols
      ObjectSection *section = symbol->section();
      if (!section || symbol->storage() != IMAGE_SYM_CLASS_EXTERNAL) {
        continue;
      }

      if (section->reloc_count()) {
        errorln("Skipped intrinsic with relocations: {}", symbol->name());
        continue;
      }

      std::vector<uint8_t> data(section->data().begin() + symbol->value(),
                                section->data().end());
      traceln("Loaded intrinsic: {} ({} bytes)", symbol->name(), data.size());
      intrinsics[symbol->name()] =
          std::make_unique<std::vector<uint8_t>>(std::move(data));
    }
  }
  return intrinsics;
}

std::string DemangleSymbol(std::string_view name, bool is_x86, bool &is_import,
                           bool &is_cdecl, bool &is_stdcall) {
  is_import = false;
  is_cdecl = false;
  is_stdcall = false;
  if (name.starts_with("__imp_")) {
    is_import = true;
    name = name.substr(6);
  }
  if (is_x86) {
    if (name.starts_with("_")) {
      is_cdecl = true;
      name = name.substr(1);
    }
    auto t = name.rfind('@');
    if (is_cdecl && t != std::string_view::npos) {
      name = name.substr(0, t);
      is_cdecl = false;
      is_stdcall = true;
    }
  }
  return std::string(name);
}

bool CheckRelocations(const ObjectSection &text_section, const ApiMap &apis,
                      const IntrinsicMap &intrinsics) {
  // Check if the object file has duplicate sections
  bool b = true;
  const ObjectFile &obj = *text_section.file();
  std::unordered_set<std::string> section_names;
  for (auto &&section : obj.sections()) {
    const std::string &name = section->name();
    auto p = section_names.insert(name);
    if (!p.second &&
        (name.starts_with(".text") || name == ".rdata" || name == ".data" ||
         name == ".xdata" || name == ".pdata" || name == ".bss")) {
      errorln("Duplicate section: {}", name);
      b = false;
    }
  }
  if (!b) {
    errorln("MSVC: Use /Gy- option and don't use 'static' keyword.");
    errorln(
        "GCC: Don't use -ffunction-sections option or -fdata-sections option.");
    return false;
  }

  // Check relocations like Cobalt Strike
  bool is_x86 = obj.machine() == IMAGE_FILE_MACHINE_I386;
  for (auto &&t : obj.sections()) {
    // Only process relocs in .text, .data, and .pdata sections
    if (t.get() != &text_section && t->name() != ".data" &&
        t->name() != ".pdata") {
      continue;
    }

    // Check relocations
    for (auto &&reloc : t->relocs()) {
      const ObjectSymbol *symbol = reloc->symbol();
      const ObjectSection *section = symbol->section();
      if (!section) {
        // The referenced symbol is in an unknown section
        const std::string &name = symbol->name();
        if (intrinsics.find(name) != intrinsics.end()) {
          continue;
        }
        bool is_import, is_cdecl, is_stdcall = false;
        std::string demangled_name =
            DemangleSymbol(name, is_x86, is_import, is_cdecl, is_stdcall);
        if (kBeaconApis.contains(demangled_name) ||
            apis.find(demangled_name) != apis.end()) {
          continue;
        }

        errorln("Unknown symbol: {}", name);
        b = false;
      } else if (section == &text_section || section->name() == ".rdata" ||
                 section->name() == ".data" || section->name() == ".xdata" ||
                 section->name() == ".bss") {
        // The referenced symbol is in an existing section
        uint16_t type = reloc->type();
        bool is_supported_reloc;
        if (is_x86) {
          is_supported_reloc =
              type == IMAGE_REL_I386_DIR32 || type == IMAGE_REL_I386_REL32;
        } else {
          is_supported_reloc = type == IMAGE_REL_AMD64_ADDR64 ||
                               type == IMAGE_REL_AMD64_ADDR32NB ||
                               (IMAGE_REL_AMD64_REL32 <= type &&
                                type <= IMAGE_REL_AMD64_REL32_5);
        }
        if (!is_supported_reloc) {
          errorln("Unsupported relocation type: {} ({}+{:#x} -> {})", type,
                  t->name(), reloc->offset(), symbol->name());
          b = false;
        }
      }
    }
  }
  return b;
}

bool RemoveOutputPath(ObjectFile &obj, std::string_view filename) {
  // Remove the object file path
  for (auto &&section : obj.sections()) {
    if (!section->name().starts_with(".debug")) {
      continue;
    }

    std::vector<uint8_t> &data = section->data();
    auto p = reinterpret_cast<char *>(data.data());
    for (size_t i = 0; i < data.size(); i++) {
      auto s = std::string_view(p + i);
      if (s.starts_with(filename)) {
        while (i && p[i - 1]) i--;
        s = std::string_view(p + i);
        println("Removed original output path: {}", s);
        std::fill_n(p + i, s.size(), 0);
        return true;
      }
    }
  }
  return false;
}

bool ResolveApiModule(const ApiMap &apis, std::string_view name,
                      std::string &module) {
  // Resolve the module name
  auto it = apis.find(name);
  if (it == apis.end()) {
    module = {};
    errorln("Can't resolve module for {}", name);
    return false;
  }
  const std::vector<std::string> &modules = it->second;
  module = [&] {
    for (auto &&pm : kPreferredModules) {
      for (auto &&m : modules) {
        if (m == pm) {
          return m;
        }
      }
    }
    return modules.front();
  }();
  trace("Resolved module for {}: {} (picked)", name, module);
  for (auto &&m : modules) {
    if (m != module) {
      trace(", {}", m);
    }
  }
  trace("\n");
  return true;
}

bool ResolveIntrinsic(const IntrinsicMap &intrinsics, std::string_view name,
                      const std::vector<uint8_t> *&data) {
  auto it = intrinsics.find(name);
  if (it == intrinsics.end()) {
    data = nullptr;
    return false;
  }
  data = it->second.get();
  traceln("Resolved intrinsic for {}: {} bytes", name, data->size());
  return true;
}

size_t FixImports(ObjectSection &text_section, const ApiMap &apis,
                  const IntrinsicMap &intrinsics) {
  size_t intrinsic_count = 0;
  size_t beacon_api_count = 0;
  size_t dynamic_api_count = 0;
  ObjectFile &obj = *text_section.file();
  for (size_t i = 0; i < obj.symbols().size(); i++) {
    // Skip defined symbols
    ObjectSymbol &symbol = *obj.symbols()[i];
    if (symbol.section() || symbol.storage() != IMAGE_SYM_CLASS_EXTERNAL ||
        symbol.name().find('$') != std::string::npos) {
      continue;
    }

    // Try resolving the intrinsic
    const std::string name = symbol.name();
    const std::vector<uint8_t> *intrinsic = nullptr;
    if (ResolveIntrinsic(intrinsics, name, intrinsic)) {
      // Add an intrinsic to the .text section
      symbol.UpdateData(text_section, *intrinsic);
      intrinsic_count++;
      println("  {}(intrin)", name);
      continue;
    }

    // Demangle the symbol name
    bool is_x86 = obj.machine() == IMAGE_FILE_MACHINE_I386;
    bool is_import, is_cdecl, is_stdcall = false;
    std::string demangled_name =
        DemangleSymbol(name, is_x86, is_import, is_cdecl, is_stdcall);
    traceln("Symbol: {}, Import: {}, Cdecl: {}, Stdcall: {}", demangled_name,
            is_import, is_cdecl, is_stdcall);

    // Don't process beacon APIs
    bool is_beacon_api = kBeaconApis.contains(demangled_name);
    if (is_beacon_api) {
      beacon_api_count++;
      traceln("Skipped beacon API: {}", name);
      continue;
    }

    // Try resolving the API module
    std::string module;
    if (!ResolveApiModule(apis, demangled_name, module)) {
      continue;
    }

    // Process dynamic APIs
    std::string new_name =
        "__imp_" + module.substr(0, module.size() - 4) + "$" + demangled_name;
    if (is_import) {
      // __imp_LoadLibraryA -> __imp_kernel32$LoadLibraryA

      // Just update the symbol name if it's already an import
      symbol.UpdateName(new_name);
      dynamic_api_count++;
      println("  {} -> {}", demangled_name, new_name);
    } else {
      // malloc -> malloc(thunk) -> __imp_msvcrt$malloc

      // Add an import symbol
      auto &import_symbol =
          obj.AddSymbol(new_name, 0, IMAGE_SYM_UNDEFINED, IMAGE_SYM_TYPE_NULL,
                        IMAGE_SYM_CLASS_EXTERNAL);

      // Make the original symbol a thunk
      uint8_t thunk[6] = {0xFF, 0x25, 0, 0, 0, 0};
      symbol.UpdateData(text_section, thunk);
      uint16_t reloc_type =
          is_x86 ? IMAGE_REL_I386_DIR32 : IMAGE_REL_AMD64_REL32;
      text_section.AddReloc(symbol.value() + 2, import_symbol, reloc_type);

      dynamic_api_count++;
      println("  {}(thunk) -> {}", demangled_name, new_name);
    }
  }
  if (dynamic_api_count > 32) {
    errorln("Too many dynamic APIs: {} (BOF limit is 32)", dynamic_api_count);
  }
  return intrinsic_count + dynamic_api_count;
}

void FixBOF(fs::path bof_path, const ApiMap &apis_x86, const ApiMap &apis_x64,
            const IntrinsicMap &intrinsics_x86,
            const IntrinsicMap &intrinsics_x64) {
  // Load the object file
  ObjectFile obj;
  obj.Load(ReadFile(bof_path.string()));
  traceln("Loaded object file: {}", bof_path.filename().string());

  // Select the API map and intrinsic map based on the machine type
  const ApiMap *apis = nullptr;
  const IntrinsicMap *intrinsics = nullptr;
  if (obj.machine() == IMAGE_FILE_MACHINE_I386) {
    apis = &apis_x86;
    intrinsics = &intrinsics_x86;
  } else if (obj.machine() == IMAGE_FILE_MACHINE_AMD64) {
    apis = &apis_x64;
    intrinsics = &intrinsics_x64;
  } else {
    errorln("Unsupported machine type: {}", obj.machine());
    return;
  }

  // Find the .text section
  ObjectSection *text_section = obj.FindSection(".text");
  if (!text_section) {
    for (auto &&section : obj.sections()) {
      if (section->characteristics() & IMAGE_SCN_MEM_EXECUTE) {
        text_section = section.get();
        break;
      }
    }
  }
  if (!text_section) {
    errorln("Can't find .text section");
    return;
  }

  // Check relocations
  if (!CheckRelocations(*text_section, *apis, *intrinsics)) {
    return;
  }

  // Remove the object file path
  std::string obj_filename = bof_path.stem().string();
  RemoveOutputPath(obj, obj_filename);

  // Fix imports
  size_t count = FixImports(*text_section, *apis, *intrinsics);
  if (count > 0) {
    println("Fixed {} imports", count);
  }

  // Save the object file
  std::vector<uint8_t> new_data;
  obj.Save(new_data);
  WriteFile(bof_path.string(), new_data);
  traceln("Saved object file: {}", bof_path.filename().string());
}
}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    errorln("Usage: {} <link_path> <file1> [file2] ...", argv[0]);
    return 1;
  }
  fs::path link_path = argv[1];
  ApiMap apis_x86 = LoadApis((link_path / "x86" / "apis.txt").string());
  ApiMap apis_x64 = LoadApis((link_path / "x64" / "apis.txt").string());
  IntrinsicMap intrinsics_x86 = LoadIntrinsics((link_path / "x86").string());
  IntrinsicMap intrinsics_x64 = LoadIntrinsics((link_path / "x64").string());
  for (int i = 2; i < argc; i++) {
    println("Processing: {}", argv[i]);
    FixBOF(argv[i], apis_x86, apis_x64, intrinsics_x86, intrinsics_x64);
  }
  return 0;
}
