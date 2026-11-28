#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestGetbsize check failed\n", stderr);
        std::abort();
    }
}

static void SetBlockSize(const char* value) {
    if (value == nullptr) unsetenv_nid_postfix("BLOCKSIZE");
    else setenv_nid_postfix("BLOCKSIZE", value, 1);
}

static void Check(const char* value, const char* header, std::int64_t blocksize) {
    SetBlockSize(value);
    int headerLength = -1;
    std::int64_t size = -1;
    const char* text = getbsize_nid_postfix(&headerLength, &size);
    Require(std::strcmp(text, header) == 0);
    Require(headerLength == static_cast<int>(std::strlen(header)));
    Require(size == blocksize);
}

int main() {
    Check(nullptr, "512-blocks", 512);
    Check("", "512-blocks", 512);
    Check("1K", "1K-blocks", 1024);
    Check("2k", "2K-blocks", 2048);
    Check("3M", "3M-blocks", 3 * 1024 * 1024);
    Check("1g", "1G-blocks", 1024LL * 1024 * 1024);
    Check("1024", "1024-blocks", 1024);
    Check("100", "512-blocks", 512);
    Check("0", "512-blocks", 512);
    Check("-1", "512-blocks", 512);
    Check("2x", "512-blocks", 512);
    Check("5000000000G", "1G-blocks", 1024LL * 1024 * 1024);
    Check("1K2", "512-blocks", 512);

    bool threw = false;
    try {
        int headerLength = 0;
        getbsize_nid_postfix(&headerLength, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    SetBlockSize(nullptr);
    return 0;
}
