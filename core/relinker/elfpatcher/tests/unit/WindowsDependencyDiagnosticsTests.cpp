#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Testing/Test.hpp>
#include <elfpatcher/windows/WindowsDependencyStubBuilder.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <elfpatcher/windows/WindowsImportBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace Fs = std::filesystem;
using namespace Elfpatcher::Windows;

constexpr DWORD DllNotFoundStatus = 0xc0000135u;
constexpr char NoMissingImports[] = "Dependency tables contain no missing imports";

Fs::path executableDirectory() {
    std::vector<char> filename(32768);
    const auto size = GetModuleFileNameA(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    Testing::Require(size != 0 && size < filename.size(), "Cannot locate diagnostic tests");
    return Fs::path(filename.data()).parent_path();
}

Fs::path createFixtureDirectory(const Fs::path& parent) {
    std::random_device random;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto directory = parent / ("windows-diagnostic-fixtures-" +
                                        std::to_string(GetCurrentProcessId()) + "-" + std::to_string(random()));
        if (Fs::create_directory(directory))
            return directory;
    }
    Testing::Fail("Cannot create a unique diagnostic fixture directory");
}

std::vector<std::string> readLibraries(const std::vector<std::string>& input) {
    Domain::SysVDynamicSection dynamic;
    for (const auto& name : input) {
        const auto offset = dynamic.DynStrData.size();
        dynamic.DynStrData.insert(dynamic.DynStrData.end(), name.begin(), name.end());
        dynamic.DynStrData.push_back(0);
        const auto position = dynamic.DynamicSegmentData.size();
        dynamic.DynamicSegmentData.resize(position + 16);
        Io::WriteU64(dynamic.DynamicSegmentData, position, 1);
        Io::WriteU64(dynamic.DynamicSegmentData, position + 8, offset);
    }
    return WindowsImportBuilder{}.ReadLibraries(dynamic);
}

void writeFile(const Fs::path& path, const std::vector<std::uint8_t>& bytes) {
    const auto fail = [&](const char* phase) {
        const auto error = errno;
        const auto nativeError = _doserrno;
        Testing::Fail("Cannot write diagnostic fixture: phase=" + std::string(phase) +
                      " path=" + path.string() + " errno=" + std::to_string(error) +
                      " native_error=" + std::to_string(nativeError) +
                      " bytes=" + std::to_string(bytes.size()));
    };
    errno = 0;
    _set_doserrno(0);
    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open())
        fail("open");
    if (!stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        fail("write");
    stream.close();
    if (!stream)
        fail("close");
}

void createImage(const Fs::path& path, const std::vector<std::pair<std::string, std::string>>& imports, const std::string& exported, const std::string& forwarded = {}) {
    PeSection section{".fixture", LoadRva, SectionRead | SectionExecute | 0x20u, std::vector<std::uint8_t>(4096)};
    auto& bytes = section.Data;
    const auto put = [&](const std::size_t offset, const std::uint64_t value, const std::size_t size = 4) {
        for (std::size_t index = 0; index < size; ++index)
            bytes.at(offset + index) = static_cast<std::uint8_t>(value >> (index * 8));
    };
    std::size_t cursor = 0x600;
    const auto string = [&](const std::string& text) {
        const auto start = cursor;
        for (const auto character : text)
            bytes.at(cursor++) = static_cast<std::uint8_t>(character);
        bytes.at(cursor++) = 0;
        return LoadRva + start;
    };
    std::array<PeDirectory, 16> directories{};
    if (!exported.empty()) {
        directories[0] = {LoadRva + 0x200, 0x100};
        put(0x210, 7);
        put(0x214, 1);
        put(0x218, 1);
        put(0x21c, LoadRva + 0x240);
        put(0x220, LoadRva + 0x248);
        put(0x224, LoadRva + 0x250);
        put(0x240, LoadRva + (forwarded.empty() ? 0x800 : 0x260));
        put(0x248, string(exported));
        for (std::size_t index = 0; index < forwarded.size(); ++index)
            bytes.at(0x260 + index) = static_cast<std::uint8_t>(forwarded[index]);
    }
    if (!imports.empty()) {
        directories[1] = {LoadRva + 0x300, CheckedRva((imports.size() + 1) * 20)};
        for (std::size_t index = 0; index < imports.size(); ++index) {
            const auto& [library, symbol] = imports[index];
            const auto thunk = 0x400 + index * 16;
            put(0x300 + index * 20, LoadRva + thunk);
            put(0x300 + index * 20 + 12, string(library));
            put(0x300 + index * 20 + 16, LoadRva + thunk);
            if (symbol.starts_with('#')) {
                put(thunk, (std::uint64_t{1} << 63) | std::stoull(symbol.substr(1)), 8);
            } else {
                cursor += 2;
                put(thunk, string(symbol) - 2, 8);
            }
        }
    }
    bytes[0x800] = 0xc3;
    auto file = WindowsPeWriter().Write({section}, LoadRva + 0x800, directories);
    Io::WriteU16(file, 0x96, 0x2022);
    Io::WriteU32(file, 0xa8, 0);
    writeFile(path, file);
}

