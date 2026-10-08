#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"
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

extern "C" {
int APS5_VABI sceSaveDataDelete(const SaveDataDelete*);
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
std::size_t APS5_VABI fwrite_nid_postfix(const void*, std::size_t, std::size_t, FileStream*);
int APS5_VABI fclose_nid_postfix(FileStream*);
}

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorBusy = -2137063421;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata delete check failed: %s\n", what.c_str());
    ++failures;
}

int Delete(const char* data, std::size_t size) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, size);
    SaveDataDelete del{};
    del.dir_name = &name;
    return sceSaveDataDelete(&del);
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

    for (const char* invalid : {"../victim", "", ".", "..", "../..", "kept/..", "a\\b", "c:d"}) {
        Check(Delete(invalid, std::strlen(invalid) + 1) == SaveDataErrorParameter, std::string("\"") + invalid + "\" is rejected");
        Check(std::filesystem::exists(kept) && std::filesystem::exists(victim), std::string("\"") + invalid + "\" deletes nothing");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Check(Delete(unterminated, sizeof(unterminated)) == SaveDataErrorParameter, "an unterminated name is rejected");

    constexpr char mountedPayload[] = "live mounted save";
    constexpr char updatedPayload[] = "still mounted";
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "kept", sizeof("kept"));
    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = 2;
    SaveDataMountResult mountResult{};
    const int mountResultCode = sceSaveDataMount3(&mount, &mountResult);
    Check(mountResultCode == 0, "the existing save mounts read/write");
    if (mountResultCode == 0) {
        Check(WriteMountedFile(mountedPayload, sizeof(mountedPayload)), "the guest alias writes the mounted save");
        std::error_code caseAliasError;
        const bool caseAlias = std::filesystem::equivalent(kept.parent_path(), work / "_sd" / "Kept", caseAliasError);
        if (caseAliasError && caseAliasError != std::errc::no_such_file_or_directory) {
            Check(false, "case-alias host identity probe has no unexpected filesystem error");
        } else if (caseAlias) {
            std::printf("savedata delete case-alias control: host paths are equivalent\n");
            Check(Delete("Kept", sizeof("Kept")) == SaveDataErrorBusy, "case-variant mounted delete returns BUSY");
            Check(Read(kept) == std::vector<char>(mountedPayload, mountedPayload + sizeof(mountedPayload)), "case-variant mounted delete preserves backing data");
            std::array<char, 32> caseAliasBuffer{};
            const bool readCaseAlias = ReadMountedFile(caseAliasBuffer, sizeof(mountedPayload));
            Check(readCaseAlias && std::memcmp(caseAliasBuffer.data(), mountedPayload, sizeof(mountedPayload)) == 0, "case-variant mounted delete preserves the live guest alias");
        } else {
            std::printf("savedata delete case-alias control: host paths are distinct\n");
        }
        const int deleteResult = Delete("kept", sizeof("kept"));
        Check(deleteResult == SaveDataErrorBusy, "mounted delete returns BUSY");
        Check(Read(kept) == std::vector<char>(mountedPayload, mountedPayload + sizeof(mountedPayload)), "mounted data remains on disk after delete");
        std::array<char, 32> buffer{};
        const bool readMountedData = ReadMountedFile(buffer, sizeof(mountedPayload));
        Check(readMountedData && std::memcmp(buffer.data(), mountedPayload, sizeof(mountedPayload)) == 0, "mounted data remains accessible through /savedata0");
        Check(WriteMountedFile(updatedPayload, sizeof(updatedPayload)), "the live /savedata0 alias remains writable after delete");
        Check(Read(kept) == std::vector<char>(updatedPayload, updatedPayload + sizeof(updatedPayload)), "writes through /savedata0 remain attached to the save");
        Check(sceSaveDataUmount2(0, &mountResult.mount_point) == 0, "the mounted save unmounts");
    }

    Check(Delete("kept", 5) == 0, "a valid name is deleted");
    Check(!std::filesystem::exists(kept.parent_path()), "the valid save directory is gone");
    Check(std::filesystem::exists(victim), "the directory beside the save root is untouched");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata delete tests passed\n");
    return 0;
}
