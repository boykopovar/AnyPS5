#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


namespace {

bool cleaned = false;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Exec fixture: %s\n", message);
        std::fflush(stderr);
        std::_Exit(98);
    }
}

std::filesystem::path Root() {
#ifdef _WIN32
    std::wstring value(32768, L'\0');
    const auto length = GetEnvironmentVariableW(L"ANYPS5_LOADEXEC_TEST_ROOT", value.data(), static_cast<DWORD>(value.size()));
    Require(length > 0 && length < value.size(), "missing fixture directory");
    value.resize(length);
    return value;
#else
    const auto* value = std::getenv("ANYPS5_LOADEXEC_TEST_ROOT");
    Require(value != nullptr, "missing fixture directory");
    return std::filesystem::path(reinterpret_cast<const char8_t*>(value));
#endif
}

void RecordPid() {
    std::ofstream file(Root() / "pids.txt", std::ios::app);
#ifdef _WIN32
    file << GetCurrentProcessId() << '\n';
#else
    file << getpid() << '\n';
#endif
    file.close();
    Require(!file.fail(), "writing the process ID file");
}

void Record(const char* value) {
    std::ofstream file(Root() / "events.txt", std::ios::app);
    file << value << '\n';
    file.close();
    Require(!file.fail(), "writing the event file");
}

std::string Events() {
    std::ifstream file(Root() / "events.txt");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void Shutdown() { cleaned = true; Record("shutdown"); }
void APS5_VABI Finalize(void*) { Record("guest-exit"); }
void HostFinalize() { Record("host-exit"); }

void Register() {
    LibcRegisterShutdown_nid_postfix(Shutdown);
    Require(__cxa_atexit_nid_postfix(Finalize, nullptr, nullptr) == 0, "guest exit registration");
    Require(std::atexit(HostFinalize) == 0, "host exit registration");
}

void Arguments(const std::uintptr_t* block, const std::vector<std::string>& expected) {
    Require(block[0] == expected.size(), "entry argument count");
    const auto* argv = reinterpret_cast<const char* const*>(block + 1);
    Require(getargc_nid_postfix() == static_cast<int>(expected.size()), "getargc argument count");
    const auto* kernel = getargv_nid_postfix();
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Require(argv[index] && expected[index] == argv[index], "entry argument value");
        Require(kernel[index] && expected[index] == kernel[index], "getargv argument value");
    }
    Require(argv[expected.size()] == nullptr && kernel[expected.size()] == nullptr, "argument terminator");
}

std::string LoaderEnvironment() {
    const auto* value = std::getenv("LD_LIBRARY_PATH");
    return value != nullptr ? std::string("LD_LIBRARY_PATH=") + value : std::string();
}

std::vector<std::string> SecondEnvironment() {
    std::vector<std::string> entries{"EXEC_SECOND=stage2"};
    auto loader = LoaderEnvironment();
    if (!loader.empty()) entries.push_back(loader);
    const auto* root = std::getenv("ANYPS5_LOADEXEC_TEST_ROOT");
    Require(root != nullptr, "missing fixture directory for the environment");
    entries.emplace_back(std::string("ANYPS5_LOADEXEC_TEST_ROOT=") + root);
    return entries;
}

}