void createRunner(const Fs::path& path, const Fs::path& root) {
    auto imports = WindowsImportBuilder().Build(LoadRva);
    PeSection data{".startup", AlignRva(LoadRva + imports.Section.Data.size()), SectionRead | SectionWrite | 0x40u, {}};
    const auto libraryPath = data.Rva;
    Io::AppendString(data.Data, root.string());
    const auto executablePath = CheckedRva(data.Rva + data.Data.size());
    Io::AppendString(data.Data, path.string());
    Io::AlignBuffer(data.Data, 4);
    const auto table = CheckedRva(data.Rva + data.Data.size());
    data.Data.resize(data.Data.size() + 32 * 12);
    const auto unwind = CheckedRva(data.Rva + data.Data.size());
    data.Data.insert(data.Data.end(), {1, 4, 1, 0, 4, 0x42, 0, 0});
    WindowsDependencyStubBuilder builder(data);
    const auto codeRva = AlignRva(data.Rva + data.Data.size());
    WindowsStubEmitter code(codeRva);
    code.Emit({0x48, 0x83, 0xec, 0x28});
    code.Rip({0x48, 0x8d, 0x0d}, libraryPath);
    code.Rip({0x48, 0x8d, 0x15}, executablePath);
    const auto call = code.Branch({0xe8});
    code.Emit({0x48, 0x83, 0xc4, 0x28, 0xc3});
    const auto wrapperEnd = code.GetRva();
    const auto diagnostic = builder.Build(code, imports);
    code.PatchBranch(call, diagnostic.EntryRva);
    Io::WriteU32(data.Data, table - data.Rva, codeRva);
    Io::WriteU32(data.Data, table - data.Rva + 4, wrapperEnd);
    Io::WriteU32(data.Data, table - data.Rva + 8, unwind);
    for (std::size_t index = 0; index < diagnostic.Functions.size(); ++index) {
        for (std::size_t field = 0; field < 3; ++field)
            Io::WriteU32(data.Data, table - data.Rva + (index + 1) * 12 + field * 4, diagnostic.Functions[index][field]);
    }
    std::array<PeDirectory, 16> directories{};
    directories[1] = imports.Directory;
    directories[3] = {table, CheckedRva((diagnostic.Functions.size() + 1) * 12)};
    directories[12] = imports.AddressTable;
    PeSection executable{".entry", codeRva, SectionRead | SectionExecute | 0x20u, code.TakeBytes()};
    writeFile(path, WindowsPeWriter().Write({imports.Section, data, executable}, codeRva, directories));
}

struct DiagnosticRun {
    DWORD Status = 0;
    std::string Output;
};

DiagnosticRun runDiagnostic(const Fs::path& runner) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    Testing::Require(CreatePipe(&input, &output, &security, 0) && SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0),
                     "Cannot create diagnostic test pipe");
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output;
    startup.hStdError = output;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const auto filename = runner.string();
    const auto started = CreateProcessA(filename.c_str(), nullptr, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                        runner.parent_path().string().c_str(), &startup, &process);
    const auto startError = GetLastError();
    CloseHandle(output);
    if (!started) {
        CloseHandle(input);
        Testing::Fail("Cannot start diagnostic test: " + std::to_string(startError));
    }
    DiagnosticRun run;
    const auto finished = WaitForSingleObject(process.hProcess, 20000) == WAIT_OBJECT_0;
    if (!finished)
        TerminateProcess(process.hProcess, 1);
    const auto exitCodeRead = finished && GetExitCodeProcess(process.hProcess, &run.Status);
    char buffer[4096];
    DWORD size = 0;
    while (ReadFile(input, buffer, sizeof(buffer), &size, nullptr) && size != 0)
        run.Output.append(buffer, size);
    CloseHandle(input);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Testing::Require(finished, "Diagnostic test timed out");
    Testing::Require(exitCodeRead, "Cannot read diagnostic test exit code");
    return run;
}

