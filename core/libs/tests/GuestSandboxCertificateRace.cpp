#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <windows.h>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Sandbox certificate race check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr char CertificateBegin[] = "-----BEGIN CERTIFICATE-----";

static int Child(const char* start) {
    const HANDLE event = ::OpenEventA(SYNCHRONIZE, FALSE, start);
    Require(event != nullptr);
    Require(::WaitForSingleObject(event, 20000) == WAIT_OBJECT_0);
    ::CloseHandle(event);
    const std::string path = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/cert/CA_LIST.cer";
    const int descriptor = sceKernelOpen(path.c_str(), 0, 0);
    Require(descriptor >= 0);
    std::string text;
    char buffer[4096];
    for (std::int64_t count; (count = sceKernelRead(descriptor, buffer, sizeof(buffer))) != 0;) {
        Require(count > 0);
        text.append(buffer, static_cast<std::size_t>(count));
    }
    Require(sceKernelClose(descriptor) == 0);
    Require(text.find(CertificateBegin) != std::string::npos);
    return 0;
}

static void RemoveTree(const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error)) return;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
    std::filesystem::remove_all(directory);
}

static void Round(const std::filesystem::path& directory, int round, int processes) {
    RemoveTree(directory);
    std::filesystem::create_directories(directory);
    const std::string start = "Local\\AnyPS5SandboxCertificateRace" + std::to_string(::GetCurrentProcessId()) + "_" + std::to_string(round);
    const HANDLE event = ::CreateEventA(nullptr, TRUE, FALSE, start.c_str());
    Require(event != nullptr);
    wchar_t executable[32768];
    Require(::GetModuleFileNameW(nullptr, executable, 32768) != 0);
    const std::wstring command = L"\"" + std::wstring(executable) + L"\" child " + std::wstring(start.begin(), start.end());
    std::vector<HANDLE> children;
    for (int index = 0; index < processes; ++index) {
        std::wstring line = command;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        Require(::CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
            directory.wstring().c_str(), &startup, &process) != 0);
        ::CloseHandle(process.hThread);
        children.push_back(process.hProcess);
    }
    Require(::SetEvent(event) != 0);
    if (::WaitForMultipleObjects(static_cast<DWORD>(children.size()), children.data(), TRUE, 20000) != WAIT_OBJECT_0) {
        for (const HANDLE child : children) ::TerminateProcess(child, 99);
        std::fprintf(stderr, "Children in %s did not finish\n", directory.string().c_str());
        std::abort();
    }
    for (const HANDLE child : children) {
        DWORD result = 99;
        Require(::GetExitCodeProcess(child, &result) != 0);
        if (result != 0) std::fprintf(stderr, "Child in %s exited with 0x%08lx\n", directory.string().c_str(), result);
        Require(result == 0);
        ::CloseHandle(child);
    }
    ::CloseHandle(event);
    const auto cached = directory / "cert_cache" / "CA_LIST.cer";
    Require(std::filesystem::is_regular_file(cached));
    Require((std::filesystem::status(cached).permissions() & std::filesystem::perms::owner_write) == std::filesystem::perms::none);
    std::ifstream file(cached, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Require(text.rfind(CertificateBegin, 0) == 0);
    for (const auto& entry : std::filesystem::directory_iterator(directory / "cert_cache"))
        Require(entry.path().filename() == "CA_LIST.cer");
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "child") return Child(argv[2]);
    Require(argc == 1);
    const auto root = std::filesystem::current_path() / "rounds";
    for (int round = 0; round < 10; ++round) Round(root / std::to_string(round), round, 8);
    RemoveTree(root);
    return 0;
}
