#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode);
FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream);
int APS5_VABI fclose_nid_postfix(FileStream* stream);
std::size_t APS5_VABI fread_nid_postfix(void* buffer, std::size_t size, std::size_t count, FileStream* stream);
std::size_t APS5_VABI fwrite_nid_postfix(const void* buffer, std::size_t size, std::size_t count, FileStream* stream);
int APS5_VABI fseek_nid_postfix(FileStream* stream, std::int64_t offset, int origin);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream* stream);
int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream);
int APS5_VABI fflush_nid_postfix(FileStream* stream);
}

static void Require(bool condition) {
    if (!condition) throw std::runtime_error("File stream check failed");
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

int main(int argc, char** argv) {
    Require(argc == 2);
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
    Require(fseek_nid_postfix(stream, 0, -1) == -1);
    Require(errno == EINVAL);
    Require(fclose_nid_postfix(stream) == 0);
    ExpectException([] { fputs_nid_postfix("invalid stream", nullptr); });
    ExpectException([] { fopen_nid_postfix(nullptr, "r"); });
    FileStream closed(std::tmpfile());
    closed.Close();
    ExpectException([&] { fflush_nid_postfix(&closed); });
    Require(std::remove(argv[1]) == 0);
    errno = 0;
    Require(fopen_nid_postfix(argv[1], "rb") == nullptr);
    Require(errno == ENOENT);
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
    std::cout << "PASS: stream objects, file operations, EOF and error handling\n";
}
