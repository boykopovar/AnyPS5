#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <elfpatcher/windows/WindowsDependencyStubBuilder.hpp>
#include <elfpatcher/windows/WindowsGuestStartup.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <optional>
#include <filesystem>

namespace Elfpatcher::Windows {

namespace {

constexpr std::uint32_t PathCapacity = 32768;
constexpr std::uint32_t ErrorMessageCapacity = 32768;
constexpr std::uint32_t ArgumentCapacity = 32768;
constexpr std::uint32_t ArgumentTextCapacity = ArgumentCapacity * 3;

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

WindowsEntryStub WindowsEntryStubBuilder::Build(const std::uint32_t dataRva, const std::uint32_t entryRva, const WindowsImports& nativeImports, const std::vector<std::string>& libraries, const std::vector<PeImport>& imports, const std::string& runPath, const bool lazyBinding, const bool dependencyDiagnostics, const std::vector<Domain::GuestRuntime>& guestModules) const {
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
    const auto argumentBlock = reserve(8);
    const auto argumentCount = reserve(4);
    const auto wideArguments = reserve(8);
    const auto shellHandle = reserve(8);
    const auto shellLibrary = addString("shell32.dll");
    const auto parseArguments = addString("CommandLineToArgvW");
    const auto handles = reserve(libraries.size() * 8);
    const auto guestFinished = reserve(4);
    const WindowsGuestStartup guestStartup;
    Io::AlignBuffer(data, 4);
    const auto functionTable = reserve(12 * 32);
    const auto unwindRva = CheckedRva(dataRva + data.size());
    data.insert(data.end(), {1, 10, 6, 0, 10, 0xb2, 6, 0xc0, 4, 0x70, 3, 0x60, 2, 0x50, 1, 0x30});

    std::vector<std::uint32_t> libraryPaths;
    std::vector<std::string> libraryNames;
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        auto name = index < guestModules.size() ? libraries[index] : path + libraries[index];
        std::replace(name.begin(), name.end(), '/', '\\');
        if (name.size() + 1 >= PathCapacity)
            throw Domain::RelinkerException("Windows library path exceeds the startup buffer: " + name);
        libraryPaths.push_back(addString(name));
        libraryNames.push_back(std::move(name));
    }

    std::vector<std::uint32_t> symbolNames;
    for (const auto& import : imports)
        symbolNames.push_back(addString(import.Name));

    std::vector<std::uint32_t> searchTables;
    std::vector<std::uint32_t> searchCounts;
    std::vector<bool> platformTlsResolvers;
    for (const auto& import : imports) {
        std::vector<std::uint32_t> order;
        for (std::size_t index = 0; index < libraries.size(); ++index) {
            auto name = std::filesystem::path(libraries[index]).filename().string();
            if (name.ends_with(".guest.prx")) name.resize(name.size() - 10);
            const bool guestName = index < guestModules.size() &&
                std::find(guestModules[index].Names.begin(), guestModules[index].Names.end(), import.Library) != guestModules[index].Names.end();
            if (import.Library.empty() || name == import.Library || guestName) order.push_back(CheckedRva(index));
        }
        if (order.empty()) throw Domain::RelinkerException("Import module is not a dependency: " + import.Library);
        const bool hostModule = !import.Library.empty() &&
            std::any_of(order.begin(), order.end(), [&](auto index) { return index >= guestModules.size(); });
        if (hostModule) {
            for (std::size_t index = guestModules.size(); index < libraries.size(); ++index) {
                if (libraries[index] == "libc.prx" && import.Library != "libc.prx") order.push_back(CheckedRva(index));
            }
            if (import.Library == "libSceLibcInternal.prx") {
                for (std::size_t index = 0; index < guestModules.size(); ++index) {
                    const auto& names = guestModules[index].Names;
                    if (std::find(names.begin(), names.end(), "libc.prx") != names.end())
                        order.push_back(CheckedRva(index));
                }
            }
        }
        Io::AlignBuffer(data, 4);
        platformTlsResolvers.push_back(std::none_of(order.begin(), order.end(), [&](auto index) {
            return index < guestModules.size() && !guestModules[index].UsePlatformTlsResolver;
        }));
        searchTables.push_back(CheckedRva(dataRva + data.size()));
        searchCounts.push_back(CheckedRva(order.size()));
        for (const auto index : order) Io::AppendU32(data, index);
    }