void expectDiagnostic(const Fs::path& runner, const std::vector<std::string>& expected) {
    const auto run = runDiagnostic(runner);
    Testing::RequireEqual(run.Status, DllNotFoundStatus, "Wrong diagnostic exit code\n" + run.Output);
    for (const auto& text : expected)
        Testing::Require(run.Output.find(text) != std::string::npos, "Missing diagnostic field: " + text + "\nActual: " + run.Output);
}

struct DiagnosticFixture {
    DiagnosticFixture()
        : Directory(createFixtureDirectory(executableDirectory())),
          Root(Directory / "diagnostic-root.prx"),
          Middle(Directory / "diagnostic-middle.dll"),
          Leaf(Directory / "diagnostic-leaf.dll"),
          Runner(Directory / "diagnostic-runner.exe") {
        createRunner(Runner, Root);
    }

    ~DiagnosticFixture() {
        std::error_code ignored;
        Fs::remove_all(Directory, ignored);
    }

    DiagnosticFixture(const DiagnosticFixture&) = delete;
    DiagnosticFixture& operator=(const DiagnosticFixture&) = delete;

    void CreateChain(const std::string& middleImport, const std::string& leafExport = "Present") const {
        createImage(Root, {{MiddleName(), "Middle"}}, {});
        createImage(Middle, {{LeafName(), middleImport}}, "Middle");
        createImage(Leaf, {}, leafExport);
    }

    std::string MiddleName() const {
        return Middle.filename().string();
    }

    std::string LeafName() const {
        return Leaf.filename().string();
    }

    Fs::path Directory;
    Fs::path Root;
    Fs::path Middle;
    Fs::path Leaf;
    Fs::path Runner;
};

const Testing::Case emptyLibraries{"WindowsImportBuilder_NoNeededLibraries_ReadsNone", [] {
    Testing::Require(readLibraries({}).empty(), "Incorrect Windows runtime dependency visibility for no libraries");
}};

const Testing::Case kernelOnly{"WindowsImportBuilder_KernelOnly_ReadsKernel", [] {
    const auto libraries = readLibraries({"libkernel.prx"});

    Testing::Require(libraries == std::vector<std::string>{"libkernel.prx"}, "Incorrect Windows runtime dependency visibility for libkernel");
}};

const Testing::Case libcInternalAddsLibc{"WindowsImportBuilder_LibcInternalWithoutLibc_AppendsLibc", [] {
    const auto libraries = readLibraries({"libSceLibcInternal.prx", "libkernel.prx"});

    Testing::Require(libraries == std::vector<std::string>{"libSceLibcInternal.prx", "libkernel.prx", "libc.prx"},
                     "Incorrect Windows runtime dependency visibility for libSceLibcInternal without libc");
}};

const Testing::Case libcNotDuplicated{"WindowsImportBuilder_LibcAlreadyNeeded_IsNotDuplicated", [] {
    const auto libraries = readLibraries({"libc.prx", "libSceLibcInternal.prx"});

    Testing::Require(libraries == std::vector<std::string>{"libc.prx", "libSceLibcInternal.prx"},
                     "Incorrect Windows runtime dependency visibility for libc with libSceLibcInternal");
}};

const Testing::Case missingNamedImport{"WindowsDependencyDiagnostics_MissingNamedImport_ReportsImporterProviderSymbolAndChain", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("Missing");

    expectDiagnostic(fixture.Runner, {"Importer: " + fixture.Middle.string(), "Provider: " + fixture.Leaf.string(), "Symbol: Missing",
                                      fixture.Runner.string() + " -> " + fixture.Root.string() + " -> " + fixture.Middle.string() + " -> " + fixture.Leaf.string()});
}};

const Testing::Case missingOrdinalImport{"WindowsDependencyDiagnostics_MissingOrdinalImport_ReportsOrdinal", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("#8");

    expectDiagnostic(fixture.Runner, {"Symbol: #8"});
}};

const Testing::Case presentOrdinalImport{"WindowsDependencyDiagnostics_PresentOrdinalImport_ReportsNoMissingImports", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("#7");

    expectDiagnostic(fixture.Runner, {NoMissingImports});
}};

