#include "log.h"
#include "obj.h"
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
    std::unordered_map<std::string, std::shared_ptr<std::vector<uint8_t>>,
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
  if (!file)
    return {};

  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char *>(data.data()), size);
  return data;
}

void WriteFile(std::string_view path, std::span<const uint8_t> data) {
  std::ofstream file(path.data(), std::ios::binary);
  if (!file)
    return;

  file.write(reinterpret_cast<const char *>(data.data()), data.size());
}

ApiMap LoadApis(std::string_view path) {
  ApiMap apis;
  std::ifstream file(path.data());
  if (!file)
    return apis;

  // Parse the API file
  std::string module;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty())
      continue;

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
      if (it == apis.end())
        apis[func] = {module};
      else
        it->second.push_back(module);
    }
  }
  return apis;
}

IntrinsicMap LoadIntrinsics(std::string_view directory) {
  IntrinsicMap intrinsics;
  for (auto &&entry : fs::directory_iterator(directory)) {
    if (!entry.is_regular_file())
      continue;

    auto ext = entry.path().extension().string();
    if (!iequals(ext, ".obj") && !iequals(ext, ".o"))
      continue;

    // Load the object file
    ObjectFile obj;
    obj.Load(ReadFile(entry.path().string()));

    // Extract intrinsics from the object file
    for (auto &&symbol : obj.symbols_) {
      if (symbol.symbol_.SectionNumber <= IMAGE_SYM_UNDEFINED ||
          symbol.symbol_.StorageClass != IMAGE_SYM_CLASS_EXTERNAL)
        continue; // Skip undefined symbols and non-external symbols

      auto &section = obj.sections_[(size_t)symbol.symbol_.SectionNumber - 1];
      if (section.relocs_.size()) {
        errorln("Skipped intrinsic with relocations: {}", symbol.name_);
        continue;
      }

      auto data = std::vector<uint8_t>(
          section.data_.begin() + symbol.symbol_.Value, section.data_.end());
      intrinsics[symbol.name_] =
          std::make_shared<std::vector<uint8_t>>(std::move(data));
      traceln("Loaded intrinsic: {} ({} bytes)", symbol.name_,
              section.data_.size());
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
    auto t = name.find('@');
    if (is_cdecl && t != std::string_view::npos) {
      name = name.substr(0, t);
      is_cdecl = false;
      is_stdcall = true;
    }
  }
  return std::string(name);
}

bool RemoveSourcePath(ObjectFile &obj, std::string_view filename) {
  // Remove the source file path
  for (auto &&section : obj.sections_) {
    if (!section.name_.starts_with(".debug"))
      continue;

    auto p = reinterpret_cast<char *>(section.data_.data());
    for (size_t i = 0; i < section.data_.size(); i++) {
      auto s = std::string_view(p + i);
      if (s.starts_with(filename)) {
        while (i && p[i - 1])
          i--;
        s = std::string_view(p + i);
        println("Removed original source path: {}", s);
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
        if (m == pm)
          return m;
      }
    }
    return modules.front();
  }();
  trace("Resolved module for {}: {} (picked)", name, module);
  for (auto &&m : modules) {
    if (m != module)
      trace(", {}", m);
  }
  trace("\n");
  return true;
}

bool ResolveIntrinsic(const IntrinsicMap &intrinsics, std::string_view name,
                      std::shared_ptr<std::vector<uint8_t>> &data) {
  auto it = intrinsics.find(name);
  if (it == intrinsics.end()) {
    data = nullptr;
    return false;
  }
  data = it->second;
  traceln("Resolved intrinsic for {}: {} bytes", name, data->size());
  return true;
}

size_t FixImports(ObjectSection &text_section, const ApiMap &apis,
                  const IntrinsicMap &intrinsics) {
  size_t count = 0;
  ObjectFile &obj = *text_section.file_;
  for (size_t i = 0; i < obj.symbols_.size(); i++) {
    auto &symbol = obj.symbols_[i];
    if (symbol.symbol_.SectionNumber > IMAGE_SYM_UNDEFINED ||
        symbol.symbol_.StorageClass != IMAGE_SYM_CLASS_EXTERNAL ||
        symbol.name_.find('$') != std::string::npos)
      continue; // Skip defined symbols

    // Try resolving the intrinsic
    std::string name = symbol.name_;
    std::shared_ptr<std::vector<uint8_t>> intrinsic;
    if (ResolveIntrinsic(intrinsics, name, intrinsic)) {
      // Add an intrinsic to the .text section
      symbol.symbol_.Value = (DWORD)text_section.AddData(*intrinsic);
      symbol.symbol_.SectionNumber = (SHORT)(USHORT)(text_section.index() + 1);
      count++;
      println("Injected intrinsic symbol {}", name);
      continue;
    }

    // Demangle the symbol name
    bool is_x86 = obj.header_.Machine == IMAGE_FILE_MACHINE_I386;
    bool is_import, is_cdecl, is_stdcall = false;
    std::string demangled_name =
        DemangleSymbol(name, is_x86, is_import, is_cdecl, is_stdcall);
    traceln("Symbol: {}, Import: {}, Cdecl: {}, Stdcall: {}", demangled_name,
            is_import, is_cdecl, is_stdcall);

    // Don't process beacon APIs
    bool is_beacon_api = kBeaconApis.contains(demangled_name);
    if (is_beacon_api) {
      traceln("Skipped beacon API: {}", name);
      continue;
    }

    // Try resolving the API module
    std::string module;
    if (!ResolveApiModule(apis, demangled_name, module))
      continue;
    std::string new_import_name =
        "__imp_" + module.substr(0, module.size() - 4) + "$" + demangled_name;

    // Just update the symbol name if it's an import
    if (is_import) {
      symbol.SetName(new_import_name);
      count++;
      println("Renamed import symbol from {} to {}", name, new_import_name);
      continue;
    }

    // Add an import thunk to the .text section
    uint8_t thunk[6] = {0xFF, 0x25, 0, 0, 0, 0};
    symbol.symbol_.Value = (DWORD)text_section.AddData(thunk);
    symbol.symbol_.SectionNumber = (SHORT)(USHORT)(text_section.index() + 1);

    // Add an import symbol
    auto &new_import_symbol =
        obj.AddSymbol(new_import_name, 0, IMAGE_SYM_UNDEFINED,
                      IMAGE_SYM_TYPE_NULL, IMAGE_SYM_CLASS_EXTERNAL);

    // Add a relocation for the import thunk
    // TODO: add ObjectRelocation and remove GetRealIndex
    size_t index = new_import_symbol.GetRealIndex();
    uint16_t reloc_type = obj.header_.Machine == IMAGE_FILE_MACHINE_I386
                              ? IMAGE_REL_I386_DIR32
                              : IMAGE_REL_AMD64_REL32;
    text_section.AddReloc(symbol.symbol_.Value + 2, (uint32_t)index,
                          reloc_type);

    count++;
    println("Injected import thunk {} and added import symbol {}", name,
            new_import_name);
  }
  return count;
}

void FixBOF(fs::path bof_path, const ApiMap &apis_x86, const ApiMap &apis_x64,
            const IntrinsicMap &intrinsics_x86,
            const IntrinsicMap &intrinsics_x64) {
  // Load the object file
  ObjectFile obj;
  obj.Load(ReadFile(bof_path.string()));
  traceln("Loaded object file: {}", bof_path.filename().string());

  // Remove the source file path
  std::string source = bof_path.stem().string();
  RemoveSourcePath(obj, source);

  // Find the .text section
  ObjectSection *text_section = obj.FindSection(".text");
  if (!text_section) {
    for (auto &&section : obj.sections_) {
      if (section.header_.Characteristics & IMAGE_SCN_MEM_EXECUTE) {
        text_section = &section;
        break;
      }
    }
  }
  if (!text_section) {
    errorln("Can't find .text section");
    return;
  }

  // Fix imports
  size_t count = 0;
  if (obj.header_.Machine == IMAGE_FILE_MACHINE_I386)
    count = FixImports(*text_section, apis_x86, intrinsics_x86);
  else if (obj.header_.Machine == IMAGE_FILE_MACHINE_AMD64)
    count = FixImports(*text_section, apis_x64, intrinsics_x64);
  if (count > 0)
    println("Fixed {} imports", count);

  // Save the object file
  std::vector<uint8_t> new_data;
  obj.Save(new_data);
  WriteFile(bof_path.string(), new_data);
  traceln("Saved object file: {}", bof_path.filename().string());
}
} // namespace

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
  for (int i = 2; i < argc; i++)
    FixBOF(argv[i], apis_x86, apis_x64, intrinsics_x86, intrinsics_x64);
  return 0;
}
