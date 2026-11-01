#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest libc internal stream check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    Require(__fflush_nid_postfix(nullptr) == 0);

    const auto filePath = "guest_libc_internal_stream_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    auto* stream = fopen_nid_postfix(filePath.c_str(), "w+b");
    Require(stream != nullptr);
    const char message[] = "libc internal stream test payload";
    Require(fwrite_nid_postfix(message, 1, sizeof(message), stream) == sizeof(message));
    Require(__fflush_nid_postfix(stream) == 0);

    {
        std::ifstream in(filePath, std::ios::binary);
        Require(in.is_open());
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        Require(contents == std::string(message, sizeof(message)));
    }
    Require(fclose_nid_postfix(stream) == 0);
    std::filesystem::remove(filePath);

    FileStream closedStream(TemporaryFile::Open());
    closedStream.Close();
    *__error_nid_postfix() = 0;
    Require(__fflush_nid_postfix(&closedStream) == -1);
    Require(*__error_nid_postfix() == 9);

    const auto fileA = "guest_libc_internal_stream_a_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    const auto fileB = "guest_libc_internal_stream_b_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    auto* streamA = fopen_nid_postfix(fileA.c_str(), "w+b");
    auto* streamB = fopen_nid_postfix(fileB.c_str(), "w+b");
    Require(streamA != nullptr && streamB != nullptr);
    Require(fwrite_nid_postfix("streamA", 1, 7, streamA) == 7);
    Require(fwrite_nid_postfix("streamB", 1, 7, streamB) == 7);
    Require(__fflush_nid_postfix(nullptr) == 0);
    {
        std::ifstream inA(fileA, std::ios::binary);
        Require(inA.is_open());
        std::string contentsA((std::istreambuf_iterator<char>(inA)), std::istreambuf_iterator<char>());
        Require(contentsA == "streamA");

        std::ifstream inB(fileB, std::ios::binary);
        Require(inB.is_open());
        std::string contentsB((std::istreambuf_iterator<char>(inB)), std::istreambuf_iterator<char>());
        Require(contentsB == "streamB");
    }
    Require(fclose_nid_postfix(streamA) == 0);
    Require(fclose_nid_postfix(streamB) == 0);
    std::filesystem::remove(fileA);
    std::filesystem::remove(fileB);

    return 0;
}
