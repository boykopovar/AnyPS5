#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
int APS5_VABI memcpy_s_nid_postfix(void*, std::size_t, const void*, std::size_t);
int APS5_VABI wmemcpy_s_nid_postfix(std::uint16_t*, std::size_t, const std::uint16_t*, std::size_t);
int APS5_VABI strcpy_s_nid_postfix(char*, std::size_t, const char*);
int APS5_VABI strncpy_s_nid_postfix(char*, std::size_t, const char*, std::size_t);
int APS5_VABI fopen_s_nid_postfix(void**, const char*, const char*);
int APS5_VABI fclose_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestErange = 34;
constexpr int GuestEexist = 17;
constexpr int GuestEisdir = 21;
constexpr int GuestEnoent = 2;
constexpr std::size_t RsizeMax = 0x1fffffffffffffff;
constexpr unsigned char Poison = 0x5a;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Annex K size limit check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

std::string Hex(const void* bytes, std::size_t count) {
    const auto* values = static_cast<const unsigned char*>(bytes);
    static const char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t index = 0; index < count; ++index) {
        text.push_back(digits[values[index] >> 4]);
        text.push_back(digits[values[index] & 0x0f]);
    }
    return text;
}

void CheckMemcpyS() {
    unsigned char buffer[64];
    const unsigned char source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const std::size_t accepted[] = {0x40, 0x1000, 0x100000, 0x40000000, 0x7fffffff, 0x80000000, 0xffffffff,
        std::size_t{1} << 32, std::size_t{1} << 40, RsizeMax};
    for (const auto size : accepted) {
        std::memset(buffer, Poison, sizeof(buffer));
        const int error = memcpy_s_nid_postfix(buffer, size, source, 1);
        Require(error == 0 && buffer[0] == 1);
    }
    const std::size_t refused[] = {0x2000000000000000, 0x4000000000000000, 0x7ffffffffffffffe, 0x7fffffffffffffff,
        0x8000000000000000, SIZE_MAX};
    for (const auto size : refused) {
        std::memset(buffer, Poison, sizeof(buffer));
        const int error = memcpy_s_nid_postfix(buffer, size, source, 1);
        Require(error == GuestEinval && buffer[0] != 1);
    }
    std::memset(buffer, Poison, sizeof(buffer));
    Require(memcpy_s_nid_postfix(buffer, 4, source, 8) == GuestErange && Hex(buffer, 8) == "000000005a5a5a5a");
    std::memset(buffer, Poison, sizeof(buffer));
    Require(memcpy_s_nid_postfix(buffer, 64, buffer + 4, 16) == GuestEinval && Hex(buffer, 8) == "0000000000000000");
    Require(memcpy_s_nid_postfix(nullptr, 8, source, 1) == GuestEinval);
    std::memset(buffer, Poison, sizeof(buffer));
    Require(memcpy_s_nid_postfix(buffer, 8, nullptr, 4) == GuestEinval && Hex(buffer, 8) == "0000000000000000");
    Require(memcpy_s_nid_postfix(buffer, 8, nullptr, 0) == GuestEinval);
}

void CheckWmemcpyS() {
    std::uint16_t buffer[32];
    const std::uint16_t source[4] = {1, 2, 3, 4};
    const std::size_t accepted[] = {0x20, 0x100000, 0x7fffffff, 0x80000000, std::size_t{1} << 32, RsizeMax};
    for (const auto size : accepted) {
        std::memset(buffer, 0xaa, sizeof(buffer));
        const int error = wmemcpy_s_nid_postfix(buffer, size, source, 1);
        Require(error == 0 && buffer[0] == 1);
    }
    const std::size_t refused[] = {0x2000000000000000, 0x3ffffffffffffffe, 0x3fffffffffffffff, 0x4000000000000000,
        0x7ffffffffffffffe, 0x7fffffffffffffff, 0x8000000000000000, SIZE_MAX};
    for (const auto size : refused) {
        std::memset(buffer, 0xaa, sizeof(buffer));
        const int error = wmemcpy_s_nid_postfix(buffer, size, source, 1);
        Require(error == GuestEinval && buffer[0] != 1);
    }
}

void CheckStrcpyS() {
    char buffer[64];
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strcpy_s_nid_postfix(buffer, 4, "abcdef") == GuestErange && Hex(buffer, 8) == "005a5a5a5a5a5a5a");
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strcpy_s_nid_postfix(buffer, 0, "abc") == GuestEinval && Hex(buffer, 4) == "5a5a5a5a");
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strcpy_s_nid_postfix(buffer, SIZE_MAX, "abc") == GuestEinval && Hex(buffer, 4) == "5a5a5a5a");
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strcpy_s_nid_postfix(buffer, 8, nullptr) == GuestEinval && Hex(buffer, 4) == "005a5a5a");
}

void CheckStrncpyS() {
    char buffer[64];
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strncpy_s_nid_postfix(buffer, 4, "abcdef", 3) == 0 && Hex(buffer, 8) == "616263005a5a5a5a");
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strncpy_s_nid_postfix(buffer, 4, "abcdef", 4) == GuestErange && Hex(buffer, 8) == "005a5a5a5a5a5a5a");
    std::memset(buffer, Poison, sizeof(buffer));
    Require(strncpy_s_nid_postfix(buffer, 4, "abcdef", SIZE_MAX) == GuestEinval && Hex(buffer, 8) == "005a5a5a5a5a5a5a");
}

void CheckFopenS() {
    const std::string root = "guest_libc_annexk_rsizemax";
    const std::string existing = root + "/exists.txt";
    const std::string missing = root + "/missing.txt";
    const std::string sub = root + "/sub";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(sub);
    {
        std::FILE* handle = std::fopen(existing.c_str(), "wb");
        if (handle) std::fclose(handle);
    }
    void* stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    *__error_nid_postfix() = 0;
    Require(fopen_s_nid_postfix(&stream, missing.c_str(), "r") == GuestEnoent && stream == nullptr);
    Require(*__error_nid_postfix() == GuestEnoent);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, existing.c_str(), "r") == 0 && stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, existing.c_str(), "wx") == GuestEexist && stream == nullptr);
    Require(*__error_nid_postfix() == GuestEexist);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, existing.c_str(), "z") == GuestEinval && stream == nullptr);
    Require(*__error_nid_postfix() == GuestEinval);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, existing.c_str(), "") == GuestEinval && stream == nullptr);
    Require(*__error_nid_postfix() == GuestEinval);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, sub.c_str(), "w") == GuestEisdir && stream == nullptr);
    Require(*__error_nid_postfix() == GuestEisdir);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, nullptr, "r") == GuestEinval && stream == nullptr);
    stream = reinterpret_cast<void*>(0x5a5a5a5a5a5a5a5aull);
    Require(fopen_s_nid_postfix(&stream, existing.c_str(), nullptr) == GuestEinval && stream == nullptr);
    Require(fopen_s_nid_postfix(nullptr, existing.c_str(), "r") == GuestEinval);
    std::error_code error;
    std::filesystem::remove_all(root, error);
}

}

int main() {
    CheckMemcpyS();
    CheckWmemcpyS();
    CheckStrcpyS();
    CheckStrncpyS();
    CheckFopenS();
    return 0;
}
