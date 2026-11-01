#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <io.h>
#endif

namespace {

constexpr int ReadOnly = 0;
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;
constexpr std::int64_t Length = 64ll * 1024 * 1024;
constexpr char FileName[] = "anyps5_truncate_extend_file";

void Cleanup() {
    unlink_nid_postfix(FileName);

}

void Check(bool condition, const char* what, int line) {
    if (condition) return;
    std::fflush(stdout);
    std::fprintf(stderr, "line %d: %s (errno %d)\n", line, what, *__error_nid_postfix());
    Cleanup();
    std::abort();
}

#define Require(condition, what) Check((condition), (what), __LINE__)

#ifdef _WIN32
bool SupportsSparseFiles() {
    const auto handle = CreateFileW(L".", 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE, "open test directory failed");
    DWORD flags = 0;
    const bool queried = GetVolumeInformationByHandleW(handle, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0);
    const DWORD error = GetLastError();
    CloseHandle(handle);
    if (!queried) {
        std::printf("query test volume failed (Windows error %lu); allocation assertions skipped\n", error);
        return false;
    }
    return (flags & FILE_SUPPORTS_SPARSE_FILES) != 0;
}

unsigned int failedVolumeQueries = 0;

BOOL WINAPI FailVolumeQuery(HANDLE, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD) {
    ++failedVolumeQueries;
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

class VolumeQueryPatch {
public:
    VolumeQueryPatch() {
        const auto module = GetModuleHandleW(L"libkernel.prx");
        Require(module != nullptr, "find loaded libkernel failed");
        const auto base = reinterpret_cast<unsigned char*>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        Require(dos->e_magic == IMAGE_DOS_SIGNATURE, "libkernel DOS signature invalid");
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        Require(nt->Signature == IMAGE_NT_SIGNATURE, "libkernel PE signature invalid");
        const auto importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        Require(importRva != 0, "libkernel imports missing");
        const auto realQuery = reinterpret_cast<ULONG_PTR>(&GetVolumeInformationByHandleW);
        for (auto import = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + importRva); import->Name != 0; ++import) {
            for (auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + import->FirstThunk); thunk->u1.Function != 0; ++thunk) {
                if (thunk->u1.Function != realQuery) continue;
                Require(slot == nullptr, "multiple libkernel volume query IAT entries found");
                slot = &thunk->u1.Function;
            }
        }
        Require(slot != nullptr, "libkernel volume query IAT entry not found");
        original = *slot;
        replace(reinterpret_cast<ULONG_PTR>(&FailVolumeQuery));
    }

    ~VolumeQueryPatch() {
        replace(original);
    }

    VolumeQueryPatch(const VolumeQueryPatch&) = delete;
    VolumeQueryPatch& operator=(const VolumeQueryPatch&) = delete;

private:
    void replace(ULONG_PTR value) {
        DWORD protection = 0;
        Require(VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection), "make volume query IAT entry writable failed");
        *slot = value;
        DWORD previous = 0;
        Require(VirtualProtect(slot, sizeof(*slot), protection, &previous), "restore volume query IAT protection failed");
    }

    ULONG_PTR* slot = nullptr;
    ULONG_PTR original = 0;
};

void CheckSparseAllocation(bool supported) {
    if (!supported) return;
    DWORD high = 0;
    SetLastError(ERROR_SUCCESS);
    const DWORD low = GetCompressedFileSizeW(L"anyps5_truncate_extend_file", &high);
    Require(low != INVALID_FILE_SIZE || GetLastError() == ERROR_SUCCESS, "query allocation failed");
    const auto allocated = (static_cast<std::uint64_t>(high) << 32) | low;
    std::printf("64 MiB extension allocated %llu bytes\n", static_cast<unsigned long long>(allocated));
    Require(allocated < static_cast<std::uint64_t>(Length) / 16, "extension must leave a sparse hole, not reserve the new range");
    const DWORD attributes = GetFileAttributesW(L"anyps5_truncate_extend_file");
    Require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0,
        "extended file must have FILE_ATTRIBUTE_SPARSE_FILE");
}

