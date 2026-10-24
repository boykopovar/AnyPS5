#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <barrier>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <fcntl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode);
FileStream* APS5_VABI _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const char*, int, int);
FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream);
int APS5_VABI fclose_nid_postfix(FileStream* stream);
std::size_t APS5_VABI fread_nid_postfix(void* buffer, std::size_t size, std::size_t count, FileStream* stream);
std::size_t APS5_VABI fwrite_nid_postfix(const void* buffer, std::size_t size, std::size_t count, FileStream* stream);
int APS5_VABI fseek_nid_postfix(FileStream* stream, std::int64_t offset, int origin);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream* stream);
int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream);
int APS5_VABI fflush_nid_postfix(FileStream* stream);
int APS5_VABI remove_nid_postfix(const char* path);
int APS5_VABI rename_nid_postfix(const char* from, const char* to);
int APS5_VABI ferror_nid_postfix(FileStream* stream);
int APS5_VABI feof_nid_postfix(FileStream* stream);
void APS5_VABI clearerr_nid_postfix(FileStream* stream);
int APS5_VABI fgetc_nid_postfix(FileStream* stream);
int APS5_VABI getc_unlocked_nid_postfix(FileStream* stream);
char* APS5_VABI fgets_nid_postfix(char* buffer, int size, FileStream* stream);
int APS5_VABI ungetc_nid_postfix(int value, FileStream* stream);
int APS5_VABI fscanf_nid_postfix(FileStream* stream, const char* format, ...);
}

static void Require(bool condition, const char* message = "File stream check failed") {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
static void ExpectException(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("Expected a stream exception");
}

static void ReplaceWithReadOnlyDescriptor(FileStream* stream, const char* path) {
#ifdef _WIN32
    const int descriptor = _open(path, _O_RDONLY | _O_BINARY);
    Require(descriptor >= 0 && _dup2(descriptor, _fileno(GetNativeStream(stream))) == 0);
    Require(_close(descriptor) == 0);
#else
    const int descriptor = open(path, O_RDONLY);
    Require(descriptor >= 0 && dup2(descriptor, fileno(GetNativeStream(stream))) >= 0);
    Require(close(descriptor) == 0);
#endif
}

static void CheckExclusiveCppOpen(const std::string& base) {
    constexpr int Out = 0x02, Binary = 0x80, Noreplace = 0x40;
    const std::string path = base + ".exclusive";
    std::remove(path.c_str());
    auto* stream = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), Out | Binary | Noreplace, 0);
    Require(stream != nullptr);
    constexpr char original[] = "keep existing contents";
    Require(fwrite_nid_postfix(original, 1, sizeof(original), stream) == sizeof(original));
    Require(fclose_nid_postfix(stream) == 0);
#ifdef _WIN32
    const auto nativePath = std::filesystem::path(path).wstring();
    const auto writer = CreateFileW(nativePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(writer != INVALID_HANDLE_VALUE);
    auto* refused = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), Out | Binary | Noreplace, 0);
    if (refused) fclose_nid_postfix(refused);
    Require(CloseHandle(writer) != 0);
    Require(refused == nullptr, "C++ noreplace truncated an existing write-shared file");
#endif
    stream = fopen_nid_postfix(path.c_str(), "rb");
    Require(stream != nullptr);
    std::array<char, sizeof(original)> saved{};
    Require(fread_nid_postfix(saved.data(), 1, saved.size(), stream) == saved.size());
    Require(std::memcmp(saved.data(), original, saved.size()) == 0);
    Require(fclose_nid_postfix(stream) == 0 && std::remove(path.c_str()) == 0);
    for (const int mode : {0x42, 0x52, 0x53, 0xc2, 0xd2, 0xd3}) {
        stream = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), mode | 0x04, 0);
        Require(stream != nullptr && ftell_nid_postfix(stream) == 0);
        Require(fwrite_nid_postfix(original, 1, sizeof(original), stream) == sizeof(original));
        Require(fclose_nid_postfix(stream) == 0);
        errno = 0;
        Require(_ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), mode, 0) == nullptr && errno == EEXIST);
        stream = fopen_nid_postfix(path.c_str(), "rb");
        Require(stream != nullptr && fread_nid_postfix(saved.data(), 1, saved.size(), stream) == saved.size());
        Require(std::memcmp(saved.data(), original, saved.size()) == 0);
        Require(fclose_nid_postfix(stream) == 0 && std::remove(path.c_str()) == 0);
    }
    for (int round = 0; round < 12; ++round) {
        std::array<FileStream*, 2> created{};
        std::barrier start(2);
        const auto create = [&](int index) {
            start.arrive_and_wait();
            created[index] = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), Out | Binary | Noreplace, 0);
        };
        std::thread first(create, 0), second(create, 1);
        first.join();
        second.join();
        const auto winners = static_cast<int>(created[0] != nullptr) + static_cast<int>(created[1] != nullptr);
        for (auto* opened : created) if (opened) Require(fclose_nid_postfix(opened) == 0);
        Require(winners == 1 && std::remove(path.c_str()) == 0);
    }
}

