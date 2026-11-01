#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
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
    Require(fclose_nid_postfix(stream) == 0);

    std::ifstream in(filePath, std::ios::binary);
    Require(in.is_open());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    Require(contents == std::string(message, sizeof(message)));
    std::filesystem::remove(filePath);

    FileStream closedStream{};
    *__error_nid_postfix() = 0;
    Require(__fflush_nid_postfix(&closedStream) == -1);
    Require(*__error_nid_postfix() == 9);

    auto* secondStream = fopen_nid_postfix(filePath.c_str(), "w+b");
    Require(secondStream != nullptr);
    Require(fwrite_nid_postfix("sample", 1, 6, secondStream) == 6);
    Require(__fflush_nid_postfix(secondStream) == 0);
    Require(fclose_nid_postfix(secondStream) == 0);
    std::filesystem::remove(filePath);

    return 0;
}
