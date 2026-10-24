#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <elfpatcher/windows/WindowsDependencyStubBuilder.hpp>
#include <elfpatcher/windows/WindowsGuestStartup.hpp>
#include <domain/ImportModule.hpp>
#include <io/BufferUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
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
    if (path.empty())
        path = ".";
    if (path.find('$') != std::string::npos)
        throw Domain::RelinkerException("Unsupported variable in Windows run path");
    if (path.back() != '\\')
        path.push_back('\\');
    return path;
}

std::u16string toUtf16(const std::string& text) {
    static constexpr std::uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
    std::u16string result;
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index]);
        const std::size_t length = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 : (lead & 0xf0) == 0xe0 ? 3 : (lead & 0xf8) == 0xf0 ? 4 : 0;
        if (length == 0 || text.size() - index < length)
            throw Domain::RelinkerException("Windows library path is not valid UTF-8: " + text);
        std::uint32_t code = length == 1 ? lead : lead & (0x7fu >> length);
        for (std::size_t next = 1; next < length; ++next) {
            const auto continuation = static_cast<unsigned char>(text[index + next]);
            if ((continuation & 0xc0) != 0x80)
                throw Domain::RelinkerException("Windows library path is not valid UTF-8: " + text);
            code = code << 6 | (continuation & 0x3f);
        }
        if (code < minimum[length] || code > 0x10ffff || (code >= 0xd800 && code < 0xe000))
            throw Domain::RelinkerException("Windows library path is not valid UTF-8: " + text);
        if (code >= 0x10000) {
            code -= 0x10000;
            result.push_back(static_cast<char16_t>(0xd800 + (code >> 10)));
            result.push_back(static_cast<char16_t>(0xdc00 + (code & 0x3ff)));
        } else {
            result.push_back(static_cast<char16_t>(code));
        }
        index += length;
    }
    return result;
}

}