    const auto lastError = reserve(4);
    const auto errorDigits = reserve(11);
    const auto errorMessage = reserve(ErrorMessageCapacity);
    const auto loading = addString("Loading PRX: ");
    const auto loaded = addString(" -> OK\n");
    const auto loadFailed = addString(" -> FAILED\n");
    const auto failedModule = addString("Failed to load module: ");
    const auto errorPrefix = addString("GetLastError: ");
    const auto messageSeparator = addString(" - ");
    const auto newline = addString("\n");
    const auto searched = addString("Searched libraries:\n");
    const auto indent = addString("  ");
    const auto enteringElf = addString("Transferring control to ELF entry point\n");
    std::vector<std::uint32_t> resolvedPaths;
    for (std::size_t index = 0; index < libraries.size(); ++index)
        resolvedPaths.push_back(reserve(PathCapacity));

    const auto diagnosticsOffset = data.size();
    std::vector<std::string> errors = {"FAIL: cannot obtain executable path\n", "FAIL: executable or library path is too long\n", "FAIL: executable path has no directory\n"};
    for (const auto& import : imports)
        errors.push_back("FAIL: unresolved ELF import " + import.Name +
            (import.Library.empty() ? std::string{} : " from " + import.Library) + "\n");
    const auto argumentError = errors.size();
    errors.push_back("FAIL: cannot prepare command-line arguments\n");
    const auto directoryError = errors.size();
    errors.push_back("FAIL: cannot make the executable directory the current directory\n");
    std::vector<std::uint32_t> errorRvas;
    for (const auto& error : errors)
        errorRvas.push_back(addString(error));
    if (data.size() <= diagnosticsOffset)
        throw Domain::RelinkerException("Empty startup diagnostics");

    std::optional<WindowsDependencyStubBuilder> dependencyBuilder;
    if (dependencyDiagnostics)
        dependencyBuilder.emplace(result.Data);
    std::vector<std::size_t> dependencyCalls;
    result.Code.Rva = AlignRva(dataRva + data.size());
    WindowsStubEmitter code(result.Code.Rva);

    const auto call = [&](const std::string& name) { code.Rip({0xff, 0x15}, nativeImports.Functions.at(name)); };

    const auto raise = [&](const std::uint32_t status) {
        code.Emit({0xb9});
        code.U32(status);
        code.Emit({0xba, 1, 0, 0, 0, 0x45, 0x31, 0xc0, 0x45, 0x31, 0xc9});
        call("RaiseException");
        code.Emit({0x0f, 0x0b});
    };

    const auto writeString = [&](const std::uint32_t stringRva, const bool isError = false) {
        code.Rip({0x48, 0x8d, 0x0d}, stringRva);
        call("lstrlenA");
        code.Emit({0x48, 0x85, 0xc0});
        const auto empty = code.Branch({0x0f, 0x84});
        code.Emit({0x89, 0x44, 0x24, 0x3c, 0xb9});
        code.U32(isError ? 0xfffffff4u : 0xfffffff5u);
        call("GetStdHandle");
        code.Emit({0x48, 0x85, 0xc0});
        const auto noHandle = code.Branch({0x0f, 0x84});
        code.Emit({0x48, 0x83, 0xf8, 0xff});
        const auto invalidHandle = code.Branch({0x0f, 0x84});
        code.Emit({0x48, 0x89, 0xc1});
        code.Rip({0x48, 0x8d, 0x15}, stringRva);
        code.Emit({0x44, 0x8b, 0x44, 0x24, 0x3c, 0x4c, 0x8d, 0x4c, 0x24, 0x38, 0x48, 0xc7, 0x44, 0x24, 0x20, 0, 0, 0, 0});
        call("WriteFile");
        code.Emit({0x48, 0x85, 0xc0});
        const auto failedWrite = code.Branch({0x0f, 0x84});
        code.Emit({0x8b, 0x44, 0x24, 0x38, 0x3b, 0x44, 0x24, 0x3c});
        const auto incompleteWrite = code.Branch({0x0f, 0x85});
        const auto done = code.GetRva();
        code.PatchBranch(empty, done);
        code.PatchBranch(noHandle, done);
        code.PatchBranch(invalidHandle, done);
        code.PatchBranch(failedWrite, done);
        code.PatchBranch(incompleteWrite, done);
    };

