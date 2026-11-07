#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/General.hpp"
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
        std::fprintf(stderr, "LoadExec fixture: %s\n", message);
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

const char* FirstArgument() {
    return std::getenv("ANYPS5_TEST_QUOTED_ARGV0") ? "first\"and\\slash\\ Кириллица 東京" : "";
}

void Reject(const char* path, const char* const* argv) {
    bool rejected = false;
    try { sceSystemServiceLoadExec(path, argv); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected && !cleaned, "rejected target ran shutdown");
}

}

extern "C" void APS5_VABI LoadExecFirst_nid_no_patch(const std::uintptr_t* block) {
    if (!Events().empty()) {
        Arguments(block, {"back", "argument with spaces"});
        Require(std::filesystem::equivalent(std::filesystem::current_path(), Root()), "return working directory");
        RecordPid();
        Record("returned");
        LibcTerminate_nid_no_patch(37);
    }
    Require(std::filesystem::equivalent(std::filesystem::current_path(), Root()), "initial working directory");
    RecordPid();
    Record("first");
    Register();
    const char* const argv[] = {FirstArgument(), "argument with spaces", "quote\"and\\slash\\", "Кириллица 東京", nullptr};
    Reject("/app0/absent.bin", argv);
    Reject("/other/second.bin", argv);
    Reject("/app0/../second.bin", argv);
#ifdef _WIN32
    const auto second = Root() / L"picker.second.bin.exe";
#else
    const auto second = Root() / "picker.second.bin.elf";
#endif
    auto saved = second;
    saved += ".saved";
    std::filesystem::rename(second, saved);
    Reject("/app0/picker/second.bin", argv);
    std::filesystem::rename(saved, second);
    std::filesystem::current_path(Root() / "app0" / "working");
    sceSystemServiceLoadExec("/app0/picker/second.bin", argv);
    Require(false, "LoadExec returned to the first executable");
}

extern "C" void APS5_VABI LoadExecSecond_nid_no_patch(const std::uintptr_t* block) {
    Arguments(block, {FirstArgument(), "argument with spaces", "quote\"and\\slash\\", "Кириллица 東京"});
    Require(std::filesystem::equivalent(std::filesystem::current_path(), Root()), "secondary working directory");
    std::ifstream resource(ResolvePath_nid_no_patch("/app0/resource.txt"));
    std::string text;
    std::getline(resource, text);
    Require(text == "resource survives", "secondary resource access");
    Require(Events() == "first\nshutdown\n", "old executable did not end after its shutdown alone");
#ifdef _WIN32
    wchar_t value[32]{};
    Require(GetEnvironmentVariableW(L"ANYPS5_LOADEXEC_PARENT", value, 32) == 0, "parent wait variable was retained");
    Require(GetEnvironmentVariableW(L"ANYPS5_LOADEXEC_ARGV_SKIP", value, 32) == 0, "argument marker was retained");
#endif
    Require(!cleaned, "secondary inherited old runtime state");
    RecordPid();
    Record("second");
    Register();
    const char* const argv[] = {"back", "argument with spaces", nullptr};
    sceSystemServiceLoadExec("/app0/eboot.bin", argv);
    Require(false, "LoadExec returned to the second executable");
}