void CheckSparseExtension(int mode, bool supported, bool queryFails = false) {
    Cleanup();
    std::printf("sparse extension path: %s\n", mode == 0 ? "descriptor" : mode == 1 ? "append-only reopened writer" : "path");
    const auto queriesBefore = failedVolumeQueries;
    int descriptor = open_nid_postfix(FileName, Create | ReadWrite, 0644);
    Require(descriptor >= 0, "open sparse test file failed");
    Require(write_nid_postfix(descriptor, "head", 4) == 4, "write sparse test prefix failed");
    if (mode == 1) {
        Require(close_nid_postfix(descriptor) == 0, "close before append-only reopen failed");
        const auto handle = CreateFileW(L"anyps5_truncate_extend_file", FILE_GENERIC_READ | (FILE_GENERIC_WRITE & ~FILE_WRITE_DATA),
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        Require(handle != INVALID_HANDLE_VALUE, "open append-only handle failed");
        descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDWR | _O_APPEND | _O_BINARY);
        Require(descriptor >= 0, "wrap append-only handle failed");
    }
    const DWORD originalAttributes = GetFileAttributesW(L"anyps5_truncate_extend_file");
    Require(originalAttributes != INVALID_FILE_ATTRIBUTES, "query initial attributes failed");
    Require(ftruncate_nid_postfix(descriptor, 4) == 0, "non-extending truncate failed");
    Require(ftruncate_nid_postfix(descriptor, 2) == 0, "initial shrink failed");
    Require((GetFileAttributesW(L"anyps5_truncate_extend_file") & FILE_ATTRIBUTE_SPARSE_FILE) ==
        (originalAttributes & FILE_ATTRIBUTE_SPARSE_FILE), "shrinking and non-extending calls must not mark the file sparse");
    const int checker = open_nid_postfix(FileName, ReadWrite, 0);
    Require(checker >= 0, "open verification descriptor failed");
    Require(sceKernelPwrite(checker, "ad", 2, 2) == 2, "restore prefix failed");
    Require(lseek_nid_postfix(descriptor, 137, 0) == 137, "set sparse test cursor failed");
    const auto resize = [&](std::int64_t length) {
        return mode == 2 ? truncate_nid_postfix(FileName, length) : ftruncate_nid_postfix(descriptor, length);
    };
    for (int round = 0; round < 2; ++round) {
        Require(resize(Length) == 0, "sparse extension failed");
        Require(lseek_nid_postfix(descriptor, 0, 1) == 137, "extension changed the file position");
        Require(lseek_nid_postfix(checker, 0, 2) == Length, "sparse extension has wrong size");
        CheckSparseAllocation(supported);
        if (queryFails) {
            const DWORD attributes = GetFileAttributesW(L"anyps5_truncate_extend_file");
            Require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_SPARSE_FILE) == 0,
                "unknown volume capabilities must not mark the file sparse");
        }
        char prefix[4]{};
        const int prefixSize = round == 0 ? 4 : 2;
        Require(pread_nid_postfix(checker, prefix, prefixSize, 0) == prefixSize && std::memcmp(prefix, "head", prefixSize) == 0,
            "sparse extension changed original bytes");
        std::array<char, 65536> buffer{};
        for (std::int64_t offset = prefixSize; offset < Length;) {
            const auto count = static_cast<std::size_t>((std::min)(Length - offset, static_cast<std::int64_t>(buffer.size())));
            buffer.fill(1);
            Require(pread_nid_postfix(checker, buffer.data(), count, offset) == static_cast<std::int64_t>(count), "read sparse hole failed");
            Require(std::all_of(buffer.begin(), buffer.begin() + count, [](char byte) { return byte == 0; }), "sparse hole must read entirely as zeros");
            offset += count;
        }
        Require(sceKernelPwrite(checker, "tail", 4, Length - 4) == 4, "write near sparse end failed");
        char tail[4]{};
        Require(pread_nid_postfix(checker, tail, 4, Length - 4) == 4 && std::memcmp(tail, "tail", 4) == 0, "write near sparse end landed incorrectly");
        Require(lseek_nid_postfix(descriptor, 0, 1) == 137, "verification changed the original cursor");
        Require(resize(2) == 0, "sparse shrink failed");
        Require(lseek_nid_postfix(descriptor, 0, 1) == 137, "shrink changed the file position");
        Require(lseek_nid_postfix(checker, 0, 2) == 2, "sparse shrink has wrong size");
    }
    Require(close_nid_postfix(checker) == 0, "close verification descriptor failed");
    Require(close_nid_postfix(descriptor) == 0, "close sparse descriptor failed");
    if (queryFails) {
        std::printf("failed capability query regression: %u calls\n", failedVolumeQueries - queriesBefore);
        Require(failedVolumeQueries > queriesBefore, "libkernel must call the failing volume capability query");
    }
    Cleanup();
}
#endif

