#include "prx/libc/include/Api.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;

int Delete(const char* data, std::size_t size) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, size);
    SaveDataDelete del{};
    del.dir_name = &name;
    return sceSaveDataDelete(&del);
}

int DeleteUnterminatedBeforeGuardPage() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous = 0;
    if (pages == nullptr || VirtualProtect(pages + page, page, PAGE_NOACCESS, &previous) == 0) {
        Expect(false, "the guard page is mapped");
        return 0;
    }
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    const std::size_t page = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 0;
    void* mapping = page != 0 ? mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) : MAP_FAILED;
    auto* pages = static_cast<char*>(mapping);
    if (mapping == MAP_FAILED || mprotect(pages + page, page, PROT_NONE) != 0) {
        Expect(false, "the guard page is mapped");
        return 0;
    }
#endif
    auto* name = reinterpret_cast<SceSaveDataDirName*>(pages + page - sizeof(SceSaveDataDirName));
    std::memset(name->data, 'a', sizeof(name->data));
    SaveDataDelete del{};
    del.dir_name = name;
    const int result = sceSaveDataDelete(&del);
#ifdef _WIN32
    Expect(VirtualFree(pages, 0, MEM_RELEASE) != 0, "the guard page is released");
#else
    Expect(munmap(pages, page * 2) == 0, "the guard page is released");
#endif
    return result;
}

void Write(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "data";
}

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool WriteMountedFile(const char* payload, std::size_t size) {
    auto* stream = fopen_nid_postfix("/savedata0/data.bin", "wb");
    if (stream == nullptr) return false;
    const bool written = fwrite_nid_postfix(payload, 1, size, stream) == size;
    return fclose_nid_postfix(stream) == 0 && written;
}

bool ReadMountedFile(std::array<char, 32>& buffer, std::size_t expectedSize) {
    auto* stream = fopen_nid_postfix("/savedata0/data.bin", "rb");
    if (stream == nullptr) return false;
    const bool read = fread_nid_postfix(buffer.data(), 1, expectedSize, stream) == expectedSize;
    return fclose_nid_postfix(stream) == 0 && read;
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-delete-" + std::to_string(std::random_device{}()));
    const auto work = root / "work";
    const auto kept = work / "_sd" / "kept" / "data.bin";
    const auto victim = work / "victim" / "important.txt";
    Write(kept);
    Write(victim);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    Expect(DeleteUnterminatedBeforeGuardPage() == SaveDataErrorParameter, "an unterminated name before a guard page is rejected");
    for (const char* invalid : {"../victim", "", ".", "..", "../..", "kept/..", "a\\b", "c:d"}) {
        Expect(Delete(invalid, std::strlen(invalid) + 1) == SaveDataErrorParameter, std::string("\"") + invalid + "\" is rejected");
        Expect(std::filesystem::exists(kept) && std::filesystem::exists(victim), std::string("\"") + invalid + "\" deletes nothing");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Expect(Delete(unterminated, sizeof(unterminated)) == SaveDataErrorParameter, "an unterminated name is rejected");
    Expect(sceSaveDataDelete(nullptr) == SaveDataErrorParameter, "null del returns PARAMETER");
    SaveDataDelete nullDirDel{};
    Expect(sceSaveDataDelete(&nullDirDel) == SaveDataErrorParameter, "null dir_name returns PARAMETER");
    Expect(sceSaveDataDeleteTransactionResource(0) == SaveDataErrorNotInitialized, "delete transaction resource fails before initialization");
    Expect(sceSaveDataDeleteTransactionResource(1) == SaveDataErrorNotInitialized, "delete transaction resource 1 fails before initialization");

    constexpr char mountedPayload[] = "live mounted save";
    constexpr char updatedPayload[] = "still mounted";
    Expect(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Expect(sceSaveDataDeleteTransactionResource(0) == 0, "delete transaction resource 0 succeeds after initialization");
    Expect(sceSaveDataDeleteTransactionResource(1) == 0, "delete transaction resource 1 succeeds after initialization");
    Expect(sceSaveDataDeleteTransactionResource(-1) == 0, "delete transaction resource -1 succeeds after initialization");
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "kept", sizeof("kept"));
    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = 2;
    SaveDataMountResult mountResult{};
    const int mountResultCode = sceSaveDataMount3(&mount, &mountResult);
    Expect(mountResultCode == 0, "the existing save mounts read/write");
    if (mountResultCode == 0) {
        Expect(WriteMountedFile(mountedPayload, sizeof(mountedPayload)), "the guest alias writes the mounted save");
        Expect(Delete("absent", sizeof("absent")) == 0, "a missing save can be deleted while another save is mounted");
        std::error_code caseAliasError;
        const auto casePath = work / "_sd" / "Kept";
        const bool caseAlias = std::filesystem::exists(casePath) && std::filesystem::equivalent(kept.parent_path(), casePath, caseAliasError);
        if (caseAliasError && caseAliasError != std::errc::no_such_file_or_directory) {
            Expect(false, "case-alias host identity probe has no unexpected filesystem error");
        } else if (caseAlias) {
            std::printf("savedata delete case-alias control: host paths are equivalent\n");
            Expect(Delete("Kept", sizeof("Kept")) == SaveDataErrorBusy, "case-variant mounted delete returns BUSY");
            Expect(Read(kept) == std::vector<char>(mountedPayload, mountedPayload + sizeof(mountedPayload)), "case-variant mounted delete preserves backing data");
            std::array<char, 32> caseAliasBuffer{};
            const bool readCaseAlias = ReadMountedFile(caseAliasBuffer, sizeof(mountedPayload));
            Expect(readCaseAlias && std::memcmp(caseAliasBuffer.data(), mountedPayload, sizeof(mountedPayload)) == 0, "case-variant mounted delete preserves the live guest alias");
        } else {
            std::printf("savedata delete case-alias control: host paths are distinct\n");
        }
        const int deleteResult = Delete("kept", sizeof("kept"));
        Expect(deleteResult == SaveDataErrorBusy, "mounted delete returns BUSY");
        Expect(Read(kept) == std::vector<char>(mountedPayload, mountedPayload + sizeof(mountedPayload)), "mounted data remains on disk after delete");
        std::array<char, 32> buffer{};
        const bool readMountedData = ReadMountedFile(buffer, sizeof(mountedPayload));
        Expect(readMountedData && std::memcmp(buffer.data(), mountedPayload, sizeof(mountedPayload)) == 0, "mounted data remains accessible through /savedata0");
        Expect(WriteMountedFile(updatedPayload, sizeof(updatedPayload)), "the live /savedata0 alias remains writable after delete");
        Expect(Read(kept) == std::vector<char>(updatedPayload, updatedPayload + sizeof(updatedPayload)), "writes through /savedata0 remain attached to the save");
        Expect(sceSaveDataUmount2(0, &mountResult.mount_point) == 0, "the mounted save unmounts");
    }

    Expect(Delete("kept", 5) == 0, "a valid name is deleted");
    Expect(!std::filesystem::exists(kept.parent_path()), "the valid save directory is gone");
    Expect(std::filesystem::exists(victim), "the directory beside the save root is untouched");
    Expect(sceSaveDataTerminate() == 0, "SaveData terminates");
    Expect(sceSaveDataDeleteTransactionResource(0) == SaveDataErrorNotInitialized, "delete transaction resource fails after termination");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (Failures() != 0) return 1;
    std::printf("savedata delete tests passed\n");
    return 0;
}