const Testing::Case missingForwardedExport{"WindowsDependencyDiagnostics_ForwarderToMissingExport_ReportsSymbolAndImporter", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("Present");
    createImage(fixture.Middle, {}, "Middle", "diagnostic-leaf.Missing");

    expectDiagnostic(fixture.Runner, {"Symbol: Missing", "Importer: " + fixture.Middle.string()});
}};

const Testing::Case presentForwardedOrdinal{"WindowsDependencyDiagnostics_ForwarderToPresentOrdinal_ReportsNoMissingImports", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("Present");
    createImage(fixture.Middle, {}, "Middle", "diagnostic-leaf.#7");

    expectDiagnostic(fixture.Runner, {NoMissingImports});
}};

const Testing::Case forwarderCycle{"WindowsDependencyDiagnostics_ForwarderCycle_ReportsCapacityExceeded", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("Present");
    createImage(fixture.Middle, {}, "Middle", "diagnostic-leaf.#7");
    createImage(fixture.Leaf, {}, "Present", "diagnostic-middle.Middle");

    expectDiagnostic(fixture.Runner, {"capacity exceeded"});
}};

const Testing::Case satisfiedImportCycle{"WindowsDependencyDiagnostics_SatisfiedImportCycle_ReportsNoMissingImports", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("Present");
    createImage(fixture.Leaf, {{fixture.MiddleName(), "Middle"}}, "Present");

    expectDiagnostic(fixture.Runner, {NoMissingImports});
}};

const Testing::Case missingImportInCycle{"WindowsDependencyDiagnostics_MissingImportInCycle_ReportsSymbolAndImporter", [] {
    const DiagnosticFixture fixture;
    fixture.CreateChain("Present");
    createImage(fixture.Leaf, {{fixture.MiddleName(), "MissingInCycle"}}, "Present");

    expectDiagnostic(fixture.Runner, {"Symbol: MissingInCycle", "Importer: " + fixture.Leaf.string()});
}};

const Testing::Case absentModule{"WindowsDependencyDiagnostics_AbsentDependencyModule_ReportsModuleNotFound", [] {
    const DiagnosticFixture fixture;
    createImage(fixture.Root, {{"diagnostic-absent.dll", "Missing"}}, {});

    expectDiagnostic(fixture.Runner, {"dependency module not found", "Importer: " + fixture.Root.string()});
}};

const Testing::Case presentSystemExport{"WindowsDependencyDiagnostics_PresentSystemExport_ReportsNoMissingImports", [] {
    const DiagnosticFixture fixture;
    createImage(fixture.Root, {{"KERNEL32.dll", "CreateRemoteThreadEx"}}, {});

    expectDiagnostic(fixture.Runner, {NoMissingImports});
}};

const Testing::Case missingSystemExport{"WindowsDependencyDiagnostics_MissingSystemExport_ReportsSymbol", [] {
    const DiagnosticFixture fixture;
    createImage(fixture.Root, {{"KERNEL32.dll", "RelinkerDiagnosticsMissingExport"}}, {});

    expectDiagnostic(fixture.Runner, {"Symbol: RelinkerDiagnosticsMissingExport"});
}};

const Testing::Case corruptImportTable{"WindowsDependencyDiagnostics_InvalidImportRva_ReportsInvalidMetadata", [] {
    const DiagnosticFixture fixture;
    createImage(fixture.Root, {{"KERNEL32.dll", "RelinkerDiagnosticsMissingExport"}}, {});
    {
        std::fstream corrupt(fixture.Root, std::ios::binary | std::ios::in | std::ios::out);
        const std::array<char, 4> invalidRva{static_cast<char>(0xf0), static_cast<char>(0xff), static_cast<char>(0xff), static_cast<char>(0xff)};
        corrupt.seekp(LoadRva + 0x300);
        Testing::Require(static_cast<bool>(corrupt.write(invalidRva.data(), invalidRva.size())), "Cannot corrupt diagnostic fixture");
    }

    expectDiagnostic(fixture.Runner, {"invalid or unsupported PE dependency metadata"});
}};

const Testing::Case unreadableRoot{"WindowsDependencyDiagnostics_UnloadableRootImage_ReportsWindowsApiFailure", [] {
    const DiagnosticFixture fixture;
    writeFile(fixture.Root, {0, 1, 2});

    expectDiagnostic(fixture.Runner, {"Windows API failed"});
}};

} // namespace
