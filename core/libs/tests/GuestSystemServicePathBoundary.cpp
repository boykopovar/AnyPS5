#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <stdexcept>
#include <windows.h>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

[[noreturn]] void UnexpectedLaunch() {
    std::_Exit(86);
}

int Probe(const char* name) {
    LibcRegisterShutdown_nid_postfix(UnexpectedLaunch);
    const std::string path = std::string("/app0/..\\") + name;
    try {
        sceSystemServiceLoadExec(path.c_str(), nullptr);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(path) != std::string::npos ? 0 : 2;
    }
    return 1;
}

DWORD RunProbe(const std::filesystem::path& executable, const std::string& name) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + executable.wstring() + L"\" --probe ";
    command.append(name.begin(), name.end());
    Require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0);
    const DWORD waitResult = WaitForSingleObject(process.hProcess, 10000);
    if (waitResult != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
        WaitForSingleObject(process.hProcess, 5000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return ERROR_TIMEOUT;
    }
    DWORD code = 1;
    if (!GetExitCodeProcess(process.hProcess, &code)) code = ERROR_GEN_FAILURE;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code;
}

int Test() {
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    Require(length > 0 && length < executable.size());
    executable.resize(length);
    const std::filesystem::path own(executable);
    const std::string name = "anyps5-path-boundary-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    const auto fixture = own.parent_path().parent_path() / (name + own.extension().string());
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } cleanup{fixture};
    std::filesystem::copy_file(own, fixture);
    Require(RunProbe(own, name) == 0);
    return 0;
}

}

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--probe") == 0) return Probe(argv[2]);
    Require(argc == 1);
    return Test();
}
