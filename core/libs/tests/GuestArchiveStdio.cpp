#include "AppArchiveFixture.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/KernelStatfs.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
int APS5_VABI fopen_s_nid_postfix(FileStream**, const char*, const char*);
FileStream* APS5_VABI freopen_nid_postfix(const char*, const char*, FileStream*);
int APS5_VABI fclose_nid_postfix(FileStream*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
int APS5_VABI fscanf_nid_postfix(FileStream*, const char*, ...);
char* APS5_VABI fgets_nid_postfix(char*, int, FileStream*);
char16_t* APS5_VABI fgetws_nid_postfix(char16_t*, int, FileStream*);
int APS5_VABI fseek_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream*);
void APS5_VABI rewind_nid_postfix(FileStream*);
int APS5_VABI ungetc_nid_postfix(int, FileStream*);
int APS5_VABI feof_nid_postfix(FileStream*);
void APS5_VABI clearerr_nid_postfix(FileStream*);
int APS5_VABI fileno_nid_postfix(FileStream*);
int APS5_VABI setvbuf_nid_postfix(FileStream*, char*, int, std::size_t);
int APS5_VABI dup_nid_postfix(int);
int APS5_VABI dup2_nid_postfix(int, int);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::size_t);
std::int64_t APS5_VABI write_nid_postfix(int, const void*, std::size_t);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
std::int64_t APS5_VABI pread_nid_postfix(int, void*, std::size_t, std::int64_t);
int APS5_VABI ftruncate_nid_postfix(int, std::int64_t);
int APS5_VABI fchmod_nid_postfix(int, int);
int APS5_VABI futimes_nid_postfix(int, const KernelTimeval*);
int APS5_VABI _fstatfs_nid_postfix(int, KernelStatfs*);
std::int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int, FileStat*);
int APS5_VABI stat_nid_postfix(const char*, FileStat*);
int APS5_VABI lstat_nid_postfix(const char*, FileStat*);
int APS5_VABI access_nid_postfix(const char*, int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr std::size_t BigSize = 3 * 65536 + 77;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t RegularType = 0100000;
constexpr std::uint16_t DirectoryType = 0040000;

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "archive stdio check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

template <typename TCall>
bool ThrowsNotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), "not implemented") != nullptr;
    }
    return false;
}

std::map<std::string, std::vector<char>> Contents() {
    return {
        {"Data/Big.bin", AppArchiveFixture::Pattern(BigSize)},
        {"Data/Empty.txt", {}},
        {"Data/Lines.txt", {'l', 'i', 'n', 'e', '\n', 's', 'e', 'c', 'o', 'n', 'd', '\n'}},
        {"Data/Override.bin", {'a', 'r', 'c', 'h', 'i', 'v', 'e'}},
        {"Data/Scan.txt", {'4', '2', ' ', 'a', 'n', 's', 'w', 'e', 'r'}},
        {"Data/Sub/Nested.txt", {'n', 'e', 's', 't', 'e', 'd'}},
        {"Data/Wide.txt", {'w', 'i', 'd', 'e', '\n'}},
    };
}

std::filesystem::path PrepareArchive() {
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-archive-stdio");
    AppArchiveFixture::WriteArchive(root / "app0.zar", Contents(), {"Data/Unused"});
    std::filesystem::create_directories(root / "app0" / "Data");
    {
        std::ofstream disk(root / "app0" / "Data" / "Override.bin", std::ios::binary);
        disk.write("disk", 4);
        Check(disk.good());
    }
    return root;
}

