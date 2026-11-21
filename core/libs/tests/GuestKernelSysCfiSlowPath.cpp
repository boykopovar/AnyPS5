#include "prx/libkernel_sys/include/AdditionalExports.hpp"

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uintptr_t PageShift = 12;

void Require(bool value, const std::string& message) {
    if (value) return;
    std::fprintf(stdout, "CFI slow path: %s\n", message.c_str());
    std::fflush(stdout);
    std::abort();
}

template<typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template<typename TAction>
std::string Captured(TAction action) {
    const auto path = (std::filesystem::temp_directory_path() / ("anyps5-cfi-slowpath-" + std::to_string(getpid()) + ".txt")).string();
    std::fflush(stderr);
    const int saved = dup(fileno(stderr));
    std::FILE* file = std::fopen(path.c_str(), "wb");
    Require(file != nullptr, "temporary file");
    dup2(fileno(file), fileno(stderr));
    action();
    std::fflush(stderr);
    dup2(saved, fileno(stderr));
    close(saved);
    std::fclose(file);
    std::ifstream input(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    std::remove(path.c_str());
    return text;
}

struct Type {
    std::uint16_t kind;
    std::uint16_t info;
    char name[32];
};

struct Diag {
    Type type{0xffff, 0, "void (int)"};
    Ubsan::CfiCheckFailData data{};
    Diag() {
        data.kind = 4;
        data.location = {"title.cpp", 12, 34};
        data.type = reinterpret_cast<const Ubsan::TypeDescriptor*>(&type);
    }
};

struct Fixture {
    std::string label;
    void* handle = nullptr;
    std::uint64_t* calls = nullptr;
    std::uint64_t* id = nullptr;
    void** target = nullptr;
    void** diag = nullptr;
    unsigned char* data = nullptr;
    std::uintptr_t check = 0;
    std::uintptr_t base = 0;
};

template<typename TValue>
TValue* Symbol(void* handle, const char* name) {
    auto* symbol = static_cast<TValue*>(dlsym(handle, name));
    Require(symbol != nullptr, std::string("fixture symbol ") + name);
    return symbol;
}

Fixture Open(const char* path, const char* label) {
    Fixture fixture;
    fixture.label = label;
    fixture.handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    const char* error = fixture.handle == nullptr ? dlerror() : nullptr;
    Require(fixture.handle != nullptr, std::string("the ") + label + " fixture loads: " + (error != nullptr ? error : ""));
    fixture.calls = Symbol<std::uint64_t>(fixture.handle, "CfiFixtureCalls_nid_no_patch");
    fixture.id = Symbol<std::uint64_t>(fixture.handle, "CfiFixtureId_nid_no_patch");
    fixture.target = Symbol<void*>(fixture.handle, "CfiFixtureTarget_nid_no_patch");
    fixture.diag = Symbol<void*>(fixture.handle, "CfiFixtureDiag_nid_no_patch");
    fixture.data = Symbol<unsigned char>(fixture.handle, "CfiFixtureData_nid_no_patch");
    auto* checkAddress = reinterpret_cast<void* (*)()>(dlsym(fixture.handle, "CfiFixtureCheckAddress_nid_no_patch"));
    Require(checkAddress != nullptr, "fixture symbol CfiFixtureCheckAddress_nid_no_patch");
    fixture.check = reinterpret_cast<std::uintptr_t>(checkAddress());
    Dl_info info{};
    Require(dladdr(fixture.data, &info) != 0 && info.dli_fbase != nullptr, "the fixture's load address");
    fixture.base = reinterpret_cast<std::uintptr_t>(info.dli_fbase);
    return fixture;
}

void TestCheckedModule(const Fixture& fixture) {
    const auto& label = fixture.label;
    Require((reinterpret_cast<std::uintptr_t>(fixture.data) >> PageShift) > (fixture.check >> PageShift), label + ": the data lies above __cfi_check");

    const auto calls = *fixture.calls;
    __cfi_slowpath_nid_postfix(0x1122334455667788ull, fixture.data);
    Require(*fixture.calls == calls + 1, label + ": a target in a module with __cfi_check calls it");
    Require(*fixture.id == 0x1122334455667788ull && *fixture.target == fixture.data && *fixture.diag == nullptr,
        label + ": __cfi_check gets the call site type id, the target and no diagnostic data");

    int marker = 0;
    __cfi_slowpath_diag_nid_postfix(7, fixture.data + 1, &marker);
    Require(*fixture.calls == calls + 2 && *fixture.id == 7 && *fixture.target == fixture.data + 1 && *fixture.diag == &marker,
        label + ": __cfi_slowpath_diag passes its diagnostic data to __cfi_check");

    __cfi_slowpath_nid_postfix(9, reinterpret_cast<void*>(fixture.check));
    Require(*fixture.calls == calls + 3 && *fixture.id == 9, label + ": the page of __cfi_check itself is checked");

    Require((fixture.base >> PageShift) < (fixture.check >> PageShift), label + ": the image starts below the page of __cfi_check");
    auto* below = reinterpret_cast<void*>(fixture.base);
    Require(Throws([&] { __cfi_slowpath_nid_postfix(1, below); }), label + ": a target below __cfi_check's page is an invalid region");
    Require(Throws([&] { __cfi_slowpath_diag_nid_postfix(1, below, nullptr); }), label + ": __cfi_slowpath_diag without data traps there too");
    Diag diag;
    const auto report = Captured([&] { __cfi_slowpath_diag_nid_postfix(1, below, &diag.data); });
    Require(report.find("title.cpp:12:34: runtime error: control flow integrity check for type 'void (int)' failed during indirect function call") != std::string::npos,
        label + ": __cfi_slowpath_diag reports an invalid region through __ubsan_handle_cfi_check_fail and returns: " + report);
    Require(*fixture.calls == calls + 3, label + ": __cfi_check is not called for an invalid region");
}

void TestUnchecked(const Fixture& fixture) {
    const auto calls = *fixture.calls;
    __cfi_slowpath_nid_postfix(3, reinterpret_cast<void*>(&Require));
    __cfi_slowpath_nid_postfix(3, reinterpret_cast<void*>(&std::strlen));
    __cfi_slowpath_diag_nid_postfix(3, reinterpret_cast<void*>(&std::strlen), nullptr);
    Require(*fixture.calls == calls, "targets in modules without __cfi_check (the program, libc) are unchecked");
}

void TestInvalid() {
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* anonymous = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(anonymous != MAP_FAILED, "an anonymous mapping");
    Require(Throws([&] { __cfi_slowpath_nid_postfix(5, anonymous); }), "a target outside every module is an invalid region");
    Require(Throws([&] { __cfi_slowpath_diag_nid_postfix(5, anonymous, nullptr); }), "__cfi_slowpath_diag without data traps outside every module");
    Diag diag;
    const auto report = Captured([&] { __cfi_slowpath_diag_nid_postfix(5, anonymous, &diag.data); });
    Require(report.find("control flow integrity check for type 'void (int)' failed during indirect function call") != std::string::npos,
        "__cfi_slowpath_diag reports a target outside every module: " + report);
    munmap(anonymous, pageSize);
}

void TestDependency(const Fixture& named, const char* path) {
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    Require(handle != nullptr, "the dependent fixture loads");
    auto* data = Symbol<unsigned char>(handle, "CfiDependentData_nid_no_patch");
    Require(dlsym(handle, "__cfi_check") == reinterpret_cast<void*>(named.check), "dlsym through the dependent fixture finds its dependency's __cfi_check");
    const auto calls = *named.calls;
    __cfi_slowpath_nid_postfix(13, data);
    __cfi_slowpath_diag_nid_postfix(13, data, nullptr);
    Require(*named.calls == calls, "a module with no __cfi_check of its own is unchecked, even when a dependency exports one");
    Require(dlclose(handle) == 0, "the dependent fixture unloads");
}

void TestUnloaded(Fixture& fixture) {
    auto* data = fixture.data;
    __cfi_slowpath_nid_postfix(11, data);
    Require(dlclose(fixture.handle) == 0, "the fixture unloads");
    fixture.handle = nullptr;
    Require(Throws([&] { __cfi_slowpath_nid_postfix(11, data); }), "a target in an unloaded module is an invalid region");
}

}

int main(int argc, char** argv) {
    Require(argc == 4, "usage: guest_kernel_sys_cfi_slowpath_tests <name fixture> <nid fixture> <dependent fixture>");
    auto named = Open(argv[1], "__cfi_check");
    auto nid = Open(argv[2], "iSB5fJWVqIU#guest");
    TestCheckedModule(named);
    TestCheckedModule(nid);
    TestUnchecked(named);
    TestInvalid();
    TestDependency(named, argv[3]);
    TestUnloaded(named);
    TestCheckedModule(nid);
    std::printf("CFI slow path tests passed\n");
    return 0;
}