    const auto writeLastError = [&] {
        writeString(errorPrefix, true);
        code.Rip({0x8b, 0x05}, lastError);
        code.Rip({0x48, 0x8d, 0x3d}, errorDigits + 10);
        code.Emit({0xc6, 0x07, 0, 0x41, 0xb8, 10, 0, 0, 0});
        const auto digit = code.GetRva();
        code.Emit({0x31, 0xd2, 0x41, 0xf7, 0xf0, 0x80, 0xc2, 0x30, 0x48, 0xff, 0xcf, 0x88, 0x17, 0x85, 0xc0});
        code.Rip({0x0f, 0x85}, digit);
        code.Emit({0x48, 0x89, 0xfe});
        code.Rip({0x48, 0x8d, 0x0d}, errorDigits + 11);
        code.Emit({0x48, 0x29, 0xf9});
        code.Rip({0x48, 0x8d, 0x3d}, errorDigits);
        code.Emit({0xf3, 0xa4});
        writeString(errorDigits, true);
        writeString(messageSeparator, true);
        code.Emit({0xb9, 0, 0x12, 0, 0, 0x31, 0xd2});
        code.Rip({0x44, 0x8b, 0x05}, lastError);
        code.Emit({0x41, 0xb9, 9, 4, 0, 0});
        code.Rip({0x48, 0x8d, 0x05}, errorMessage);
        code.Emit({0x48, 0x89, 0x44, 0x24, 0x20, 0x48, 0xc7, 0x44, 0x24, 0x28});
        code.U32(ErrorMessageCapacity);
        code.Emit({0x48, 0xc7, 0x44, 0x24, 0x30, 0, 0, 0, 0});
        call("FormatMessageA");
        code.Emit({0x48, 0x85, 0xc0});
        const auto noMessage = code.Branch({0x0f, 0x84});
        writeString(errorMessage, true);
        code.PatchBranch(noMessage, code.GetRva());
    };

    const auto captureLastError = [&] {
        call("GetLastError");
        code.Rip({0x89, 0x05}, lastError);
    };

    const auto fail = [&](const std::size_t error, const std::uint32_t status) {
        writeString(errorRvas.at(error), true);
        raise(status);
    };