std::uint64_t BytesWritten() {
#ifdef _WIN32
    IO_COUNTERS counters{};
    return GetProcessIoCounters(GetCurrentProcess(), &counters) ? counters.WriteTransferCount : 0;
#else
    return 0;
#endif
}

}

int main() {
#ifdef _WIN32
    const bool sparseSupported = SupportsSparseFiles();
    std::printf("test volume sparse support: %s\n", sparseSupported ? "yes" : "no; allocation assertions skipped");
    for (int mode = 0; mode < 3; ++mode) CheckSparseExtension(mode, sparseSupported);
    {
        const VolumeQueryPatch patch;
        for (int mode = 0; mode < 3; ++mode) CheckSparseExtension(mode, false, true);
    }
#endif
    Cleanup();
    const int descriptor = open_nid_postfix(FileName, Create | ReadWrite, 0644);
    Require(descriptor >= 0, "open failed");
    Require(write_nid_postfix(descriptor, "head", 4) == 4, "write failed");

    const std::uint64_t before = BytesWritten();
    Require(ftruncate_nid_postfix(descriptor, Length) == 0, "ftruncate to 64 MiB failed");
    const std::uint64_t written = BytesWritten() - before;
    std::printf("extending by 64 MiB wrote %llu bytes\n", static_cast<unsigned long long>(written));
#ifdef _WIN32
    Require(written < static_cast<std::uint64_t>(Length) / 16, "extending a file must not write its new length as zero bytes: the process wrote about as many bytes as the new length");
#endif

    Require(lseek_nid_postfix(descriptor, 0, 2) == Length, "the extended file has the wrong size");
    Require(lseek_nid_postfix(descriptor, 0, 1) == Length, "ftruncate must not move the file offset");
    char head[4] = {};
    char tail[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    Require(pread_nid_postfix(descriptor, head, 4, 0) == 4 && std::memcmp(head, "head", 4) == 0, "extending must keep the existing bytes");
    Require(pread_nid_postfix(descriptor, tail, 8, Length - 8) == 8 && tail[0] == 0 && tail[7] == 0, "the extension must read as zero bytes");

    Require(ftruncate_nid_postfix(descriptor, 2) == 0, "shrinking failed");
    Require(lseek_nid_postfix(descriptor, 0, 2) == 2, "the shrunk file has the wrong size");
    Require(ftruncate_nid_postfix(descriptor, -1) == -1 && *__error_nid_postfix() == 22, "a negative length must fail with EINVAL (22)");
    Require(close_nid_postfix(descriptor) == 0, "close failed");

    const int readOnly = open_nid_postfix(FileName, ReadOnly, 0);
    Require(readOnly >= 0, "reopen failed");
    Require(ftruncate_nid_postfix(readOnly, 8) == -1 && *__error_nid_postfix() == 22, "ftruncate on a read-only descriptor must fail with EINVAL (22)");
    Require(close_nid_postfix(readOnly) == 0, "close failed");

    Require(truncate_nid_postfix(FileName, Length) == 0, "truncate to 64 MiB failed");
    const int check = open_nid_postfix(FileName, ReadOnly, 0);
    Require(check >= 0 && lseek_nid_postfix(check, 0, 2) == Length, "truncate by path left the wrong size");
    Require(close_nid_postfix(check) == 0, "close failed");
    Require(truncate_nid_postfix(FileName, -1) == -1 && *__error_nid_postfix() == 22, "truncate by path with negative length must fail with EINVAL (22)");
    Require(truncate_nid_postfix(FileName, -100) == -1 && *__error_nid_postfix() == 22, "truncate by path with negative length must fail with EINVAL (22)");
    Cleanup();
    return 0;
}
