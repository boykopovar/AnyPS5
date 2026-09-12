#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <elfpatcher/windows/WindowsDependencyStubBuilder.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <io/BufferUtils.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

namespace Fs = std::filesystem;
using namespace Elfpatcher::Windows;

void writeFile(const Fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot write diagnostic fixture");
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

void expectDiagnostic(const Fs::path& runner, const std::vector<std::string>& expected) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    if (!CreatePipe(&input, &output, &security, 0) || !SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("Cannot create diagnostic test pipe");
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output;
    startup.hStdError = output;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const auto filename = runner.string();
    if (!CreateProcessA(filename.c_str(), nullptr, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, runner.parent_path().string().c_str(), &startup, &process))
        throw std::runtime_error("Cannot start diagnostic test: " + std::to_string(GetLastError()));
    CloseHandle(output);
    if (WaitForSingleObject(process.hProcess, 20000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        throw std::runtime_error("Diagnostic test timed out");
    }
    DWORD status = 0;
    if (!GetExitCodeProcess(process.hProcess, &status))
        throw std::runtime_error("Cannot read diagnostic test exit code");
    std::string message;
    char buffer[4096];
    DWORD size = 0;
    while (ReadFile(input, buffer, sizeof(buffer), &size, nullptr) && size != 0)
        message.append(buffer, size);
    CloseHandle(input);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (status != 0xc0000135u)
        throw std::runtime_error("Wrong diagnostic exit code: " + std::to_string(status) + "\n" + message);
    for (const auto& text : expected) {
        if (message.find(text) == std::string::npos)
            throw std::runtime_error("Missing diagnostic field: " + text + "\nActual: " + message);
    }
}

}

int main() {
    try {
        std::vector<char> filename(32768);
        const auto size = GetModuleFileNameA(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
        if (size == 0 || size >= filename.size())
            throw std::runtime_error("Cannot locate diagnostic tests");
        const auto directory = Fs::path(filename.data()).parent_path() / "windows-diagnostic-fixtures";
        Fs::create_directories(directory);
        const auto root = directory / "diagnostic-root.prx";
        const auto middle = directory / "diagnostic-middle.dll";
        const auto leaf = directory / "diagnostic-leaf.dll";
        const auto runner = directory / "diagnostic-runner.exe";
        createRunner(runner, root);
        createImage(root, {{middle.filename().string(), "Middle"}}, {});
        createImage(middle, {{leaf.filename().string(), "Missing"}}, "Middle");
        createImage(leaf, {}, "Present");
        expectDiagnostic(runner, {"Importer: " + middle.string(), "Provider: " + leaf.string(), "Symbol: Missing", runner.string() + " -> " + root.string() + " -> " + middle.string() + " -> " + leaf.string()});
        createImage(middle, {{leaf.filename().string(), "#8"}}, "Middle");
        expectDiagnostic(runner, {"Symbol: #8"});
        createImage(middle, {{leaf.filename().string(), "#7"}}, "Middle");
        expectDiagnostic(runner, {"Dependency tables contain no missing imports"});
        createImage(middle, {}, "Middle", "diagnostic-leaf.Missing");
        expectDiagnostic(runner, {"Symbol: Missing", "Importer: " + middle.string()});
        createImage(middle, {}, "Middle", "diagnostic-leaf.#7");
        expectDiagnostic(runner, {"Dependency tables contain no missing imports"});
        createImage(leaf, {}, "Present", "diagnostic-middle.Middle");
        expectDiagnostic(runner, {"capacity exceeded"});
        createImage(middle, {{leaf.filename().string(), "Present"}}, "Middle");
        createImage(leaf, {{middle.filename().string(), "Middle"}}, "Present");
        expectDiagnostic(runner, {"Dependency tables contain no missing imports"});
        createImage(leaf, {{middle.filename().string(), "MissingInCycle"}}, "Present");
        expectDiagnostic(runner, {"Symbol: MissingInCycle", "Importer: " + leaf.string()});
        createImage(root, {{"diagnostic-absent.dll", "Missing"}}, {});
        expectDiagnostic(runner, {"dependency module not found", "Importer: " + root.string()});
        createImage(root, {{"KERNEL32.dll", "CreateRemoteThreadEx"}}, {});
        expectDiagnostic(runner, {"Dependency tables contain no missing imports"});
        createImage(root, {{"KERNEL32.dll", "RelinkerDiagnosticsMissingExport"}}, {});
        expectDiagnostic(runner, {"Symbol: RelinkerDiagnosticsMissingExport"});
        {
            std::fstream corrupt(root, std::ios::binary | std::ios::in | std::ios::out);
            const std::array<char, 4> invalidRva{static_cast<char>(0xf0), static_cast<char>(0xff), static_cast<char>(0xff), static_cast<char>(0xff)};
            corrupt.seekp(LoadRva + 0x300);
            if (!corrupt.write(invalidRva.data(), invalidRva.size()))
                throw std::runtime_error("Cannot corrupt diagnostic fixture");
        }
        expectDiagnostic(runner, {"invalid or unsupported PE dependency metadata"});
        writeFile(root, {0, 1, 2});
        expectDiagnostic(runner, {"Windows API failed"});
        std::cout << "Windows dependency machine-code tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