WindowsEntryStub WindowsEntryStubBuilder::Build(const std::uint32_t dataRva, const std::uint32_t entryRva, const WindowsImports& nativeImports, const std::vector<std::string>& libraries, const std::vector<PeImport>& imports, const std::string& runPath, const bool lazyBinding, const bool dependencyDiagnostics, const std::vector<Domain::GuestRuntime>& guestModules, const std::set<std::string>& unmatchedImportModules, const bool traceImports) const {
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
    const auto addWideString = [&](const std::u16string& value) {
        Io::AlignBuffer(data, 2);
        const auto rva = CheckedRva(dataRva + data.size());
        for (const auto unit : value)
            Io::AppendU16(data, unit);
        Io::AppendU16(data, 0);
        return rva;
    };

    const auto parentVariable = addWideString(u"ANYPS5_LOADEXEC_PARENT");
    const auto allocatorVariable = addWideString(u"APS5_MIMALLOC");
    const auto parentText = reserve(34);
    const auto skipFirstArgument = reserve(4);
    const auto programPath = reserve(PathCapacity * 2);
    const auto modulePath = reserve(PathCapacity * 2);
    const auto argumentBlock = reserve(8);
    const auto argumentCount = reserve(4);
    const auto wideArguments = reserve(8);
    const auto shellHandle = reserve(8);
    const auto logHandle = reserve(8);
    const std::u16string logFileName = u"anyps5.log";
    const auto logName = addWideString(logFileName);
    const auto shellLibrary = addString("shell32.dll");
    const auto parseArguments = addString("CommandLineToArgvW");
    const auto runtimeModule = reserve(8);
    const auto runtimeIobFunction = reserve(8);
    const auto runtimeFreopenFunction = reserve(8);
    const auto msvcrtLibrary = addWideString(u"msvcrt.dll");
    const auto ucrtbaseLibrary = addWideString(u"ucrtbase.dll");
    const auto msvcrtIobFunctionName = addString("__iob_func");
    const auto ucrtbaseIobFunctionName = addString("__acrt_iob_func");
    const auto runtimeFreopenName = addString("_wfreopen");
    const auto runtimeAppendMode = addWideString(u"a");
    const auto handles = reserve(libraries.size() * 8);
    const auto guestFinished = reserve(4);
    const WindowsGuestStartup guestStartup;
    Io::AlignBuffer(data, 4);
    const auto functionTable = reserve(12 * 32);
    const auto unwindRva = CheckedRva(dataRva + data.size());
    data.insert(data.end(), {1, 10, 6, 0, 10, 0xb2, 6, 0xc0, 4, 0x70, 3, 0x60, 2, 0x50, 1, 0x30});

    std::vector<std::uint32_t> allocatorPaths;
    std::vector<std::size_t> allocatorUnits;
    for (const std::string library : {"mimalloc.dll"}) {
        const auto name = path + library;
        const auto wideName = toUtf16(name);
        if (wideName.size() + 1 >= PathCapacity)
            throw Domain::RelinkerException("Windows library path exceeds the startup buffer: " + name);
        allocatorPaths.push_back(addWideString(wideName));
        allocatorUnits.push_back(wideName.size());
    }

    std::vector<std::uint32_t> libraryPaths;
    std::vector<std::size_t> libraryUnits;
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        auto name = index < guestModules.size() ? libraries[index] : path + libraries[index];
        std::replace(name.begin(), name.end(), '/', '\\');
        const auto wideName = toUtf16(name);
        if (wideName.size() + 1 >= PathCapacity)
            throw Domain::RelinkerException("Windows library path exceeds the startup buffer: " + name);
        libraryPaths.push_back(addWideString(wideName));
        libraryUnits.push_back(wideName.size());
    }

    std::vector<std::uint32_t> symbolNames;
    std::vector<std::optional<std::uint32_t>> nidNames;
    for (const auto& import : imports) {
        symbolNames.push_back(addString(import.Name));
        const bool named = import.Library.empty() && !import.Name.empty() && import.Name.find('#') == std::string::npos;
        nidNames.push_back(named ? std::optional(addString(Nid::ComputeNid(import.Name, ""))) : std::nullopt);
    }

    std::vector<std::uint32_t> searchTables;
    std::vector<std::uint32_t> searchCounts;
    std::vector<bool> platformTlsResolvers;
    std::vector<bool> imported(libraries.size());
    std::vector<bool> executableImports(guestModules.size());
    const bool unscopedImports = std::any_of(imports.begin(), imports.end(), [](const auto& import) { return import.Library.empty(); });
    for (const auto& import : imports) {
        const auto library = Domain::NeededLibraryName(import.Library);
        std::vector<std::uint32_t> order;
        for (std::size_t index = 0; index < libraries.size(); ++index) {
            auto name = std::filesystem::path(libraries[index]).filename().string();
            if (name.ends_with(".guest.prx")) name.resize(name.size() - 10);
            const bool guestName = index < guestModules.size() &&
                std::find(guestModules[index].Names.begin(), guestModules[index].Names.end(), import.Library) != guestModules[index].Names.end();
            if (import.Library.empty() || name == library || guestName) order.push_back(CheckedRva(index));
            if (import.TargetModule < 0 && !import.Library.empty() && index < guestModules.size() && (name == library || guestName)) executableImports[index] = true;
        }
        if (order.empty()) throw Domain::RelinkerException("Import module is not a dependency: " + import.Library);
        const bool hostModule = !import.Library.empty() &&
            std::any_of(order.begin(), order.end(), [&](auto index) { return index >= guestModules.size(); });
        if (hostModule) {
            if (library == "libkernel_sys.prx" || library == "libkernel_web.prx") {
                for (std::size_t index = guestModules.size(); index < libraries.size(); ++index) {
                    if (libraries[index] == "libkernel.prx") order.push_back(CheckedRva(index));
                }
            }
            for (std::size_t index = guestModules.size(); index < libraries.size(); ++index) {
                if (libraries[index] == "libc.prx" && library != "libc.prx") order.push_back(CheckedRva(index));
            }
            if (library == "libSceLibcInternal.prx") {
                for (std::size_t index = 0; index < guestModules.size(); ++index) {
                    const auto& names = guestModules[index].Names;
                    if (std::find(names.begin(), names.end(), "libc.prx") != names.end() && !guestModules[index].ZeroReturnExports.contains(import.Name))
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
        for (const auto index : order) {
            Io::AppendU32(data, index);
            if (!import.Library.empty()) imported[index] = true;
        }
    }

    const auto stem = [](const std::string& name) {
        auto result = std::filesystem::path(name).filename().string();
        result.resize(std::min(result.size(), result.find('.')));
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return result;
    };
    std::set<std::string> unmatched;
    for (const auto& module : unmatchedImportModules) unmatched.insert(stem(module));
    for (const auto& guest : guestModules) {
        for (const auto& module : guest.UnmatchedImportModules) unmatched.insert(stem(module));
    }
    for (std::size_t index = guestModules.size(); index < libraries.size(); ++index) {
        if (unmatched.contains(stem(libraries[index]))) imported[index] = true;
    }

    std::optional<std::size_t> traceProvider;
    std::map<std::string, std::uint32_t> traceLibraries;
    std::uint32_t traceSymbol = 0;
    std::uint32_t traceFunction = 0;
    if (traceImports) {
        for (std::size_t index = guestModules.size(); index < libraries.size(); ++index) {
            if (libraries[index] == "libc.prx") traceProvider = index;
        }
        if (!traceProvider.has_value())
            throw Domain::RelinkerException("--trace-imports requires libc.prx among the libraries");
        imported[*traceProvider] = true;
        traceSymbol = addString("TraceImport_nid_no_patch");
        traceFunction = reserve(8);
        for (const auto& import : imports) {
            if (!traceLibraries.contains(import.Library)) traceLibraries.emplace(import.Library, addString(import.Library));
        }
    }

    const auto lastError = reserve(4);
    const auto unresolvedCount = reserve(4);
    const auto errorDigits = reserve(11);
    const auto errorMessage = reserve(ErrorMessageCapacity);
    const auto loading = addString("Loading PRX: ");
    const auto loaded = addString(" -> OK\n");
    const auto loadFailed = addString(" -> FAILED\n");
    const auto failedModule = addString("Failed to load module: ");
    const auto skippedModule = addString(" -> not found; nothing is imported from it, continuing without it\n");
    const auto errorPrefix = addString("GetLastError: ");
    const auto moduleNotFoundHint = addString("hint: the module or one of its dependent DLLs was not found; a self-built libs/ also needs libgcc_s_seh-1.dll, libstdc++-6.dll and libwinpthread-1.dll from the toolchain's mingw64/bin next to it\n");
    const auto messageSeparator = addString(" - ");
    const auto newline = addString("\n");
    const auto searched = addString("Searched libraries:\n");
    const auto indent = addString("  ");
    const auto enteringElf = addString("Transferring control to ELF entry point\n");
    std::vector<std::uint32_t> deferredImports;
    for (std::size_t index = 0; index < guestModules.size(); ++index) {
        if (guestModules[index].DeferInitialization && executableImports[index])
            deferredImports.push_back(addString("Deferred module " + guestModules[index].Path + ": the executable imports from it, but it starts only when the title loads it\n"));
    }
    std::vector<std::uint32_t> resolvedPaths;
    for (std::size_t index = 0; index < libraries.size(); ++index)
        resolvedPaths.push_back(reserve(PathCapacity));
    const std::uint32_t programText = dependencyDiagnostics ? reserve(PathCapacity) : 0;

    const auto diagnosticsOffset = data.size();
    std::vector<std::string> errors = {"FAIL: cannot obtain executable path\n", "FAIL: executable or library path is too long\n", "FAIL: executable path has no directory\n"};
    for (const auto& import : imports)
        errors.push_back("FAIL: unresolved ELF import " + import.Name +
            (import.Library.empty() ? std::string{} : " from " + import.Library) + "\n");
    const auto argumentError = errors.size();
    errors.push_back("FAIL: cannot prepare command-line arguments\n");
    const auto parentError = errors.size();
    errors.push_back("FAIL: cannot wait for the previous LoadExec process\n");
    const auto directoryError = errors.size();
    errors.push_back("FAIL: cannot make the executable directory the current directory\n");
    const auto traceError = errors.size();
    errors.push_back("FAIL: libc.prx does not export TraceImport_nid_no_patch\n");
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

    const auto writeUtf8 = [&](const std::uint32_t destination) {
        code.Emit({0xb9});
        code.U32(65001);
        code.Emit({0x31, 0xd2, 0x41, 0xb9, 0xff, 0xff, 0xff, 0xff});
        code.Rip({0x48, 0x8d, 0x05}, destination);
        code.Emit({0x48, 0x89, 0x44, 0x24, 0x20, 0x48, 0xc7, 0x44, 0x24, 0x28});
        code.U32(PathCapacity);
        code.Emit({0x48, 0xc7, 0x44, 0x24, 0x30, 0, 0, 0, 0, 0x48, 0xc7, 0x44, 0x24, 0x38, 0, 0, 0, 0});
        call("WideCharToMultiByte");
        code.Emit({0x85, 0xc0});
        const auto converted = code.Branch({0x0f, 0x85});
        fail(1, 0xc0000106u);
        code.PatchBranch(converted, code.GetRva());
    };

    const auto logMissingHandle = [&](const std::uint32_t standardHandle) {
        const auto units = CheckedRva(logFileName.size() + 1);
        code.Emit({0xb9});
        code.U32(standardHandle);
        call("GetStdHandle");
        code.Emit({0x48, 0x85, 0xc0});
        const auto missing = code.Branch({0x0f, 0x84});
        code.Emit({0x48, 0x83, 0xf8, 0xff});
        const auto present = code.Branch({0x0f, 0x85});
        code.PatchBranch(missing, code.GetRva());
        code.Rip({0x48, 0x8b, 0x05}, logHandle);
        code.Emit({0x48, 0x85, 0xc0});
        const auto opened = code.Branch({0x0f, 0x85});
        code.Emit({0x4c, 0x89, 0xe0, 0x48, 0x29, 0xd8, 0x48, 0x05});
        code.U32(units * 2);
        code.Emit({0x48, 0x3d});
        code.U32(PathCapacity * 2);
        const auto tooLong = code.Branch({0x0f, 0x87});
        code.Emit({0x4c, 0x89, 0xe7});
        code.Rip({0x48, 0x8d, 0x35}, logName);
        code.Emit({0xb9});
        code.U32(units);
        code.Emit({0x66, 0xf3, 0xa5});
        code.Rip({0x48, 0x8d, 0x0d}, modulePath);
        code.Emit({0xba, 4, 0, 0, 0, 0x41, 0xb8, 3, 0, 0, 0, 0x45, 0x31, 0xc9});
        code.Emit({0x48, 0xc7, 0x44, 0x24, 0x20, 2, 0, 0, 0, 0x48, 0xc7, 0x44, 0x24, 0x28, 0x80, 0, 0, 0, 0x48, 0xc7, 0x44, 0x24, 0x30, 0, 0, 0, 0});
        call("CreateFileW");
        code.Emit({0x66, 0x41, 0xc7, 0x04, 0x24, 0x00, 0x00});
        code.Emit({0x48, 0x83, 0xf8, 0xff});
        const auto failed = code.Branch({0x0f, 0x84});
        code.Rip({0x48, 0x89, 0x05}, logHandle);
        code.PatchBranch(opened, code.GetRva());
        code.Emit({0xb9});
        code.U32(standardHandle);
        code.Emit({0x48, 0x89, 0xc2});
        call("SetStdHandle");
        const auto done = code.GetRva();
        code.PatchBranch(present, done);
        code.PatchBranch(tooLong, done);
        code.PatchBranch(failed, done);
    };

    const auto rebindPreloadedRuntimeLog = [&](const std::uint32_t runtimeLibrary, const std::uint32_t iobFunctionName, const bool indexedIob) {
        code.Rip({0x48, 0x8d, 0x0d}, runtimeLibrary);
        call("GetModuleHandleW");
        code.Emit({0x48, 0x85, 0xc0});
        std::vector<std::size_t> absent = {code.Branch({0x0f, 0x84})};
        code.Rip({0x48, 0x89, 0x05}, runtimeModule);
        code.Emit({0x48, 0x89, 0xc1});
        code.Rip({0x48, 0x8d, 0x15}, iobFunctionName);
        call("GetProcAddress");
        code.Emit({0x48, 0x85, 0xc0});
        absent.push_back(code.Branch({0x0f, 0x84}));
        code.Rip({0x48, 0x89, 0x05}, runtimeIobFunction);
        code.Rip({0x48, 0x8b, 0x0d}, runtimeModule);
        code.Rip({0x48, 0x8d, 0x15}, runtimeFreopenName);
        call("GetProcAddress");
        code.Emit({0x48, 0x85, 0xc0});
        absent.push_back(code.Branch({0x0f, 0x84}));
        code.Rip({0x48, 0x89, 0x05}, runtimeFreopenFunction);
        for (const auto& stream : {std::pair<std::uint32_t, std::uint8_t>{0xfffffff5u, 1}, {0xfffffff4u, 2}}) {
            code.Emit({0xb9});
            code.U32(stream.first);
            call("GetStdHandle");
            code.Emit({0x48, 0x85, 0xc0});
            const auto missing = code.Branch({0x0f, 0x84});
            code.Rip({0x48, 0x3b, 0x05}, logHandle);
            const auto notRedirected = code.Branch({0x0f, 0x85});
            if (indexedIob) {
                code.Emit({0xb9, stream.second, 0, 0, 0});
                code.Rip({0xff, 0x15}, runtimeIobFunction);
                code.Emit({0x49, 0x89, 0xc0});
            } else {
                code.Rip({0xff, 0x15}, runtimeIobFunction);
                code.Emit({0x4c, 0x8d, 0x40, static_cast<std::uint8_t>(48 * stream.second)});
            }
            code.Rip({0x48, 0x8d, 0x0d}, logName);
            code.Rip({0x48, 0x8d, 0x15}, runtimeAppendMode);
            code.Rip({0xff, 0x15}, runtimeFreopenFunction);
            const auto next = code.GetRva();
            code.PatchBranch(missing, next);
            code.PatchBranch(notRedirected, next);
        }
        const auto done = code.GetRva();
        for (const auto branch : absent) code.PatchBranch(branch, done);
    };

    code.Emit({0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x48, 0x83, 0xec, 0x60});
    code.Rip({0x48, 0x8d, 0x0d}, parentVariable);
    code.Rip({0x48, 0x8d, 0x15}, parentText);
    code.Emit({0x41, 0xb8, 17, 0, 0, 0});
    call("GetEnvironmentVariableW");
    code.Emit({0x85, 0xc0});
    const auto noParent = code.Branch({0x0f, 0x84});
    code.Emit({0x83, 0xf8, 16});
    const auto shortValue = code.Branch({0x0f, 0x86});
    fail(parentError, 1);
    code.PatchBranch(shortValue, code.GetRva());
    code.Rip({0xc7, 0x05}, skipFirstArgument - 4);
    code.U32(1);
    code.Rip({0x48, 0x8d, 0x35}, parentText);
    code.Emit({0x31, 0xff});
    const auto digit = code.GetRva();
    code.Emit({0x0f, 0xb7, 0x06, 0x85, 0xc0});
    const auto parsed = code.Branch({0x0f, 0x84});
    code.Emit({0x83, 0xe8, 0x30, 0x83, 0xf8, 9});
    const auto decimal = code.Branch({0x0f, 0x86});
    code.Emit({0x83, 0xe8, 0x31, 0x83, 0xf8, 5});
    const auto hex = code.Branch({0x0f, 0x86});
    fail(parentError, 1);
    code.PatchBranch(hex, code.GetRva());
    code.Emit({0x83, 0xc0, 10});
    code.PatchBranch(decimal, code.GetRva());
    code.Emit({0x48, 0xc1, 0xe7, 4, 0x48, 0x09, 0xc7, 0x48, 0x83, 0xc6, 2});
    code.Rip({0xe9}, digit);
    code.PatchBranch(parsed, code.GetRva());
    code.Emit({0x48, 0x89, 0xf9, 0xba, 0xff, 0xff, 0xff, 0xff});
    call("WaitForSingleObject");
    code.Emit({0x85, 0xc0});
    const auto waited = code.Branch({0x0f, 0x84});
    fail(parentError, 1);
    code.PatchBranch(waited, code.GetRva());
    code.Emit({0x48, 0x89, 0xf9});
    call("CloseHandle");
    requireNonzero(parentError, 1);
    code.Rip({0x48, 0x8d, 0x0d}, parentVariable);
    code.Emit({0x31, 0xd2});
    call("SetEnvironmentVariableW");
    requireNonzero(parentError, 1);
    code.PatchBranch(noParent, code.GetRva());

    code.Emit({0x31, 0xc9});
    code.Rip({0x48, 0x8d, 0x15}, programPath);
    code.Emit({0x41, 0xb8});
    code.U32(PathCapacity);
    call("GetModuleFileNameW");
    requireNonzero(0, 0xc000000du);
    code.Emit({0x3d});
    code.U32(PathCapacity);
    const auto pathFits = code.Branch({0x0f, 0x82});
    fail(1, 0xc0000106u);
    code.PatchBranch(pathFits, code.GetRva());
    code.Emit({0x89, 0xc1});
    code.Rip({0x48, 0x8d, 0x35}, programPath);
    code.Rip({0x48, 0x8d, 0x3d}, modulePath);
    code.Emit({0xfc, 0x66, 0xf3, 0xa5, 0x49, 0x89, 0xfc});
    code.Rip({0x48, 0x8d, 0x1d}, modulePath);
    const auto findSeparator = code.GetRva();
    code.Emit({0x49, 0x39, 0xdc});
    const auto hasDirectory = code.Branch({0x0f, 0x85});
    fail(2, 0xc000000du);
    code.PatchBranch(hasDirectory, code.GetRva());
    code.Emit({0x49, 0x83, 0xec, 0x02, 0x66, 0x41, 0x83, 0x3c, 0x24, 0x5c});
    code.Rip({0x0f, 0x85}, findSeparator);
    code.Emit({0x49, 0x83, 0xc4, 0x02});
    code.Emit({0x66, 0x41, 0xc7, 0x04, 0x24, 0x00, 0x00});
    logMissingHandle(0xfffffff5u);
    logMissingHandle(0xfffffff4u);
    code.Rip({0x48, 0x8d, 0x0d}, modulePath);
    call("SetCurrentDirectoryW");
    requireNonzero(directoryError, 0xc000000du);
    rebindPreloadedRuntimeLog(msvcrtLibrary, msvcrtIobFunctionName, false);
    rebindPreloadedRuntimeLog(ucrtbaseLibrary, ucrtbaseIobFunctionName, true);
    if (dependencyDiagnostics) {
        code.Rip({0x4c, 0x8d, 0x05}, programPath);
        writeUtf8(programText);
    }

    code.Rip({0x48, 0x8d, 0x0d}, allocatorVariable);
    code.Emit({0x31, 0xd2, 0x45, 0x31, 0xc0});
    call("GetEnvironmentVariableW");
    code.Emit({0x85, 0xc0});
    const auto noAllocator = code.Branch({0x0f, 0x84});
    for (std::size_t index = 0; index < allocatorPaths.size(); ++index) {
        std::optional<std::size_t> tooLong;
        if (absolutePath) {
            code.Rip({0x48, 0x8d, 0x0d}, allocatorPaths[index]);
        } else {
            const auto units = CheckedRva(allocatorUnits[index] + 1);
            code.Emit({0x4c, 0x89, 0xe0, 0x48, 0x29, 0xd8, 0x48, 0x05});
            code.U32(units * 2);
            code.Emit({0x48, 0x3d});
            code.U32(PathCapacity * 2);
            tooLong = code.Branch({0x0f, 0x87});
            code.Emit({0x4c, 0x89, 0xe7});
            code.Rip({0x48, 0x8d, 0x35}, allocatorPaths[index]);
            code.Emit({0xb9});
            code.U32(units);
            code.Emit({0x66, 0xf3, 0xa5});
            code.Rip({0x48, 0x8d, 0x0d}, modulePath);
        }
        code.Emit({0x31, 0xd2, 0x41, 0xb8, 0, 0x11, 0, 0});
        call("LoadLibraryExW");
        if (tooLong) code.PatchBranch(*tooLong, code.GetRva());
    }
    code.PatchBranch(noAllocator, code.GetRva());

    for (std::size_t index = 0; index < libraries.size(); ++index) {
        if (index >= guestModules.size() && !imported[index] && !unscopedImports) continue;
        if (absolutePath && index >= guestModules.size()) {
            code.Rip({0x48, 0x8d, 0x0d}, libraryPaths[index]);
        } else {
            const auto units = CheckedRva(libraryUnits[index] + 1);
            code.Emit({0x4c, 0x89, 0xe0, 0x48, 0x29, 0xd8, 0x48, 0x05});
            code.U32(units * 2);
            code.Emit({0x48, 0x3d});
            code.U32(PathCapacity * 2);
            const auto fits = code.Branch({0x0f, 0x86});
            fail(1, 0xc0000106u);
            code.PatchBranch(fits, code.GetRva());
            code.Emit({0x4c, 0x89, 0xe7});
            code.Rip({0x48, 0x8d, 0x35}, libraryPaths[index]);
            code.Emit({0xb9});
            code.U32(units);
            code.Emit({0x66, 0xf3, 0xa5});
            code.Rip({0x48, 0x8d, 0x0d}, modulePath);
        }
        code.Emit({0x48, 0x89, 0xce, 0x49, 0x89, 0xf0});
        writeUtf8(resolvedPaths[index]);

        if (dependencyDiagnostics) {
            writeString(loading);
            writeString(resolvedPaths[index]);
        }

        std::vector<std::size_t> missingOptional;
        std::optional<std::size_t> probeFailed;
        if (index >= guestModules.size() && !imported[index]) {
            code.Emit({0x48, 0x89, 0xf1});
            call("GetFileAttributesW");
            code.Emit({0x83, 0xf8, 0xff});
            const auto exists = code.Branch({0x0f, 0x85});
            captureLastError();
            code.Emit({0x83, 0xf8, 2});
            missingOptional.push_back(code.Branch({0x0f, 0x84}));
            code.Emit({0x83, 0xf8, 3});
            missingOptional.push_back(code.Branch({0x0f, 0x84}));
            probeFailed = code.Branch({0xe9});
            code.PatchBranch(exists, code.GetRva());
        }

        code.Emit({0x48, 0x89, 0xf1, 0x31, 0xd2, 0x41, 0xb8, 0, 0x11, 0, 0});
        call("LoadLibraryExW");
        code.Emit({0x48, 0x85, 0xc0});
        const auto loadSucceeded = code.Branch({0x0f, 0x85});
        captureLastError();
        if (dependencyDiagnostics) {
            writeString(loadFailed, true);
        }
        if (probeFailed) code.PatchBranch(*probeFailed, code.GetRva());
        writeString(failedModule, true);
        writeString(resolvedPaths[index], true);
        writeString(newline, true);
        writeLastError();
        code.Rip({0x8b, 0x05}, lastError);
        code.Emit({0x83, 0xf8, 0x7e});
        const auto notModuleNotFound = code.Branch({0x0f, 0x85});
        writeString(moduleNotFoundHint, true);
        code.PatchBranch(notModuleNotFound, code.GetRva());
        if (dependencyDiagnostics) {
            code.Rip({0x48, 0x8d, 0x0d}, resolvedPaths[index]);
            code.Rip({0x48, 0x8d, 0x15}, programText);
            dependencyCalls.push_back(code.Branch({0xe8}));
        }
        raise(0xc0000135u);
        code.PatchBranch(loadSucceeded, code.GetRva());
        code.Rip({0x48, 0x89, 0x05}, CheckedRva(handles + index * 8));

        if (dependencyDiagnostics) {
            writeString(loaded);
        }
        if (!missingOptional.empty()) {
            const auto next = code.Branch({0xe9});
            for (const auto branch : missingOptional) code.PatchBranch(branch, code.GetRva());
            if (dependencyDiagnostics) writeString(skippedModule, true);
            code.PatchBranch(next, code.GetRva());
        }
    }

    if (traceImports) {
        code.Rip({0x48, 0x8b, 0x0d}, CheckedRva(handles + *traceProvider * 8));
        code.Rip({0x48, 0x8d, 0x15}, traceSymbol);
        call("GetProcAddress");
        requireNonzero(traceError, 0xc0000139u);
        code.Rip({0x48, 0x89, 0x05}, traceFunction);
    }
    const auto trace = [&](const PeImport& import, const std::uint32_t symbolName) {
        if (!traceImports || !import.Function || import.Addend != 0) return;
        code.Emit({0x48, 0x89, 0xc1});
        code.Rip({0x48, 0x8d, 0x15}, symbolName);
        code.Rip({0x4c, 0x8d, 0x05}, traceLibraries.at(import.Library));
        code.Rip({0xff, 0x15}, traceFunction);
    };

    std::vector<std::size_t> unresolvedBranches;
    bool anyUnresolvable = false;
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
        std::vector<std::size_t> resolved = {code.Branch({0x0f, 0x85})};
        code.Emit({0x48, 0x83, 0xc7, 4, 0xff, 0xcd});
        code.Rip({0x0f, 0x85}, search);
        if (nidNames[index]) {
            code.Rip({0x48, 0x8d, 0x35}, *nidNames[index]);
            code.Rip({0x48, 0x8d, 0x3d}, searchTables[index]);
            code.Emit({0xbd});
            code.U32(searchCounts[index]);
            const auto nidSearch = code.GetRva();
            code.Emit({0x8b, 0x0f, 0x48, 0x8b, 0x0c, 0xcb, 0x48, 0x89, 0xf2});
            call("GetProcAddress");
            code.Emit({0x48, 0x85, 0xc0});
            resolved.push_back(code.Branch({0x0f, 0x85}));
            code.Emit({0x48, 0x83, 0xc7, 4, 0xff, 0xcd});
            code.Rip({0x0f, 0x85}, nidSearch);
        }
        const auto patchResolved = [&] { for (const auto branch : resolved) code.PatchBranch(branch, code.GetRva()); };
        if (imports[index].Weak) {
            code.Emit({0x31, 0xc0});
            const auto writeWeak = code.GetRva();
            if (imports[index].Addend != 0) {
                code.Emit({0x48, 0xba});
                code.U64(imports[index].Addend);
                code.Emit({0x48, 0x01, 0xd0});
            }
            guestStartup.WriteImport(code, imports[index], handles);
            const auto weakDone = code.Branch({0xe9});
            patchResolved();
            code.Rip({0xe9}, writeWeak);
            code.PatchBranch(weakDone, code.GetRva());
            continue;
        }
        if (lazyBinding) {
            lazyUnresolvedImports.push_back(index);
            const auto skipGotWrite = code.Branch({0xe9});
            patchResolved();
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
        code.Rip({0xff, 0x05}, unresolvedCount);
        const auto nextImport = code.Branch({0xe9});
        patchResolved();
        if (imports[index].RelocationType == 16) code.Emit({0x48, 0x8b, 0x00});
        if (imports[index].RelocationType == 17) code.Emit({0x48, 0x8b, 0x40, 8});
        if (imports[index].Addend != 0) {
            code.Emit({0x48, 0xba});
            code.U64(imports[index].Addend);
            code.Emit({0x48, 0x01, 0xd0});
        }
        trace(imports[index], symbolNames[index]);
        guestStartup.WriteImport(code, imports[index], handles);
        code.PatchBranch(nextImport, code.GetRva());
        anyUnresolvable = true;
    }

    if (anyUnresolvable) {
        code.Rip({0x8b, 0x05}, unresolvedCount);
        code.Emit({0x85, 0xc0});
        unresolvedBranches.push_back(code.Branch({0x0f, 0x85}));
    }
    for (const auto message : deferredImports) writeString(message);
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
    code.Rip({0x83, 0x3d}, skipFirstArgument - 1);
    code.Emit({0});
    const auto retainProgramName = code.Branch({0x0f, 0x84});
    code.Emit({0x48, 0x83, 0xc3, 8});
    code.Rip({0x83, 0x2d}, argumentCount - 1);
    code.Emit({1});
    code.PatchBranch(retainProgramName, code.GetRva());
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
    code.Emit({0x48, 0xc7, 0x44, 0x24, 0x40, 0, 0, 0, 0, 0x48, 0xc7, 0x44, 0x24, 0x48, 0, 0, 0, 0, 0x48, 0x8d, 0x6c, 0x24, 0x40});
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
        for (std::size_t index = 0; index < resolvedPaths.size(); ++index) {
            if (index >= guestModules.size() && !imported[index] && !unscopedImports) continue;
            writeString(indent, true);
            writeString(resolvedPaths[index], true);
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
