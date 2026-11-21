#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/GuestUnwindModuleInfoFixture.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <psapi.h>

static void Require(bool value) { if (!value) std::abort(); }

static bool Throws(std::uint64_t address) {
    ModuleInfoForUnwind info{};
    info.st_size = sizeof(ModuleInfoForUnwind);
    try {
        sceKernelGetModuleInfoForUnwind(address, 1, &info);

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool AllBytes(const ModuleInfoForUnwind& info, std::uint8_t value, std::size_t from) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&info);
    for (std::size_t index = from; index < sizeof(info); ++index) {
        if (bytes[index] != value) return false;
    }
    return true;
}

static ModuleInfoForUnwind Filled(std::uint64_t size) {
    ModuleInfoForUnwind info;
    std::memset(&info, 0xab, sizeof(info));
    info.st_size = size;
    return info;
}

static std::string FailureText(std::uint64_t address, int flags, ModuleInfoForUnwind* info) {
    try {
        sceKernelGetModuleInfoForUnwind(address, flags, info);
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

static void CheckContract(std::uint64_t found, std::uint64_t missing, const ModuleInfoForUnwind& expected) {
    for (const int flags : {0, 3, 4, 8, -1}) {
        auto info = Filled(sizeof(ModuleInfoForUnwind));
        Require(sceKernelGetModuleInfoForUnwind(found, flags, &info) == static_cast<int>(0x80020016));
        Require(AllBytes(info, 0, 0));
    }
    Require(FailureText(found, 0, nullptr) == "sceKernelGetModuleInfoForUnwind: a null output with flags other than 1 or 2, which the console clears without checking");
    Require(sceKernelGetModuleInfoForUnwind(found, 1, nullptr) == static_cast<int>(0x8002000e));
    for (const std::uint64_t size : {std::uint64_t{0}, std::uint64_t{0x12f}}) {
        auto info = Filled(size);
        Require(sceKernelGetModuleInfoForUnwind(found, 1, &info) == static_cast<int>(0x80020016));
        Require(info.st_size == size && AllBytes(info, 0xab, sizeof(info.st_size)));
    }
    auto wide = Filled(0x200);
    Require(sceKernelGetModuleInfoForUnwind(found, 1, &wide) == 0);
    Require(wide.st_size == 0x200);
    wide.st_size = expected.st_size;
    Require(std::memcmp(&wide, &expected, sizeof(wide)) == 0);
    auto second = Filled(sizeof(ModuleInfoForUnwind));
    Require(FailureText(found, 2, &second) == "sceKernelGetModuleInfoForUnwind: flags 2, a search by the module's second address range not implemented");
    auto absent = Filled(sizeof(ModuleInfoForUnwind));
    Require(sceKernelGetModuleInfoForUnwind(missing, 1, &absent) == static_cast<int>(0x80020003));
    Require(AllBytes(absent, 0, 0));
}

struct FixtureCopy {
    void* handle;
    UnwindFixture* fixture;
    std::uint64_t address;
};

static void RequireTerminatedFrames(const ModuleInfoForUnwind& info, std::uint64_t expectedSize) {
    if (info.eh_frame_size != expectedSize) {
        std::fprintf(stderr, "unwind table size: expected %llu including terminator, got %llu\n",
            static_cast<unsigned long long>(expectedSize), static_cast<unsigned long long>(info.eh_frame_size));
    }
    Require(info.eh_frame_size == expectedSize);
    Require(info.eh_frame_size >= sizeof(std::uint32_t));
    std::uint32_t terminator = 1;
    std::memcpy(&terminator, reinterpret_cast<const void*>(info.eh_frame_addr + info.eh_frame_size - sizeof(terminator)), sizeof(terminator));
    Require(terminator == 0);
}

static std::string FixtureCopyPath() {
    HMODULE linked = nullptr;
    Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(GetUnwindFixture()), &linked));
    wchar_t original[MAX_PATH] = {};
    Require(GetModuleFileNameW(linked, original, MAX_PATH) != 0);
    const auto copy = std::filesystem::path(original).replace_filename("unwind_fixture_copy.dll");
    std::filesystem::copy_file(original, copy, std::filesystem::copy_options::overwrite_existing);
    return copy.string();
}

static FixtureCopy LoadFixtureCopy(const std::string& path) {
    void* handle = dlopen_nid_postfix(path.c_str(), 2);
    Require(handle != nullptr);
    const auto get = reinterpret_cast<UnwindFixture* (*)()>(dlsym_nid_postfix(handle, "GetUnwindFixture_nid_no_patch"));
    Require(get != nullptr);
    auto* fixture = get();
    return {handle, fixture, reinterpret_cast<std::uint64_t>(fixture)};
}

