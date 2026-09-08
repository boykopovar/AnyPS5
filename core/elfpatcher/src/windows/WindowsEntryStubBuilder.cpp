#include "WindowsEntryStubBuilder.hpp"
#include "WindowsStubEmitter.hpp"
#include <io/BufferUtils.hpp>
#include <algorithm>

namespace Elfpatcher::Windows {

namespace {

constexpr std::uint32_t PathCapacity = 32768;

std::string normalizeRunPath(std::string path) {
    if (path.empty() || std::any_of(path.begin(), path.end(), [](const unsigned char value) { return value == 0 || value >= 128; }))
        throw Domain::RelinkerException("Windows run path must be a nonempty ASCII path");
    std::replace(path.begin(), path.end(), '/', '\\');
    if (path == "$ORIGIN")
        path = ".";
    if (path.starts_with("$ORIGIN\\"))
        path.erase(0, 8);
    if (path.find('$') != std::string::npos)
        throw Domain::RelinkerException("Unsupported variable in Windows run path");
    if (path.back() != '\\')
        path.push_back('\\');
    return path;
}

}

WindowsEntryStub WindowsEntryStubBuilder::Build(const std::uint32_t dataRva, const std::uint32_t entryRva, const WindowsImports& nativeImports, const std::vector<std::string>& libraries, const std::vector<PeImport>& imports, const std::string& runPath) const {
    if (!imports.empty() && libraries.empty())
        throw Domain::RelinkerException("ELF imports have no DT_NEEDED libraries");
    const auto path = normalizeRunPath(runPath);
    const bool absolutePath = path.starts_with("\\\\") || (path.size() >= 3 && path[1] == ':' && path[2] == '\\');
    if (!absolutePath && (path.front() == '\\' || path.find(':') != std::string::npos))
        throw Domain::RelinkerException("Ambiguous Windows run path: " + path);
    WindowsEntryStub result{{".startup", dataRva, SectionRead | SectionWrite | 0x40u, {}}, {".entry", 0, SectionRead | SectionExecute | 0x20u, {}}, {}};
    auto& data = result.Data.Data;
    const auto reserve = [&](const std::size_t size) {
        const auto rva = CheckedRva(dataRva + data.size());
        data.resize(data.size() + size);
        return rva;
    };
    const auto addString = [&](const std::string& value) {
        const auto rva = CheckedRva(dataRva + data.size());
        Io::AppendString(data, value);
        return rva;
    };
    const auto programPath = reserve(PathCapacity);
    const auto modulePath = reserve(PathCapacity);
    const auto handles = reserve(libraries.size() * 8);
    const auto functionTable = reserve(12);
    const auto unwindRva = CheckedRva(dataRva + data.size());
    data.insert(data.end(), {1, 10, 6, 0, 10, 0xb2, 6, 0xc0, 4, 0x70, 3, 0x60, 2, 0x50, 1, 0x30});
    std::vector<std::uint32_t> libraryPaths;
    std::vector<std::string> libraryNames;
    for (const auto& library : libraries) {
        auto name = path + library;
        if (name.size() + 1 >= PathCapacity)
            throw Domain::RelinkerException("Windows library path exceeds the startup buffer: " + name);
        libraryPaths.push_back(addString(name));
        libraryNames.push_back(std::move(name));
    }
    std::vector<std::uint32_t> symbolNames;
    for (const auto& import : imports)
        symbolNames.push_back(addString(import.Name));
    const auto diagnosticsOffset = data.size();
    std::vector<std::string> errors = {"FAIL: cannot obtain executable path\n", "FAIL: executable or library path is too long\n", "FAIL: executable path has no directory\n"};
    for (const auto& name : libraryNames)
        errors.push_back("FAIL: cannot load library " + name + "\n");
    for (const auto& import : imports)
        errors.push_back("FAIL: unresolved ELF import " + import.Name + "\n");
    std::vector<std::uint32_t> errorRvas;
    for (const auto& error : errors)
        errorRvas.push_back(addString(error));
    if (data.size() <= diagnosticsOffset)
        throw Domain::RelinkerException("Empty startup diagnostics");
    result.Code.Rva = AlignRva(dataRva + data.size());
    WindowsStubEmitter code(result.Code.Rva);
    const auto call = [&](const std::string& name) { code.Rip({0xff, 0x15}, nativeImports.Functions.at(name)); };
    const auto fail = [&](const std::size_t error, const std::uint32_t status) {
        code.Emit({0xb9});
        code.U32(0xfffffff4u);
        call("GetStdHandle");
        code.Emit({0x48, 0x89, 0xc1});
        code.Rip({0x48, 0x8d, 0x15}, errorRvas.at(error));
        code.Emit({0x41, 0xb8});
        code.U32(CheckedRva(errors.at(error).size()));
        code.Emit({0x4c, 0x8d, 0x4c, 0x24, 0x30, 0x48, 0xc7, 0x44, 0x24, 0x20, 0, 0, 0, 0});
        call("WriteFile");
        code.Emit({0xb9});
        code.U32(status);
        code.Emit({0xba, 1, 0, 0, 0, 0x45, 0x31, 0xc0, 0x45, 0x31, 0xc9});
        call("RaiseException");
        code.Emit({0x0f, 0x0b});
    };
    const auto requireNonzero = [&](const std::size_t error, const std::uint32_t status) {
        code.Emit({0x48, 0x85, 0xc0});
        const auto success = code.Branch({0x0f, 0x85});
        fail(error, status);
        code.PatchBranch(success, code.GetRva());
    };
    code.Emit({0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x48, 0x83, 0xec, 0x60});
    code.Emit({0x31, 0xc9});
    code.Rip({0x48, 0x8d, 0x15}, programPath);
    code.Emit({0x41, 0xb8});
    code.U32(PathCapacity);
    call("GetModuleFileNameA");
    requireNonzero(0, 0xc000000du);
    code.Emit({0x3d});
    code.U32(PathCapacity);
    const auto pathFits = code.Branch({0x0f, 0x82});
    fail(1, 0xc0000106u);
    code.PatchBranch(pathFits, code.GetRva());
    code.Emit({0x89, 0xc1});
    code.Rip({0x48, 0x8d, 0x35}, programPath);
    code.Rip({0x48, 0x8d, 0x3d}, modulePath);
    code.Emit({0xfc, 0xf3, 0xa4, 0x49, 0x89, 0xfc});
    code.Rip({0x48, 0x8d, 0x1d}, modulePath);
    const auto findSeparator = code.GetRva();
    code.Emit({0x49, 0x39, 0xdc});
    const auto hasDirectory = code.Branch({0x0f, 0x85});
    fail(2, 0xc000000du);
    code.PatchBranch(hasDirectory, code.GetRva());
    code.Emit({0x49, 0xff, 0xcc, 0x41, 0x80, 0x3c, 0x24, 0x5c});
    code.Rip({0x0f, 0x85}, findSeparator);
    code.Emit({0x49, 0xff, 0xc4});
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        if (absolutePath) {
            code.Rip({0x48, 0x8d, 0x0d}, libraryPaths[index]);
        } else {
            code.Emit({0x4c, 0x89, 0xe0, 0x48, 0x29, 0xd8, 0x48, 0x05});
            code.U32(CheckedRva(libraryNames[index].size() + 1));
            code.Emit({0x48, 0x3d});
            code.U32(PathCapacity);
            const auto fits = code.Branch({0x0f, 0x86});
            fail(1, 0xc0000106u);
            code.PatchBranch(fits, code.GetRva());
            code.Emit({0x4c, 0x89, 0xe7});
            code.Rip({0x48, 0x8d, 0x35}, libraryPaths[index]);
            code.Emit({0xb9});
            code.U32(CheckedRva(libraryNames[index].size() + 1));
            code.Emit({0xf3, 0xa4});
            code.Rip({0x48, 0x8d, 0x0d}, modulePath);
        }
        code.Emit({0x31, 0xd2, 0x41, 0xb8, 0, 0x11, 0, 0});
        call("LoadLibraryExA");
        requireNonzero(3 + index, 0xc0000135u);
        code.Rip({0x48, 0x89, 0x05}, CheckedRva(handles + index * 8));
    }
    for (std::size_t index = 0; index < imports.size(); ++index) {
        code.Rip({0x48, 0x8d, 0x1d}, handles);
        code.Rip({0x48, 0x8d, 0x35}, symbolNames[index]);
        code.Emit({0xbd});
        code.U32(CheckedRva(libraries.size()));
        const auto search = code.GetRva();
        code.Emit({0x48, 0x8b, 0x0b, 0x48, 0x89, 0xf2});
        call("GetProcAddress");
        code.Emit({0x48, 0x85, 0xc0});
        const auto resolved = code.Branch({0x0f, 0x85});
        code.Emit({0x48, 0x83, 0xc3, 8, 0xff, 0xcd});
        code.Rip({0x0f, 0x85}, search);
        fail(3 + libraries.size() + index, 0xc0000139u);
        code.PatchBranch(resolved, code.GetRva());
        if (imports[index].Addend != 0) {
            code.Emit({0x48, 0xba});
            code.U64(imports[index].Addend);
            code.Emit({0x48, 0x01, 0xd0});
        }
        code.Rip({0x48, 0x89, 0x05}, imports[index].TargetRva);
    }
    code.Emit({0x48, 0xc7, 0x44, 0x24, 0x40, 1, 0, 0, 0});
    code.Rip({0x48, 0x8d, 0x05}, programPath);
    code.Emit({0x48, 0x89, 0x44, 0x24, 0x48, 0x31, 0xc0, 0x48, 0x89, 0x44, 0x24, 0x50, 0x48, 0x89, 0x44, 0x24, 0x58, 0x48, 0x8d, 0x7c, 0x24, 0x40});
    const auto exitCallback = code.Branch({0x48, 0x8d, 0x35});
    code.Rip({0xe8}, entryRva);
    code.Emit({0x89, 0xc1});
    call("ExitProcess");
    code.Emit({0x0f, 0x0b});
    const auto functionEnd = code.GetRva();
    code.PatchBranch(exitCallback, functionEnd);
    code.Emit({0xc3});
    Io::WriteU32(data, functionTable - dataRva, result.Code.Rva);
    Io::WriteU32(data, functionTable - dataRva + 4, functionEnd);
    Io::WriteU32(data, functionTable - dataRva + 8, unwindRva);
    result.ExceptionDirectory = {functionTable, 12};
    result.Code.Data = code.TakeBytes();
    return result;
}

}