    const auto requireNonzero = [&](const std::size_t error, const std::uint32_t status) {
        code.Emit({0x48, 0x85, 0xc0});
        const auto success = code.Branch({0x0f, 0x85});
        captureLastError();
        writeString(errorRvas.at(error), true);
        writeLastError();
        raise(status);
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
    code.Emit({0x41, 0xc6, 0x04, 0x24, 0x00});
    code.Rip({0x48, 0x8d, 0x0d}, modulePath);
    call("SetCurrentDirectoryA");
    requireNonzero(directoryError, 0xc000000du);

    for (std::size_t index = 0; index < libraries.size(); ++index) {
        if (absolutePath && index >= guestModules.size()) {
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
        code.Emit({0x48, 0x89, 0xce});
        call("lstrlenA");
        code.Emit({0x8d, 0x48, 1});
        code.Rip({0x48, 0x8d, 0x3d}, resolvedPaths[index]);
        code.Emit({0xf3, 0xa4});

        if (dependencyDiagnostics) {
            writeString(loading);
            writeString(resolvedPaths[index]);
        }

        code.Rip({0x48, 0x8d, 0x0d}, resolvedPaths[index]);
        code.Emit({0x31, 0xd2, 0x41, 0xb8, 0, 0x11, 0, 0});
        call("LoadLibraryExA");
        code.Emit({0x48, 0x85, 0xc0});
        const auto loadSucceeded = code.Branch({0x0f, 0x85});
        captureLastError();
        writeString(loadFailed, true);
        writeString(failedModule, true);
        writeString(resolvedPaths[index], true);
        writeString(newline, true);
        writeLastError();
        if (dependencyDiagnostics) {
            code.Rip({0x48, 0x8d, 0x0d}, resolvedPaths[index]);
            code.Rip({0x48, 0x8d, 0x15}, programPath);
            dependencyCalls.push_back(code.Branch({0xe8}));
        }
        raise(0xc0000135u);
        code.PatchBranch(loadSucceeded, code.GetRva());
        code.Rip({0x48, 0x89, 0x05}, CheckedRva(handles + index * 8));

        if (dependencyDiagnostics) {
            writeString(loaded);
        }
    }

    std::vector<std::size_t> unresolvedBranches;
    std::vector<std::size_t> lazyUnresolvedImports;
    std::vector<std::size_t> tlsResolverAddresses;
    for (std::size_t index = 0; index < imports.size(); ++index) {
        if (!guestModules.empty() && platformTlsResolvers[index] && imports[index].Name == "vNe1w4diLCs") {
            if (imports[index].Addend != 0) throw Domain::RelinkerException("TLS resolver import has an addend");
            tlsResolverAddresses.push_back(code.Branch({0x48, 0x8d, 0x05}));
            guestStartup.WriteImport(code, imports[index], handles);
            continue;
        }
        code.Rip({0x48, 0x8d, 0x1d}, handles);
        code.Rip({0x48, 0x8d, 0x35}, symbolNames[index]);
        code.Rip({0x48, 0x8d, 0x3d}, searchTables[index]);
        code.Emit({0xbd});
        code.U32(searchCounts[index]);
        const auto search = code.GetRva();
        code.Emit({0x8b, 0x0f, 0x48, 0x8b, 0x0c, 0xcb, 0x48, 0x89, 0xf2});
        call("GetProcAddress");
        code.Emit({0x48, 0x85, 0xc0});
        const auto resolved = code.Branch({0x0f, 0x85});
        code.Emit({0x48, 0x83, 0xc7, 4, 0xff, 0xcd});
        code.Rip({0x0f, 0x85}, search);
        if (lazyBinding) {
            lazyUnresolvedImports.push_back(index);
            const auto skipGotWrite = code.Branch({0xe9});
            code.PatchBranch(resolved, code.GetRva());
            if (imports[index].Addend != 0) {
                code.Emit({0x48, 0xba});
                code.U64(imports[index].Addend);
                code.Emit({0x48, 0x01, 0xd0});
            }
            guestStartup.WriteImport(code, imports[index], handles);
            code.PatchBranch(skipGotWrite, code.GetRva());
            continue;
        }
        captureLastError();
        writeString(errorRvas.at(3 + index), true);
        unresolvedBranches.push_back(code.Branch({0xe9}));
        code.PatchBranch(resolved, code.GetRva());
        if (imports[index].RelocationType == 16) code.Emit({0x48, 0x8b, 0x00});
        if (imports[index].RelocationType == 17) code.Emit({0x48, 0x8b, 0x40, 8});
        if (imports[index].Addend != 0) {
            code.Emit({0x48, 0xba});
            code.U64(imports[index].Addend);
            code.Emit({0x48, 0x01, 0xd0});
        }
        guestStartup.WriteImport(code, imports[index], handles);
    }

    guestStartup.Initialize(code, guestModules, handles);
    writeString(enteringElf);
    code.Rip({0x48, 0x8d, 0x0d}, shellLibrary);
    code.Emit({0x31, 0xd2, 0x41, 0xb8, 0, 8, 0, 0});
    call("LoadLibraryExA");
    requireNonzero(argumentError, 0xc0000135u);
    code.Rip({0x48, 0x89, 0x05}, shellHandle);
    code.Emit({0x48, 0x89, 0xc1});
    code.Rip({0x48, 0x8d, 0x15}, parseArguments);
    call("GetProcAddress");
    requireNonzero(argumentError, 0xc0000139u);
    code.Emit({0x48, 0x89, 0xc3});
    call("GetCommandLineW");
    code.Emit({0x48, 0x89, 0xc1});
    code.Rip({0x48, 0x8d, 0x15}, argumentCount);
    code.Emit({0xff, 0xd3});
    requireNonzero(argumentError, 0xc000000du);
    code.Rip({0x48, 0x89, 0x05}, wideArguments);
    code.Emit({0x48, 0x89, 0xc3});
    code.Rip({0x8b, 0x2d}, argumentCount);
    code.Emit({0x8d, 0x45, 0xff, 0x3d});
    code.U32(ArgumentCapacity - 1);
    const auto countFits = code.Branch({0x0f, 0x82});
    fail(argumentError, 0xc000000du);
    code.PatchBranch(countFits, code.GetRva());
    code.Emit({0x31, 0xc9, 0xba});
    code.U32((ArgumentCapacity + 3) * 8 + ArgumentTextCapacity);
    code.Emit({0x41, 0xb8, 0, 0x30, 0, 0, 0x41, 0xb9, 4, 0, 0, 0});
    call("VirtualAlloc");
    requireNonzero(argumentError, 0xc0000017u);
    code.Rip({0x48, 0x89, 0x05}, argumentBlock);
    code.Emit({0x48, 0x89, 0x28, 0x48, 0x8d, 0x70, 8, 0x48, 0x8d, 0xb8});
    code.U32((ArgumentCapacity + 3) * 8);
    code.Emit({0x41, 0xbc});
    code.U32(ArgumentTextCapacity);
    const auto convertArgument = code.GetRva();
    code.Emit({0x48, 0x89, 0x3e, 0xb9});
    code.U32(65001);
    code.Emit({0xba, 0x80, 0, 0, 0, 0x4c, 0x8b, 0x03, 0x41, 0xb9, 0xff, 0xff, 0xff, 0xff});
    code.Emit({0x48, 0x89, 0x7c, 0x24, 0x20, 0x44, 0x89, 0x64, 0x24, 0x28});
    code.Emit({0x48, 0xc7, 0x44, 0x24, 0x30, 0, 0, 0, 0, 0x48, 0xc7, 0x44, 0x24, 0x38, 0, 0, 0, 0});
    call("WideCharToMultiByte");
    requireNonzero(argumentError, 0xc000000du);
    code.Emit({0x48, 0x01, 0xc7, 0x41, 0x29, 0xc4, 0x48, 0x83, 0xc3, 8, 0x48, 0x83, 0xc6, 8, 0xff, 0xcd});
    code.Rip({0x0f, 0x85}, convertArgument);
    code.Rip({0x48, 0x8b, 0x0d}, wideArguments);
    call("LocalFree");
    code.Rip({0x48, 0x8b, 0x0d}, shellHandle);
    call("FreeLibrary");
    code.Rip({0x48, 0x8b, 0x3d}, argumentBlock);
    const auto exitCallback = code.Branch({0x48, 0x8d, 0x35});
    code.Rip({0xe8}, entryRva);
    code.Emit({0x89, 0x44, 0x24, 0x58});
    guestStartup.Finalize(code, guestModules, handles, guestFinished);
    code.Emit({0x8b, 0x44, 0x24, 0x58});
    code.Emit({0x89, 0xc1});
    call("ExitProcess");
    code.Emit({0x0f, 0x0b});

    if (!unresolvedBranches.empty()) {
        for (const auto branch : unresolvedBranches)
            code.PatchBranch(branch, code.GetRva());
        writeString(searched, true);
        for (const auto resolvedPath : resolvedPaths) {
            writeString(indent, true);
            writeString(resolvedPath, true);
            writeString(newline, true);
        }
        writeLastError();
        raise(0xc0000139u);
    }

    const auto functionEnd = code.GetRva();
    code.PatchBranch(exitCallback, functionEnd);
    code.Emit({0x48, 0x83, 0xec, 0x28});
    guestStartup.Finalize(code, guestModules, handles, guestFinished);
    code.Emit({0x48, 0x83, 0xc4, 0x28});
    code.Emit({0xc3});
    const auto tlsResolver = guestStartup.EmitTlsResolver(code);
    for (const auto offset : tlsResolverAddresses) code.PatchBranch(offset, tlsResolver);

    for (const auto index : lazyUnresolvedImports) {
        const auto stubRva = code.GetRva();
        writeString(errorRvas.at(3 + index), true);
        raise(0xc0000139u);
        result.LazyStubs.push_back({imports[index].TargetRva, stubRva});
    }

    Io::WriteU32(data, functionTable - dataRva, result.Code.Rva);
    Io::WriteU32(data, functionTable - dataRva + 4, functionEnd);
    Io::WriteU32(data, functionTable - dataRva + 8, unwindRva);
    auto exceptionEntries = CheckedRva(1);
    if (dependencyBuilder.has_value()) {
        const auto dependency = dependencyBuilder->Build(code, nativeImports);
        for (const auto offset : dependencyCalls)
            code.PatchBranch(offset, dependency.EntryRva);
        if (dependency.Functions.size() >= 32)
            throw Domain::RelinkerException("Too many dependency diagnostic routines");
        for (std::size_t index = 0; index < dependency.Functions.size(); ++index) {
            for (std::size_t field = 0; field < 3; ++field)
                Io::WriteU32(data, functionTable - dataRva + (index + 1) * 12 + field * 4, dependency.Functions[index][field]);
        }
        exceptionEntries = CheckedRva(dependency.Functions.size() + 1);
    }
    result.ExceptionDirectory = {functionTable, CheckedRva(exceptionEntries * 12)};
    result.Code.Data = code.TakeBytes();
    return result;
}

}