extern "C" void APS5_VABI ExecFirst_nid_no_patch(const std::uintptr_t* block) {
    (void)block;
    RecordPid();
    Record("first");
    Register();
    char* const present[] = {const_cast<char*>("/app0/picker/second.bin"), nullptr};
    char* const empty[] = {nullptr};
    char* const unpaired[] = {const_cast<char*>("EXEC_SECOND"), nullptr};
    const char* const absent = "/app0/picker/absent.bin";
    Require(execv_nid_postfix(nullptr, present) == -1 && *__error_nid_postfix() == 9, "null path rejected with EBADF");
    Require(execv_nid_postfix("/app0/picker/second.bin", nullptr) == -1 && *__error_nid_postfix() == 14,
            "null arguments rejected with EFAULT");
    Require(execv_nid_postfix(absent, empty) == -1 && *__error_nid_postfix() == 2,
            "an empty argument list is accepted and an absent path rejected with ENOENT");
    Require(execve_nid_postfix(absent, present, nullptr) == -1 && *__error_nid_postfix() == 2,
            "a null environment is accepted and an absent path rejected with ENOENT");
    Require(execve_nid_postfix(absent, present, unpaired) == -1 && *__error_nid_postfix() == 2,
            "an unpaired environment entry is copied and an absent path rejected with ENOENT");
    Require(execvp_nid_postfix("absent-bin", present) == -1 && *__error_nid_postfix() == 2, "PATH search ends with ENOENT");
    static std::string huge(300000, 'X');
    char* const oversized[] = {const_cast<char*>("/app0/picker/second.bin"), &huge[0], nullptr};
    Require(execv_nid_postfix("/app0/picker/second.bin", oversized) == -1 && *__error_nid_postfix() == 7,
            "oversized arguments rejected with E2BIG");
    static std::string largeVector(262201, 'K');
    static std::string largeEntry = "EXEC_LARGE_SECOND=" + largeVector;
    char* const largeEnvironment[] = {&largeEntry[0], nullptr};
    Require(execve_nid_postfix(absent, present, largeEnvironment) == -1 && *__error_nid_postfix() == 7,
            "an environment vector beyond the limit rejected with E2BIG");
    char* const beyond[] = {const_cast<char*>("/app0/picker/second.bin/child"), nullptr};
    Require(execv_nid_postfix("/app0/picker/second.bin/child", beyond) == -1 && *__error_nid_postfix() == 20,
            "a path beyond a regular file rejected with ENOTDIR");
#ifndef _WIN32
    char* const stripped[] = {const_cast<char*>("/app0/picker/noexec.bin"), nullptr};
    Require(execv_nid_postfix("/app0/picker/noexec.bin", stripped) == -1 && *__error_nid_postfix() == 13,
            "a regular file without execute permission rejected with EACCES");
#endif
#ifdef _WIN32
    const auto second = Root() / L"picker.second.bin.exe";
#else
    const auto second = Root() / "picker.second.bin.elf";
#endif
    auto intact = second;
    intact += ".intact";
    std::filesystem::rename(second, intact);
    {
        std::ofstream broken(second, std::ios::binary);
        broken << "no image";
        broken.close();
        Require(execv_nid_postfix("/app0/picker/second.bin", present) == -1 && *__error_nid_postfix() == 8,
                "a converted target without an executable image rejected with ENOEXEC");
    }
    std::filesystem::remove(second);
    std::filesystem::rename(intact, second);
    auto saved = second;
    saved += ".saved";
    std::filesystem::rename(second, saved);
    bool rejected = false;
    try {
        execv_nid_postfix("/app0/picker/second.bin", present);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    std::filesystem::rename(saved, second);
    Require(rejected && Events() == "first\n", "missing conversion rejected without replacing");
    std::filesystem::current_path(Root() / "app0" / "working");
    Record("launch");
    std::vector<std::string> values = SecondEnvironment();
    std::vector<char*> environment;
    for (auto& value : values) environment.push_back(&value[0]);
    environment.push_back(nullptr);
    char* const target[] = {const_cast<char*>("/app0/picker/second.bin"), const_cast<char*>("argument with spaces"),
                            const_cast<char*>("quote\"and\\slash\\ Кириллица 東京"), nullptr};
    try {
        execve_nid_postfix("/app0/picker/second.bin", target, environment.data());
    } catch (...) {
        Record("throw");
        std::fflush(nullptr);
        std::_Exit(96);
    }
    Require(false, "execve returned to the first executable");
}

extern "C" void APS5_VABI ExecSecond_nid_no_patch(const std::uintptr_t* block) {
    Arguments(block, {"/app0/picker/second.bin", "argument with spaces", "quote\"and\\slash\\ Кириллица 東京"});
#ifdef _WIN32
    Require(std::filesystem::equivalent(std::filesystem::current_path(), Root()), "secondary inherits the image directory");
#else
    Require(std::filesystem::equivalent(std::filesystem::current_path(), Root() / "app0" / "working"), "exec keeps the working directory");
#endif
    Require(Events() == "first\nlaunch\n", "exec replaced the image without running exit handlers");
    const std::vector<std::string> entries{SecondEnvironment()};
#ifdef _WIN32
    char value[64]{};
    Require(GetEnvironmentVariableA("EXEC_SECOND", value, 64) == 6 && value == std::string("stage2"),
            "execve environment entry reached the replacement");
    Require(GetEnvironmentVariableA("PATH", value, 64) != 0, "execve keeps the inherited host variables");
    Require(GetEnvironmentVariableA("ANYPS5_LOADEXEC_PARENT", value, 64) == 0, "parent wait variable was retained");
    Require(GetEnvironmentVariableA("ANYPS5_LOADEXEC_ARGV_SKIP", value, 64) == 0, "argument marker was retained");
#else
    extern char** environ;
    Require(environ[0] != nullptr, "replacement environment present");
    for (const auto& entry : entries) {
        bool found = false;
        for (std::size_t index = 0; environ[index] != nullptr; ++index)
            found = found || std::strcmp(environ[index], entry.c_str()) == 0;
        Require(found, "replacement environment entry value");
    }
    bool hostPath = false;
    for (std::size_t index = 0; environ[index] != nullptr; ++index)
        hostPath = hostPath || std::strncmp(environ[index], "PATH=", 5) == 0;
    Require(hostPath, "exec keeps the inherited host variables");
    Require(!cleaned, "secondary inherited old runtime state");
#endif
    RecordPid();
    Record("second");
    std::fflush(nullptr);
    std::_Exit(37);
}
