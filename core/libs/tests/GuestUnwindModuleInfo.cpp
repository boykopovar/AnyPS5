#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/GuestUnwindModuleInfoFixture.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

static std::vector<std::filesystem::path> longTree;

static std::wstring Extended(const std::filesystem::path& path) {
    return LR"(\\?\)" + std::filesystem::absolute(path).lexically_normal().make_preferred().wstring();
}

static void Require(bool value) {
    if (value) return;
    for (auto entry = longTree.rbegin(); entry != longTree.rend(); ++entry) {
        if (RemoveDirectoryW(Extended(*entry).c_str()) == FALSE) DeleteFileW(Extended(*entry).c_str());
    }
    std::abort();
}

static std::string NtName(const std::filesystem::path& file) {
    const HANDLE handle = CreateFileW(Extended(file).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    std::wstring name(32768, L'\0');
    const DWORD length = GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
    Require(CloseHandle(handle) != FALSE);
    Require(length > 0 && length < name.size());
    const int bytes = WideCharToMultiByte(CP_ACP, 0, name.data(), static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    Require(bytes > 0);
    std::string result(static_cast<std::size_t>(bytes), '\0');
    Require(WideCharToMultiByte(CP_ACP, 0, name.data(), static_cast<int>(length), result.data(), bytes, nullptr, nullptr) == bytes);
    return result;
}

static void RequireMappedName(const ModuleInfoForUnwind& info, const std::filesystem::path& file) {
    const auto expected = NtName(file);
    const auto length = std::min(expected.size(), sizeof(info.name) - 1);
    if (std::strlen(info.name) != length || _strnicmp(info.name, expected.c_str(), length) != 0) {
        std::fprintf(stderr, "unwind module name: expected the first %zu bytes of %s, got %s\n", length, expected.c_str(), info.name);
    }
    Require(std::strlen(info.name) == length && _strnicmp(info.name, expected.c_str(), length) == 0);
}

static std::filesystem::path ModuleFile(HMODULE module) {
    std::wstring file(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, file.data(), static_cast<DWORD>(file.size()));
    Require(length > 0 && length < file.size());
    file.resize(length);
    return file;
}

static void CreateJunction(const std::filesystem::path& link, const std::filesystem::path& target) {
    Require(CreateDirectoryW(Extended(link).c_str(), nullptr) != FALSE);
    const auto printName = std::filesystem::absolute(target).lexically_normal().make_preferred().wstring();
    const auto substituteName = L"\\??\\" + printName;
    struct MountPoint {
        ULONG tag;
        USHORT dataLength;
        USHORT reserved;
        USHORT substituteOffset;
        USHORT substituteLength;
        USHORT printOffset;
        USHORT printLength;
        wchar_t names[2048];
    } data{};
    Require(substituteName.size() + printName.size() + 2 <= std::size(data.names));
    data.tag = IO_REPARSE_TAG_MOUNT_POINT;
    data.substituteLength = static_cast<USHORT>(substituteName.size() * sizeof(wchar_t));
    data.printOffset = static_cast<USHORT>((substituteName.size() + 1) * sizeof(wchar_t));
    data.printLength = static_cast<USHORT>(printName.size() * sizeof(wchar_t));
    data.dataLength = static_cast<USHORT>(8 + data.printOffset + data.printLength + sizeof(wchar_t));
    std::memcpy(data.names, substituteName.c_str(), data.substituteLength + sizeof(wchar_t));
    std::memcpy(reinterpret_cast<char*>(data.names) + data.printOffset, printName.c_str(), data.printLength + sizeof(wchar_t));
    const HANDLE handle = CreateFileW(Extended(link).c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    DWORD returned = 0;
    const BOOL result = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data, 8 + data.dataLength, nullptr, 0, &returned, nullptr);
    Require(CloseHandle(handle) != FALSE);
    Require(result != FALSE);
}

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
        Require(sceKernelGetModuleInfoForUnwind(copy.address, 0, &info) == 0);
        RequireTerminatedFrames(info, round == 0 ? 4 + 12 + 4 + 12 + 4 : round == 1 ? 4 + 12 + 4 : 4);
        ModuleInfoForUnwind cached{};
        Require(sceKernelGetModuleInfoForUnwind(copy.address, 0, &cached) == 0);
        Require(std::memcmp(&cached, &info, sizeof(info)) == 0);
        Require(dlclose_nid_postfix(copy.handle) == 0);
    }
}

static void LongPathModuleIsNamed(const std::string& path) {
    const auto directory = std::filesystem::path(path).parent_path();
    const auto unique = std::to_string(GetCurrentProcessId());
    std::vector<std::filesystem::path> levels{directory / ("unwind-long-" + unique)};
    for (int level = 0; level < 5; ++level) levels.push_back(levels.back() / std::string(60, 'd'));
    const auto file = levels.back() / "unwind_fixture_long.dll";
    const auto link = directory / ("unwind-link-" + unique);
    longTree = levels;
    longTree.push_back(file);
    longTree.push_back(link);
    for (const auto& level : levels) Require(CreateDirectoryW(Extended(level).c_str(), nullptr) != FALSE);
    Require(std::filesystem::absolute(file).native().size() > MAX_PATH);
    Require(CopyFileW(Extended(path).c_str(), Extended(file).c_str(), FALSE) != FALSE);
    CreateJunction(link, levels.back());
    const auto copy = LoadFixtureCopy((link / "unwind_fixture_long.dll").string());
    ModuleInfoForUnwind info{};
    const int result = sceKernelGetModuleInfoForUnwind(copy.address, 0, &info);
    Require(dlclose_nid_postfix(copy.handle) == 0);
    Require(result == 0);
    RequireMappedName(info, file);
    Require(RemoveDirectoryW(Extended(link).c_str()) != FALSE);
    Require(DeleteFileW(Extended(file).c_str()) != FALSE);
    for (auto level = levels.rbegin(); level != levels.rend(); ++level) Require(RemoveDirectoryW(Extended(*level).c_str()) != FALSE);
    longTree.clear();
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
    RequireTerminatedFrames(info, 4 + 12 + 4);
    Require(info.seg0_addr == base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    Require(info.seg0_size == nt->OptionalHeader.SizeOfImage);
    RequireMappedName(info, ModuleFile(reinterpret_cast<HMODULE>(base)));
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
    LongPathModuleIsNamed(path);
    std::filesystem::remove(path);

    ModuleInfoForUnwind host{};
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 0, &host) == 0);
    Require(host.eh_frame_hdr_addr == 0 && host.eh_frame_addr == 0 && host.eh_frame_size == 0);
    ModuleInfoForUnwind hostAgain{};
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 0, &hostAgain) == 0);
    Require(std::memcmp(&hostAgain, &host, sizeof(host)) == 0);
}
