#include "prx/libc/include/FileStream.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

extern "C" {
FileStream* fopen_nid_postfix(const char* filename, const char* mode);
int fclose_nid_postfix(FileStream* stream);
std::size_t fread_nid_postfix(void* buffer, std::size_t size, std::size_t count, FileStream* stream);
std::size_t fwrite_nid_postfix(const void* buffer, std::size_t size, std::size_t count, FileStream* stream);
int fseek_nid_postfix(FileStream* stream, long offset, int origin);
long ftell_nid_postfix(FileStream* stream);
int fputs_nid_postfix(const char* str, FileStream* stream);
int fflush_nid_postfix(FileStream* stream);
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
    ExpectException([&] { fseek_nid_postfix(stream, 0, -1); });
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(argv[1], "rb");
    ExpectException([&] { fwrite_nid_postfix(payload, 1, sizeof(payload), stream); });
    Require(fclose_nid_postfix(stream) == 0);
    ExpectException([] { fputs_nid_postfix("invalid stream", nullptr); });
    ExpectException([] { fopen_nid_postfix(nullptr, "r"); });
    FileStream closed(std::tmpfile());
    closed.Close();
    ExpectException([&] { fflush_nid_postfix(&closed); });
    Require(std::remove(argv[1]) == 0);
    ExpectException([&] { fopen_nid_postfix(argv[1], "rb"); });
    std::cout << "PASS: stream objects, file operations, EOF and error handling\n";
}
