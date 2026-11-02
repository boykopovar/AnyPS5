#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/GuestUnwindModuleInfoFixture.hpp"
#include <cstdint>
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
    try {
        sceKernelGetModuleInfoForUnwind(address, 0, &info);

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct FixtureCopy {
    void* handle;
    UnwindFixture* fixture;
    std::uint64_t address;
};

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
    for (int round = 0; round < 2; ++round) {
        const auto copy = LoadFixtureCopy(path);
        if (round == 0) {
            copy.fixture->frames.terminator = 12;
            copy.fixture->frames.spare[0] = 1;
        }
        ModuleInfoForUnwind info{};
        Require(sceKernelGetModuleInfoForUnwind(copy.address, 0, &info) == 0);
        Require(info.eh_frame_size == (round == 0 ? 4 + 12 + 4 + 12 : 4 + 12));
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
    Require(sceKernelGetModuleInfoForUnwind(address, 0, &info) == 0);
    Require(info.st_size == sizeof(ModuleInfoForUnwind));
    Require(info.eh_frame_hdr_addr == reinterpret_cast<std::uint64_t>(&fixture->header));
    Require(info.eh_frame_addr == reinterpret_cast<std::uint64_t>(&fixture->frames));
    Require(info.eh_frame_size == 4 + 12);
    Require(info.seg0_addr == base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    Require(info.seg0_size == nt->OptionalHeader.SizeOfImage);
    char mapped[4096] = {};
    Require(GetMappedFileNameA(GetCurrentProcess(), memory.BaseAddress, mapped, sizeof(mapped) - 1) != 0);
    Require(std::strncmp(info.name, mapped, sizeof(info.name) - 1) == 0);
    for (const auto* inside : {static_cast<const void*>(&fixture->header), static_cast<const void*>(&fixture->frames)}) {
        ModuleInfoForUnwind other{};
        Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(inside), 0, &other) == 0);
        Require(std::memcmp(&other, &info, sizeof(info)) == 0);
    }
    for (int repeat = 0; repeat < 3; ++repeat) {
        ModuleInfoForUnwind again{};
        Require(sceKernelGetModuleInfoForUnwind(address, 0, &again) == 0);
        Require(std::memcmp(&again, &info, sizeof(info)) == 0);
    }

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
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 0, &host) == 0);
    Require(host.eh_frame_hdr_addr == 0 && host.eh_frame_addr == 0 && host.eh_frame_size == 0);
    ModuleInfoForUnwind hostAgain{};
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 0, &hostAgain) == 0);
    Require(std::memcmp(&hostAgain, &host, sizeof(host)) == 0);
}
