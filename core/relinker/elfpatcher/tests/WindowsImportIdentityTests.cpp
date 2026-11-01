#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsImportBuilder.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Fs = std::filesystem;
using namespace Elfpatcher::Windows;

constexpr std::uint32_t GotRva = LoadRva + 0x1000;
constexpr std::array<const char*, 3> LocaleObjects{"-mLzBSk-VGs", "a54t8+k7KpY", "KfcTPbeaOqg"};

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Fs::path createFixtureDirectory(const Fs::path& parent) {
    std::random_device random;
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto name = Fs::path("windows-import-identity-");
        name += std::to_string(GetCurrentProcessId()) + "-" + std::to_string(random());
        const auto directory = parent / name;
        if (Fs::create_directory(directory)) return directory;
    }
    throw std::runtime_error("Cannot create a unique import identity fixture directory");
}

void appendDisplacement(std::vector<std::uint8_t>& code, const std::int32_t displacement) {
    for (int i = 0; i < 4; ++i) code.push_back(static_cast<std::uint8_t>((static_cast<std::uint32_t>(displacement) >> (8 * i)) & 0xffu));
}

// For each pair of GOT slots: mov rax,[a]; cmp rax,[b]; jne fail. Then xor eax,eax; ret. fail: mov eax,1; ret.
std::vector<std::uint8_t> compareCode(const std::vector<std::pair<int, int>>& pairs) {
    std::vector<std::uint8_t> code;
    const auto failOffset = static_cast<std::int32_t>(pairs.size() * 16 + 3);
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const auto base = static_cast<std::uint32_t>(i * 16);
        code.insert(code.end(), {0x48, 0x8b, 0x05});
        appendDisplacement(code, static_cast<std::int32_t>(GotRva + 8u * static_cast<std::uint32_t>(pairs[i].first)) - static_cast<std::int32_t>(LoadRva + base + 7));
        code.insert(code.end(), {0x48, 0x3b, 0x05});
        appendDisplacement(code, static_cast<std::int32_t>(GotRva + 8u * static_cast<std::uint32_t>(pairs[i].second)) - static_cast<std::int32_t>(LoadRva + base + 14));
        code.push_back(0x75);
        code.push_back(static_cast<std::uint8_t>(failOffset - static_cast<std::int32_t>(base + 16)));
    }
    code.insert(code.end(), {0x31, 0xc0, 0xc3});
    code.insert(code.end(), {0xb8, 0x01, 0x00, 0x00, 0x00, 0xc3});
    return code;
}

std::uint32_t slot(const int index) {
    return GotRva + 8u * static_cast<std::uint32_t>(index);
}

void generateProbe(const Fs::path& file, const std::string& libraries, const std::vector<PeImport>& peImports, const std::vector<std::pair<int, int>>& pairs) {
    const std::vector<std::string> loaded{"libSceAmpr.prx", "libkernel.prx", "libc.prx"};
    PeSection entry{".guest", LoadRva, SectionRead | SectionExecute | 0x20u, compareCode(pairs)};
    PeSection got{".got", GotRva, SectionRead | SectionWrite | 0x40u, std::vector<std::uint8_t>(8 * 8)};
    const auto imports = WindowsImportBuilder().Build(LoadRva + 0x2000);
    const auto startup = WindowsEntryStubBuilder().Build(LoadRva + 0x3000, entry.Rva, imports, loaded, peImports, libraries, false, false);
    std::array<PeDirectory, 16> directories{};
    directories[1] = imports.Directory;
    directories[3] = startup.ExceptionDirectory;
    directories[12] = imports.AddressTable;
    const auto image = WindowsPeWriter().Write({entry, got, imports.Section, startup.Data, startup.Code}, startup.Code.Rva, directories, false, ImageBase, true, false);
    std::ofstream stream(file, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    require(static_cast<bool>(stream), "Cannot write " + file.string());
}

DWORD runProbe(const Fs::path& probe, std::string& output) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input = nullptr;
    HANDLE written = nullptr;
    require(CreatePipe(&input, &written, &security, 0) && SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0), "Cannot create the probe's pipe");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = written;
    startup.hStdError = written;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const auto filename = probe.wstring();
    require(CreateProcessW(filename.c_str(), nullptr, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, probe.parent_path().wstring().c_str(), &startup, &process) != 0,
        "Cannot start " + probe.filename().string() + ": " + std::to_string(GetLastError()));
    CloseHandle(written);
    if (WaitForSingleObject(process.hProcess, 120000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        throw std::runtime_error(probe.filename().string() + " timed out");
    }
    DWORD status = 0;
    require(GetExitCodeProcess(process.hProcess, &status) != 0, "Cannot read the probe's exit code");
    char buffer[4096];
    DWORD size = 0;
    while (ReadFile(input, buffer, sizeof(buffer), &size, nullptr) && size != 0) output.append(buffer, size);
    CloseHandle(input);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return status;
}

// The generated probes load libc.prx next to the other libraries; its compiler runtime DLLs sit next to this test.
void copyRuntime(const Fs::path& directory) {
    wchar_t self[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    const auto own = Fs::path(self).parent_path();
    for (const char* name : {"libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"}) {
        std::error_code error;
        if (Fs::exists(own / name, error)) Fs::copy_file(own / name, directory / name, Fs::copy_options::overwrite_existing, error);
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: windows_import_identity_tests <patched libraries directory>\n";
        return 2;
    }
    try {
        const auto libraries = Fs::absolute(argv[1]).string();
        const auto directory = createFixtureDirectory(Fs::temp_directory_path());
        copyRuntime(directory);
        std::vector<PeImport> same;
        for (int i = 0; i < 3; ++i) {
            same.push_back({.Name = LocaleObjects[i], .TargetRva = slot(2 * i), .Addend = 0, .Library = "libSceAmpr.prx"});
            same.push_back({.Name = LocaleObjects[i], .TargetRva = slot(2 * i + 1), .Addend = 0, .Library = "libc.prx"});
        }
        generateProbe(directory / "same.exe", libraries, same, {{0, 1}, {2, 3}, {4, 5}});
        const std::vector<PeImport> different{
            {.Name = LocaleObjects[0], .TargetRva = slot(0), .Addend = 0, .Library = "libSceAmpr.prx"},
            {.Name = "k+AXqu2-eBc", .TargetRva = slot(6), .Addend = 0, .Library = "libkernel.prx"},
        };
        generateProbe(directory / "different.exe", libraries, different, {{0, 6}});
        std::string output;
        const auto sameStatus = runProbe(directory / "same.exe", output);
        require(sameStatus == 0, "A libSceAmpr.prx-qualified import of the locale objects did not resolve to libc's object (exit " + std::to_string(sameStatus) + ")\n" + output);
        output.clear();
        const auto differentStatus = runProbe(directory / "different.exe", output);
        require(differentStatus == 1, "The probe did not tell two different addresses apart (exit " + std::to_string(differentStatus) + ")\n" + output);
        std::error_code error;
        Fs::remove_all(directory, error);
        std::cout << "Windows import identity tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