static std::string ReadWhole(const std::string& path) {
    auto* stream = fopen_nid_postfix(path.c_str(), "rb");
    Require(stream != nullptr);
    std::array<char, 64> buffer{};
    const auto size = fread_nid_postfix(buffer.data(), 1, buffer.size(), stream);
    Require(fclose_nid_postfix(stream) == 0);
    return std::string(buffer.data(), size);
}

template<typename TRead>
static void CheckReadAfterWrite(const std::string& path, const char* mode, const char* expected, TRead read) {
    auto* stream = fopen_nid_postfix(path.c_str(), "wb");
    Require(stream != nullptr && fputs_nid_postfix("0123456789", stream) >= 0 && fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(path.c_str(), mode);
    Require(stream != nullptr && fwrite_nid_postfix("AB", 1, 2, stream) == 2);
    read(stream);
    Require(ferror_nid_postfix(stream) == 0, "Read after write on an update stream set the error indicator");
    Require(fclose_nid_postfix(stream) == 0);
    Require(ReadWhole(path) == expected, "Read after write on an update stream changed the file");
}

static void CheckUpdateStreamReadsAfterWrites(const std::string& base) {
    const std::string path = base + ".update";
    constexpr char overwritten[] = "AB23456789";
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fread_nid_postfix(buffer.data(), 1, 3, stream) == 3, "fread after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "fread after fwrite on an r+ stream read other bytes");
        Require(ftell_nid_postfix(stream) == 5);
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::vector<char> buffer(65536);
        Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == 8, "fread of more than the stream buffer after fwrite on an r+ stream");
        Require(std::string_view(buffer.data(), 8) == "23456789", "fread of more than the stream buffer after fwrite on an r+ stream read other bytes");
        Require(ftell_nid_postfix(stream) == 10);
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == '2', "fgetc after fwrite on an r+ stream");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        Require(getc_unlocked_nid_postfix(stream) == '2', "getc_unlocked after fwrite on an r+ stream");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fgets_nid_postfix(buffer.data(), static_cast<int>(buffer.size()), stream) == buffer.data(), "fgets after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "fgets after fwrite on an r+ stream read other bytes");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        Require(ungetc_nid_postfix('Z', stream) == 'Z', "ungetc after fwrite on an r+ stream");
        Require(fgetc_nid_postfix(stream) == 'Z' && fgetc_nid_postfix(stream) == '2', "ungetc after fwrite on an r+ stream pushed back other bytes");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fscanf_nid_postfix(stream, "%3s", buffer.data()) == 1, "fscanf after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "fscanf after fwrite on an r+ stream read other bytes");
    });
    CheckReadAfterWrite(path, "w+b", "AB", [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == 0 && feof_nid_postfix(stream) != 0, "fread after fwrite on a w+ stream");
    });
    CheckReadAfterWrite(path, "a+b", "0123456789AB", [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == EOF && feof_nid_postfix(stream) != 0, "fgetc after fwrite on an a+ stream");
    });
    auto* stream = fopen_nid_postfix(path.c_str(), "r+b");
    Require(stream != nullptr);
    Require(ungetc_nid_postfix('Z', stream) == 'Z', "ungetc on a new r+ stream");
    Require(fgetc_nid_postfix(stream) == 'Z' && fgetc_nid_postfix(stream) == '0', "ungetc on a new r+ stream pushed back other bytes");
    Require(fseek_nid_postfix(stream, 2, SEEK_SET) == 0 && ungetc_nid_postfix('Y', stream) == 'Y', "ungetc after fseek on an r+ stream");
    Require(fgetc_nid_postfix(stream) == 'Y' && fgetc_nid_postfix(stream) == '2', "ungetc after fseek on an r+ stream pushed back other bytes");
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::remove(path.c_str()) == 0);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    CheckExclusiveCppOpen(argv[1]);
    CheckUpdateStreamReadsAfterWrites(argv[1]);
    Require(_Stdout_nid_postfix.GetHandle() == stdout);
    Require(_Stderr_nid_postfix.GetHandle() == stderr);
    Require(fputs_nid_postfix("stdout object works\n", &_Stdout_nid_postfix) >= 0);
    Require(fputs_nid_postfix("stderr object works\n", &_Stderr_nid_postfix) >= 0);
    Require(fflush_nid_postfix(nullptr) == 0);
    auto* stream = fopen_nid_postfix(argv[1], "w+b");
    constexpr char payload[] = "stream round trip";
    Require(fputs_nid_postfix(payload, stream) >= 0);
    Require(fwrite_nid_postfix(payload, 1, sizeof(payload), stream) == sizeof(payload));
    const auto expectedSize = sizeof(payload) * 2 - 1;
    Require(ftell_nid_postfix(stream) == static_cast<long>(expectedSize));
    Require(fflush_nid_postfix(stream) == 0);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> buffer{};
    Require(fread_nid_postfix(buffer.data(), 1, sizeof(payload) - 1, stream) == sizeof(payload) - 1);
    Require(std::strcmp(buffer.data(), payload) == 0);
    Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == buffer.size());
    Require(std::strcmp(buffer.data(), payload) == 0);
    Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == 0);
    ExpectException([&] { fputs_nid_postfix(nullptr, stream); });
    ExpectException([&] { fwrite_nid_postfix(nullptr, 1, sizeof(payload), stream); });
    errno = 0;
    Require(fread_nid_postfix(buffer.data(), 2, SIZE_MAX / 2 + 1, stream) == 0 && errno == EINVAL && ferror_nid_postfix(stream) == 0);
    errno = 0;
    Require(fread_nid_postfix(buffer.data(), 2, SIZE_MAX / 2 + 1, nullptr) == 0 && errno == EINVAL);
    ExpectException([&] { fwrite_nid_postfix(buffer.data(), SIZE_MAX / 2 + 1, 2, stream); });
    errno = 0;
    Require(fseek_nid_postfix(stream, 0, -1) == -1);
    Require(errno == EINVAL);
    Require(fclose_nid_postfix(stream) == 0);
    ExpectException([] { fputs_nid_postfix("invalid stream", nullptr); });
    ExpectException([] { fopen_nid_postfix(nullptr, "r"); });
    FileStream closed(std::tmpfile());
    closed.Close();
    Require(fflush_nid_postfix(&closed) == -1 && errno == 9);
    Require(std::remove(argv[1]) == 0);
    errno = 0;
    Require(fopen_nid_postfix(argv[1], "rb") == nullptr);
    Require(errno == ENOENT);
    const std::string directoryName = std::string(argv[1]) + ".dir";
    Require(std::filesystem::create_directory(directoryName));
    errno = 0;
    Require(fopen_nid_postfix(directoryName.c_str(), "wb") == nullptr);
    Require(errno == EISDIR);
    Require(std::filesystem::remove(directoryName));
    const std::u8string unicodeName = u8"セーブé.tmp";
    const std::string guestName(unicodeName.begin(), unicodeName.end());
    const std::filesystem::path hostName(unicodeName);
    constexpr char unicodePayload[] = "unicode";
    stream = fopen_nid_postfix(guestName.c_str(), "wb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix(unicodePayload, 1, sizeof(unicodePayload), stream) == sizeof(unicodePayload));
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::filesystem::exists(hostName));
    Require(std::filesystem::file_size(hostName) == sizeof(unicodePayload));
    stream = fopen_nid_postfix(guestName.c_str(), "rb");
    Require(stream != nullptr);
    std::array<char, sizeof(unicodePayload)> unicodeBuffer{};
    Require(fread_nid_postfix(unicodeBuffer.data(), 1, unicodeBuffer.size(), stream) == unicodeBuffer.size());
    Require(std::strcmp(unicodeBuffer.data(), unicodePayload) == 0);
    stream = freopen_nid_postfix(guestName.c_str(), "wb", stream);
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::filesystem::file_size(hostName) == 0);
    std::filesystem::remove(hostName);
    const std::string heldName = std::string(argv[1]) + ".held";
    const std::string movedName = std::string(argv[1]) + ".moved";
    stream = fopen_nid_postfix(heldName.c_str(), "w+b");
    Require(stream != nullptr && fwrite_nid_postfix(payload, 1, sizeof(payload), stream) == sizeof(payload));
    Require(fflush_nid_postfix(stream) == 0);
    Require(rename_nid_postfix(heldName.c_str(), movedName.c_str()) == 0 && !std::filesystem::exists(heldName));
    Require(remove_nid_postfix(movedName.c_str()) == 0 && !std::filesystem::exists(movedName));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> heldBuffer{};
    Require(fread_nid_postfix(heldBuffer.data(), 1, heldBuffer.size(), stream) == heldBuffer.size());
    Require(std::strcmp(heldBuffer.data(), payload) == 0 && fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(heldName.c_str(), "ab");
    Require(stream != nullptr && fputs_nid_postfix("ab", stream) >= 0 && fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(heldName.c_str(), "ab");
    Require(stream != nullptr && fputs_nid_postfix("cd", stream) >= 0 && fclose_nid_postfix(stream) == 0);
    errno = 0;
    Require(fopen_nid_postfix(heldName.c_str(), "wbx") == nullptr && errno == EEXIST);
    stream = fopen_nid_postfix(heldName.c_str(), "rb");
    std::array<char, 5> appended{};
    Require(stream != nullptr && fread_nid_postfix(appended.data(), 1, 4, stream) == 4 && std::strcmp(appended.data(), "abcd") == 0);
    Require(fclose_nid_postfix(stream) == 0 && remove_nid_postfix(heldName.c_str()) == 0);
    stream = fopen_nid_postfix(heldName.c_str(), "w+b");
    Require(stream != nullptr);
    stream = freopen_nid_postfix(movedName.c_str(), "w+b", stream);
    Require(stream != nullptr && fwrite_nid_postfix(payload, 1, sizeof(payload), stream) == sizeof(payload));
    Require(fflush_nid_postfix(stream) == 0);
    Require(rename_nid_postfix(movedName.c_str(), heldName.c_str()) == 0 && !std::filesystem::exists(movedName));
    Require(remove_nid_postfix(heldName.c_str()) == 0 && !std::filesystem::exists(heldName));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> reopenedBuffer{};
    Require(fread_nid_postfix(reopenedBuffer.data(), 1, reopenedBuffer.size(), stream) == reopenedBuffer.size());
    Require(std::strcmp(reopenedBuffer.data(), payload) == 0 && fclose_nid_postfix(stream) == 0);
    constexpr char marker[] = "marker";
    stream = fopen_nid_postfix(argv[1], "wb");
    Require(stream != nullptr);
    std::array<char, sizeof(marker)> markerBuffer{};
    Require(fread_nid_postfix(markerBuffer.data(), 1, markerBuffer.size(), stream) == 0);
    Require(ferror_nid_postfix(stream) != 0);
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(argv[1], "rb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix(marker, 1, sizeof(marker), stream) == 0);
    Require(ferror_nid_postfix(stream) != 0);
    clearerr_nid_postfix(stream);
    Require(fputs_nid_postfix(marker, stream) == EOF);
    Require(ferror_nid_postfix(stream) != 0);
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(argv[1], "wb");
    Require(stream != nullptr && fputs_nid_postfix(marker, stream) >= 0);
    ReplaceWithReadOnlyDescriptor(stream, argv[1]);
    Require(fflush_nid_postfix(stream) == EOF);
    Require(ferror_nid_postfix(stream) != 0);
    static_cast<void>(fclose_nid_postfix(stream));
    Require(std::remove(argv[1]) == 0);
    std::cout << "PASS: stream objects, file operations, EOF and error handling\n";
}