void Basic() {
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    auto* stream = fopen_nid_postfix("/APP0/data/BIG.BIN", "r");
    Check(stream != nullptr);
    Check(setvbuf_nid_postfix(stream, nullptr, 2, 0) == 0);
    std::vector<char> actual(BigSize + 32);
    Check(fread_nid_postfix(actual.data(), 1, 70000, stream) == 70000);
    Check(std::equal(actual.begin(), actual.begin() + 70000, expected.begin()));
    Check(ftell_nid_postfix(stream) == 70000);
    Check(fseek_nid_postfix(stream, 65530, SEEK_SET) == 0 && ftell_nid_postfix(stream) == 65530);
    Check(fread_nid_postfix(actual.data(), 1, 70000, stream) == 70000);
    Check(std::equal(actual.begin(), actual.begin() + 70000, expected.begin() + 65530));
    Check(fseek_nid_postfix(stream, -5, SEEK_END) == 0 && ftell_nid_postfix(stream) == static_cast<std::int64_t>(BigSize - 5));
    Check(fread_nid_postfix(actual.data(), 1, 32, stream) == 5);
    Check(std::equal(actual.begin(), actual.begin() + 5, expected.end() - 5));
    Check(fread_nid_postfix(actual.data(), 1, 1, stream) == 0 && feof_nid_postfix(stream) != 0);
    clearerr_nid_postfix(stream);
    Check(feof_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    Check(ftell_nid_postfix(stream) == 0 && feof_nid_postfix(stream) == 0);
    Check(fread_nid_postfix(actual.data(), 1, 1, stream) == 1 && actual[0] == expected[0]);
    Check(ungetc_nid_postfix('Z', stream) == 'Z' && ftell_nid_postfix(stream) == 0);
    Check(fread_nid_postfix(actual.data(), 1, 1, stream) == 1 && actual[0] == 'Z');
    Check(fclose_nid_postfix(stream) == 0);

    FileStream* checked = nullptr;
    Check(fopen_s_nid_postfix(&checked, "/app0/Data/Big.bin", "rb") == 0 && checked != nullptr);
    Check(fclose_nid_postfix(checked) == 0);

    stream = fopen_nid_postfix("/app0/data/empty.TXT", "rb");
    Check(stream != nullptr);
    Check(fread_nid_postfix(actual.data(), 1, 1, stream) == 0 && feof_nid_postfix(stream) != 0);
    Check(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix("/app0/Data/Lines.txt", "r");
    Check(stream != nullptr);
    char line[16]{};
    Check(fgets_nid_postfix(line, sizeof(line), stream) == line && std::strcmp(line, "line\n") == 0);
    Check(fgets_nid_postfix(line, sizeof(line), stream) == line && std::strcmp(line, "second\n") == 0);
    Check(fgets_nid_postfix(line, sizeof(line), stream) == nullptr && feof_nid_postfix(stream) != 0);
    Check(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix("/app0/Data/Scan.txt", "r");
    Check(stream != nullptr);
    int number = 0;
    char word[16]{};
    Check(fscanf_nid_postfix(stream, "%d %15s", &number, word) == 2);
    Check(number == 42 && std::strcmp(word, "answer") == 0);
    Check(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix("/app0/Data/Wide.txt", "r");
    Check(stream != nullptr);
    char16_t wide[16]{};
    Check(fgetws_nid_postfix(wide, 16, stream) == wide);
    Check(wide[0] == u'w' && wide[1] == u'i' && wide[2] == u'd' && wide[3] == u'e' && wide[4] == u'\n' && wide[5] == 0);
    Check(fclose_nid_postfix(stream) == 0);

    auto* first = fopen_nid_postfix("/app0/Data/Big.bin", "rb");
    auto* second = fopen_nid_postfix("/app0/Data/Big.bin", "rb");
    Check(first != nullptr && second != nullptr);
    Check(setvbuf_nid_postfix(first, nullptr, 2, 0) == 0 && setvbuf_nid_postfix(second, nullptr, 2, 0) == 0);
    Check(fread_nid_postfix(actual.data(), 1, 4, first) == 4 && std::equal(actual.begin(), actual.begin() + 4, expected.begin()));
    Check(ftell_nid_postfix(second) == 0);
    Check(fread_nid_postfix(actual.data(), 1, 4, second) == 4 && std::equal(actual.begin(), actual.begin() + 4, expected.begin()));
    Check(fclose_nid_postfix(first) == 0 && fclose_nid_postfix(second) == 0);

    stream = fopen_nid_postfix("/app0/Data/Override.bin", "rb");
    Check(stream != nullptr);
    std::memset(actual.data(), 0, actual.size());
    Check(fread_nid_postfix(actual.data(), 1, 8, stream) == 4 && std::memcmp(actual.data(), "disk", 4) == 0);
    Check(fclose_nid_postfix(stream) == 0);
}

void Descriptors() {
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    auto* stream = fopen_nid_postfix("/app0/Data/Big.bin", "rb");
    Check(stream != nullptr && setvbuf_nid_postfix(stream, nullptr, 2, 0) == 0);
    const int descriptor = fileno_nid_postfix(stream);
    Check(descriptor >= 0);
    FileStat descriptorStatus{}, pathStatus{};
    Check(fstat_nid_disambig1_nid_postfix(descriptor, &descriptorStatus) == 0);
    Check(stat_nid_postfix("/app0/Data/Big.bin", &pathStatus) == 0);
    Check((descriptorStatus.st_mode & TypeMask) == RegularType && (descriptorStatus.st_mode & 0222) == 0);
    Check(descriptorStatus.st_ino == pathStatus.st_ino && descriptorStatus.st_size == static_cast<std::int64_t>(BigSize));

    const int duplicate = dup_nid_postfix(descriptor);
    Check(duplicate >= 0);
    char bytes[32]{};
    Check(fread_nid_postfix(bytes, 1, 8, stream) == 8 && std::equal(bytes, bytes + 8, expected.begin()));
    Check(ftell_nid_postfix(stream) == 8);
    Check(read_nid_postfix(duplicate, bytes, 9) == 9 && std::equal(bytes, bytes + 9, expected.begin() + 8));
    Check(lseek_nid_postfix(descriptor, 65530, SEEK_SET) == 65530);
    Check(pread_nid_postfix(duplicate, bytes, 20, 65530) == 20 && std::equal(bytes, bytes + 20, expected.begin() + 65530));
    Check(lseek_nid_postfix(duplicate, 0, SEEK_CUR) == 65530);
    Check(write_nid_postfix(duplicate, "x", 1) == -1);
    Check(ftruncate_nid_postfix(duplicate, 0) == -1);
    Check(ThrowsNotImplemented([&] { (void)fchmod_nid_postfix(duplicate, 0600); }));
    Check(ThrowsNotImplemented([&] { (void)futimes_nid_postfix(duplicate, nullptr); }));
    KernelStatfs statfs{};
    Check(ThrowsNotImplemented([&] { (void)_fstatfs_nid_postfix(duplicate, &statfs); }));
    Check(fclose_nid_postfix(stream) == 0);
    FileStat duplicateStatus{};
    Check(fstat_nid_disambig1_nid_postfix(duplicate, &duplicateStatus) == 0 && duplicateStatus.st_size == static_cast<std::int64_t>(BigSize));
    Check(read_nid_postfix(duplicate, bytes, 11) == 11 && std::equal(bytes, bytes + 11, expected.begin() + 65530));
    Check(close_nid_postfix(duplicate) == 0);

    stream = fopen_nid_postfix("/app0/Data/Big.bin", "rb");
    Check(stream != nullptr && setvbuf_nid_postfix(stream, nullptr, 2, 0) == 0);
    const int oldDescriptor = fileno_nid_postfix(stream);
    Check(oldDescriptor >= 0);
    Check(freopen_nid_postfix("/app0/Data/Override.bin", "rb", stream) == stream);
    Check(fileno_nid_postfix(stream) == oldDescriptor);
    FileStat reusedStatus{};
    Check(fstat_nid_disambig1_nid_postfix(fileno_nid_postfix(stream), &reusedStatus) == 0);
    Check(reusedStatus.st_size == 4 && (reusedStatus.st_mode & 0222) != 0);
    std::memset(bytes, 0, sizeof(bytes));
    Check(fread_nid_postfix(bytes, 1, sizeof(bytes), stream) == 4 && std::memcmp(bytes, "disk", 4) == 0);
    Check(fclose_nid_postfix(stream) == 0);

    auto* archiveTarget = fopen_nid_postfix("/app0/Data/Big.bin", "rb");
    auto* diskSource = fopen_nid_postfix("/app0/Data/Override.bin", "rb");
    Check(archiveTarget != nullptr && diskSource != nullptr);
    const int targetDescriptor = fileno_nid_postfix(archiveTarget);
    const int sourceDescriptor = fileno_nid_postfix(diskSource);
    Check(targetDescriptor >= 0 && sourceDescriptor >= 0 && targetDescriptor != sourceDescriptor);
    Check(dup2_nid_postfix(sourceDescriptor, targetDescriptor) == targetDescriptor);
    FileStat replacedStatus{};
    Check(fstat_nid_disambig1_nid_postfix(targetDescriptor, &replacedStatus) == 0);
    Check(replacedStatus.st_size == 4 && (replacedStatus.st_mode & 0222) != 0);
    Check(pread_nid_postfix(targetDescriptor, bytes, 4, 0) == 4 && std::memcmp(bytes, "disk", 4) == 0);
    Check(fclose_nid_postfix(archiveTarget) == 0 && fclose_nid_postfix(diskSource) == 0);
}

void Errors() {
    for (const char* mode : {"r+", "w", "a"}) {
        *__error_nid_postfix() = 0;
        Check(fopen_nid_postfix("/app0/Data/Big.bin", mode) == nullptr);
        Check(*__error_nid_postfix() == EROFS);
    }
    *__error_nid_postfix() = 0;
    Check(fopen_nid_postfix("/app0/Data/Big.bin", "wx") == nullptr);
    Check(*__error_nid_postfix() == EEXIST);

    *__error_nid_postfix() = 0;
    Check(fopen_nid_postfix("/app0/Data/Unused", "r") == nullptr && *__error_nid_postfix() == EISDIR);
    Check(fopen_nid_postfix("/app0/Data/Unused", "w") == nullptr && *__error_nid_postfix() == EROFS);
    Check(fopen_nid_postfix("/app0/Data/Unused", "wx") == nullptr && *__error_nid_postfix() == EEXIST);
    Check(fopen_nid_postfix("/app0/Data/Big.bin", "r+x") == nullptr && *__error_nid_postfix() == EEXIST);
    FileStat status{}, linkStatus{};
    Check(stat_nid_postfix("/app0/data/big.BIN", &status) == 0);
    Check(lstat_nid_postfix("/app0/data/big.BIN", &linkStatus) == 0);
    Check((status.st_mode & TypeMask) == RegularType && status.st_size == static_cast<std::int64_t>(BigSize));
    Check(linkStatus.st_ino == status.st_ino && linkStatus.st_size == status.st_size && linkStatus.st_mode == status.st_mode);
    Check(access_nid_postfix("/app0/Data/Big.bin", 0) == 0);
    Check(access_nid_postfix("/app0/Data/Big.bin", 4) == 0);
    *__error_nid_postfix() = 0;
    Check(access_nid_postfix("/app0/Data/Big.bin", 2) == -1 && *__error_nid_postfix() == EROFS);
    *__error_nid_postfix() = 0;
    Check(access_nid_postfix("/app0/Data/Big.bin", 1) == -1 && *__error_nid_postfix() == EACCES);
    Check(access_nid_postfix("/app0/Data", 1) == 0);
    Check(access_nid_postfix("/app0/Data/Unused", 1) == 0);

    std::ofstream ordinary("ordinary.txt", std::ios::binary);
    ordinary << "ordinary";
    ordinary.close();
    auto* ordinaryStream = fopen_nid_postfix("ordinary.txt", "rb");
    Check(ordinaryStream != nullptr && fclose_nid_postfix(ordinaryStream) == 0);
    *__error_nid_postfix() = 0;
    Check(fopen_nid_postfix("missing.txt", "rb") == nullptr && *__error_nid_postfix() == ENOENT);
    *__error_nid_postfix() = 0;
    Check(access_nid_postfix("missing.txt", 0) == -1 && *__error_nid_postfix() == ENOENT);
}

void Absent() {
    std::ofstream plain(std::filesystem::current_path() / "app0" / "Plain.txt", std::ios::binary);
    plain << "plain";
    plain.close();
    auto* stream = fopen_nid_postfix("/app0/Plain.txt", "rb");
    Check(stream != nullptr);
    char contents[8]{};
    Check(fread_nid_postfix(contents, 1, sizeof(contents), stream) == 5 && std::memcmp(contents, "plain", 5) == 0);
    Check(fclose_nid_postfix(stream) == 0);
    *__error_nid_postfix() = 0;
    Check(fopen_nid_postfix("/app0/Data/Big.bin", "rb") == nullptr && *__error_nid_postfix() == ENOENT);
    FileStat status{};
    Check(stat_nid_postfix("/app0/Plain.txt", &status) == 0 && status.st_size == 5);
    Check(access_nid_postfix("/app0/Plain.txt", 4) == 0);
}

void Damaged(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", {
        {"Data/Corrupt.bin", std::vector<char>(65536, 'x')},
    });
    {
        std::fstream archive(root / "app0.zar", std::ios::binary | std::ios::in | std::ios::out);
        Check(archive.good());
        archive.put(static_cast<char>(0xff));
        Check(archive.good());
    }
    bool threw = false;
    try {
        (void)fopen_nid_postfix("/app0/Data/Corrupt.bin", "rb");
    } catch (const std::runtime_error& error) {
        threw = std::strstr(error.what(), "damaged") != nullptr;
    }
    Check(threw);
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    const auto root = mode == "absent"
        ? AppArchiveFixture::EnterFreshRoot("anyps5-archive-stdio-absent")
        : PrepareArchive();
    if (mode == "basic") Basic();
    else if (mode == "descriptors") Descriptors();
    else if (mode == "errors") Errors();
    else if (mode == "absent") Absent();
    else if (mode == "damaged") Damaged(root);
    else return 2;
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