template <typename Edit>
static void LookupOfEditedCopy(const std::string& path, Edit edit, bool throws) {
    const auto copy = LoadFixtureCopy(path);
    edit(*copy.fixture);
    Require(Throws(copy.address) == throws);
    Require(dlclose_nid_postfix(copy.handle) == 0);
}

static void ReloadedModuleIsRead(const std::string& path) {
    for (int round = 0; round < 3; ++round) {
        const auto copy = LoadFixtureCopy(path);
        if (round == 0) {
            copy.fixture->frames.terminator = 12;
            copy.fixture->frames.spare[0] = 1;
        } else if (round == 2) {
            copy.fixture->frames.cieLength = 0;
        }
        ModuleInfoForUnwind info{};
        info.st_size = sizeof(ModuleInfoForUnwind);
        Require(sceKernelGetModuleInfoForUnwind(copy.address, 1, &info) == 0);
        RequireTerminatedFrames(info, round == 0 ? 4 + 12 + 4 + 12 + 4 : round == 1 ? 4 + 12 + 4 : 4);
        ModuleInfoForUnwind cached{};
        cached.st_size = sizeof(ModuleInfoForUnwind);
        Require(sceKernelGetModuleInfoForUnwind(copy.address, 1, &cached) == 0);
        Require(std::memcmp(&cached, &info, sizeof(info)) == 0);
        Require(dlclose_nid_postfix(copy.handle) == 0);
    }
}

int main() {
    auto* fixture = GetUnwindFixture();
    Require(fixture != nullptr);
    const auto address = reinterpret_cast<std::uint64_t>(fixture);
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(reinterpret_cast<LPCVOID>(address), &memory, sizeof(memory)) != 0);
    const auto base = reinterpret_cast<std::uint64_t>(memory.AllocationBase);
    Require(base != reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr)));

    ModuleInfoForUnwind info{};

    info.st_size = sizeof(ModuleInfoForUnwind);
    Require(sceKernelGetModuleInfoForUnwind(address, 1, &info) == 0);
    Require(info.st_size == sizeof(ModuleInfoForUnwind));
    Require(info.eh_frame_hdr_addr == reinterpret_cast<std::uint64_t>(&fixture->header));
    Require(info.eh_frame_addr == reinterpret_cast<std::uint64_t>(&fixture->frames));
    RequireTerminatedFrames(info, 4 + 12 + 4);
    Require(info.seg0_addr == base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    Require(info.seg0_size == nt->OptionalHeader.SizeOfImage);
    char mapped[4096] = {};
    Require(GetMappedFileNameA(GetCurrentProcess(), memory.BaseAddress, mapped, sizeof(mapped) - 1) != 0);
    Require(std::strncmp(info.name, mapped, sizeof(info.name) - 1) == 0);
    for (const auto* inside : {static_cast<const void*>(&fixture->header), static_cast<const void*>(&fixture->frames)}) {
        ModuleInfoForUnwind other{};
        other.st_size = sizeof(ModuleInfoForUnwind);
        Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(inside), 1, &other) == 0);
        Require(std::memcmp(&other, &info, sizeof(info)) == 0);
    }
    for (int repeat = 0; repeat < 3; ++repeat) {
        ModuleInfoForUnwind again{};
        again.st_size = sizeof(ModuleInfoForUnwind);
        Require(sceKernelGetModuleInfoForUnwind(address, 1, &again) == 0);
        Require(std::memcmp(&again, &info, sizeof(info)) == 0);
    }

    CheckContract(address, 0xffff800000000000ull, info);

    const auto path = FixtureCopyPath();
    LookupOfEditedCopy(path, [](UnwindFixture& edited) { edited.header.version = 2; }, true);
    for (const std::uint8_t encoding : {0x3b, 0x9b, 0x0f, 0xff}) {
        LookupOfEditedCopy(path, [encoding](UnwindFixture& edited) { edited.header.framePointerEncoding = encoding; }, true);
    }
    LookupOfEditedCopy(path, [](UnwindFixture& edited) { edited.frames.cieLength = 0xffffffffu; }, true);
    LookupOfEditedCopy(path, [](UnwindFixture&) {}, false);
    ReloadedModuleIsRead(path);
    std::filesystem::remove(path);

    ModuleInfoForUnwind host{};

    host.st_size = sizeof(ModuleInfoForUnwind);
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 1, &host) == 0);
    Require(host.eh_frame_hdr_addr == 0 && host.eh_frame_addr == 0 && host.eh_frame_size == 0);
    ModuleInfoForUnwind hostAgain{};
    hostAgain.st_size = sizeof(ModuleInfoForUnwind);
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 1, &hostAgain) == 0);
    Require(std::memcmp(&hostAgain, &host, sizeof(host)) == 0);
}
