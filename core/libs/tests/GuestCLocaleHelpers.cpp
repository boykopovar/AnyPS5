#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

struct Ctypevec {
    const short* table;
    std::int32_t deleteFlag;
};
struct Collvec {
    std::uint64_t handle;
    std::uint64_t page;
};
struct Cvtvec {
    std::uint32_t page;
    std::uint32_t maxBytes;
    std::uint8_t reserved[32];
};
static_assert(sizeof(Ctypevec) == 16 && offsetof(Ctypevec, deleteFlag) == 8);
static_assert(sizeof(Collvec) == 16);
static_assert(sizeof(Cvtvec) == 40 && offsetof(Cvtvec, reserved) == 8);

extern "C" {
const short* APS5_VABI _Getpctype_nid_postfix();
Ctypevec APS5_VABI _Getctype_nid_postfix();
Collvec APS5_VABI _Getcoll_nid_postfix();
Cvtvec APS5_VABI _Getcvt_nid_postfix();
int APS5_VABI _Getdateorder_nid_postfix();
int APS5_VABI _Strcoll_nid_postfix(const char*, const char*, const char*, const char*, const void*);
std::size_t APS5_VABI _Strxfrm_nid_postfix(char*, char*, const char*, const char*, const void*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "C locale helper check failed at line %u\n", location.line());
        std::abort();
    }
}

static int Collate(const char* first, std::size_t firstLength, const char* second, std::size_t secondLength) {
    return _Strcoll_nid_postfix(first, first + firstLength, second, second + secondLength, nullptr);
}

static int Collate(const char* first, const char* second) {
    return Collate(first, std::strlen(first), second, std::strlen(second));
}

static void CheckCollation() {
    Require(Collate("abc", "abc") == 0);
    Require(Collate("", "") == 0);
    Require(Collate("a", "b") < 0);
    Require(Collate("b", "a") > 0);
    Require(Collate("a", "c") == -2);
    Require(Collate("c", "a") == 2);
    Require(Collate("ab", "abc") == -1);
    Require(Collate("abc", "ab") == 1);
    Require(Collate("", "a") == -1);
    Require(Collate("a", "") == 1);
    Require(Collate("abc", "abd") == -1);
    Require(Collate("Zebra", "apple") < 0);
    Require(Collate("\x80", "\x7f") == 1);
    Require(Collate("\x7f", "\x80") == -1);
    Require(Collate("\xff", "\x01") == 254);
    Require(Collate("\x01", "\xff") == -254);
    Require(Collate("a\xff", "a\x80") == 0x7f);
    Require(Collate("aaaaaaaaaaaaaaaab", "aaaaaaaaaaaaaaaac") == -1);
    Require(Collate("aaaaaaaaaaaaaaaaz", "aaaaaaaaaaaaaaaaa") == 25);

    const char first[] = {'a', 0, 'b'};
    const char second[] = {'a', 0, 'c'};
    Require(Collate(first, 3, second, 3) == -1);
    Require(Collate(first, 2, second, 3) == -1);
    Require(Collate(first, 2, second, 2) == 0);
    Require(Collate(first, 3, second, 2) == 1);

    const char buffer[] = "abc";
    Require(_Strcoll_nid_postfix(buffer, buffer + 3, buffer, buffer + 3, nullptr) == 0);
    Require(_Strcoll_nid_postfix(buffer, buffer + 2, buffer + 1, buffer + 3, nullptr) == -1);
}

static void CheckTransform() {
    const char source[] = "hello";
    char buffer[16];

    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer + 16, source, source + 5, nullptr) == 5);
    Require(std::memcmp(buffer, "hello", 5) == 0);
    for (int i = 5; i < 16; ++i) Require(buffer[i] == 0x5a);

    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer + 5, source, source + 5, nullptr) == 5);
    Require(std::memcmp(buffer, "hello", 5) == 0);
    Require(buffer[5] == 0x5a);

    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer + 3, source, source + 5, nullptr) == 5);
    for (int i = 0; i < 16; ++i) Require(buffer[i] == 0x5a);

    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer, source, source + 5, nullptr) == 5);
    for (int i = 0; i < 16; ++i) Require(buffer[i] == 0x5a);

    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer + 16, source, source, nullptr) == 0);
    for (int i = 0; i < 16; ++i) Require(buffer[i] == 0x5a);

    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer, source, source, nullptr) == 0);
    for (int i = 0; i < 16; ++i) Require(buffer[i] == 0x5a);

    const char binary[] = {'\0', '\x80', '\xff', 'a'};
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(_Strxfrm_nid_postfix(buffer, buffer + 16, binary, binary + 4, nullptr) == 4);
    Require(std::memcmp(buffer, binary, 4) == 0);
}

static short Classify(int value) {
    short mask = 0;
    if (value < 32 || value == 127) mask |= 0x80;
    if (value == ' ') mask |= 0x04;
    if (value >= '\t' && value <= '\r') mask |= 0x40;
    if (value >= 'A' && value <= 'Z') mask |= 0x02;
    if (value >= 'a' && value <= 'z') mask |= 0x10;
    if (value >= '0' && value <= '9') mask |= 0x20;
    if ((value >= '0' && value <= '9') || (value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f')) mask |= 0x01;
    if (value >= 33 && value <= 126 && (mask & 0x232) == 0) mask |= 0x08;
    return mask;
}

static void CheckCtype() {
    const Ctypevec vec = _Getctype_nid_postfix();
    Require(vec.table == _Getpctype_nid_postfix());
    Require(vec.deleteFlag == 0);
    Require(vec.table[-1] == 0);
    for (int value = 0; value < 256; ++value) Require(vec.table[value] == Classify(value));
    Require(vec.table['A'] == 0x03 && vec.table['a'] == 0x11);
    Require(vec.table['G'] == 0x02 && vec.table['g'] == 0x10);
    Require(vec.table['0'] == 0x21 && vec.table['9'] == 0x21);
    Require(vec.table[' '] == 0x04 && vec.table['\t'] == 0xc0 && vec.table['\n'] == 0xc0);
    Require(vec.table['!'] == 0x08 && vec.table['~'] == 0x08);
    Require(vec.table[0] == 0x80 && vec.table[127] == 0x80 && vec.table[128] == 0 && vec.table[255] == 0);
    Require(_Getctype_nid_postfix().table == vec.table);
}

static void CheckCollvec() {
    const Collvec vec = _Getcoll_nid_postfix();
    Require(vec.handle == 0 && vec.page == 0);
}

static void CheckCvt() {
    const Cvtvec vec = _Getcvt_nid_postfix();
    Require(vec.page == 1);
    Require(vec.maxBytes == 1);
    for (const auto byte : vec.reserved) Require(byte == 0);

    unsigned char raw[sizeof(Cvtvec) + 8];
    std::memset(raw, 0x5a, sizeof(raw));
    const Cvtvec again = _Getcvt_nid_postfix();
    std::memcpy(raw, &again, sizeof(again));
    Require(std::memcmp(raw, &vec, sizeof(vec)) == 0);
    for (std::size_t i = sizeof(Cvtvec); i < sizeof(raw); ++i) Require(raw[i] == 0x5a);
}

static void CheckDateOrder() {
    Require(_Getdateorder_nid_postfix() == 2);
}

int main() {
    CheckCollation();
    CheckTransform();
    CheckCtype();
    CheckCollvec();
    CheckCvt();
    CheckDateOrder();
    return 0;
}
