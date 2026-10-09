#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
char* APS5_VABI basename_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
std::size_t APS5_VABI strnlen_nid_postfix(const char*, std::size_t);
std::size_t APS5_VABI strnlen_s_nid_postfix(const char*, std::size_t);
char* APS5_VABI strncat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI strpbrk_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strcspn_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strlcat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI stpcpy_nid_postfix(char*, const char*);
char* APS5_VABI strtok_r_nid_postfix(char*, const char*, char**);
char* APS5_VABI strtok_nid_postfix(char*, const char*);
char* APS5_VABI strcasestr_nid_postfix(const char*, const char*);
int APS5_VABI strcpy_s_nid_postfix(char*, std::size_t, const char*);
int APS5_VABI strcat_s_nid_postfix(char*, std::size_t, const char*);
int APS5_VABI strncat_s_nid_postfix(char*, std::size_t, const char*, std::size_t);
int APS5_VABI memcpy_s_nid_postfix(void*, std::size_t, const void*, std::size_t);
int APS5_VABI memmove_s_nid_postfix(void*, std::size_t, const void*, std::size_t);
int APS5_VABI memset_s_nid_postfix(void*, std::size_t, int, std::size_t);
char* APS5_VABI strnstr_nid_postfix(const char*, const char*, std::size_t);
int APS5_VABI snprintf_s_nid_postfix(char*, std::size_t, const char*, ...);
int APS5_VABI sscanf_s_nid_postfix(const char*, const char*, ...);
int APS5_VABI __inet_aton_nid_postfix(const char*, void*);
}

static void Require(bool condition) {
    if (!condition) {
        std::fputs("Guest string check failed\n", stderr);
        std::abort();
    }
}

static bool CheckMemcpyOverlap() {
    bool correct = true;
    for (const auto offset : {0, 1, -1, 3, -3}) {
        unsigned char bytes[16];
        std::memset(bytes, 0x5a, sizeof(bytes));
        auto* destination = bytes + 4;
        const auto* source = destination + offset;
        const int error = memcpy_s_nid_postfix(destination, 8, source, 4);
        bool cleared = true;
        for (unsigned i = 4; i < 12; ++i) cleared &= bytes[i] == 0;
        const bool matches = error == 22 && cleared && bytes[3] == 0x5a && bytes[12] == 0x5a;
        if (!matches) std::fprintf(stderr, "memcpy_s overlap %+d: expected EINVAL 22 and eight zero bytes, received %d\n", offset, error);
        correct &= matches;
    }
    unsigned char adjacent[] = {1, 2, 3, 4, 5, 6, 7, 8};
    Require(memcpy_s_nid_postfix(adjacent, 4, adjacent + 4, 4) == 0);
    Require(std::memcmp(adjacent, adjacent + 4, 4) == 0);
    const unsigned char original[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::memcpy(adjacent, original, sizeof(adjacent));
    Require(memcpy_s_nid_postfix(adjacent + 4, 4, adjacent, 4) == 0);
    const unsigned char unchanged[] = {1, 2, 3, 4, 1, 2, 3, 4};
    Require(std::memcmp(adjacent, unchanged, sizeof(adjacent)) == 0);
    Require(memcpy_s_nid_postfix(adjacent, sizeof(adjacent), adjacent, 0) == 0);
    Require(std::memcmp(adjacent, unchanged, sizeof(adjacent)) == 0);
    return correct;
}

static void CheckBoundsCheckedFunctions() {
    char small[4] = "zz";
    Require(strcpy_s_nid_postfix(small, sizeof(small), "abc") == 0 && std::strcmp(small, "abc") == 0);
    Require(strcpy_s_nid_postfix(small, sizeof(small), "abcd") == 34 && small[0] == '\0');
    Require(strcpy_s_nid_postfix(nullptr, 4, "a") == 22);
    char joined[8] = "ab";
    Require(strcat_s_nid_postfix(joined, sizeof(joined), "cd") == 0 && std::strcmp(joined, "abcd") == 0);
    Require(strncat_s_nid_postfix(joined, sizeof(joined), "efgh", 2) == 0 && std::strcmp(joined, "abcdef") == 0);
    Require(strcat_s_nid_postfix(joined, sizeof(joined), "gh") == 34 && joined[0] == '\0');
    char bytes[4] = {1, 2, 3, 4};
    const char source[4] = {5, 6, 7, 8};
    Require(memcpy_s_nid_postfix(bytes, sizeof(bytes), source, 2) == 0 && bytes[0] == 5 && bytes[2] == 3);
    Require(memcpy_s_nid_postfix(bytes, 2, source, 4) == 34 && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 3);
    char overlap[6] = "abcde";
    Require(memmove_s_nid_postfix(overlap + 1, 5, overlap, 3) == 0 && std::strcmp(overlap, "aabce") == 0);
    Require(memset_s_nid_postfix(bytes, sizeof(bytes), 9, 8) == 34 && bytes[3] == 9);
    Require(strnstr_nid_postfix("haystack", "st", 4) == nullptr);
    const char haystack[] = "haystack";
    Require(strnstr_nid_postfix(haystack, "st", 6) == haystack + 3);
    char formatted[8];
    Require(snprintf_s_nid_postfix(formatted, sizeof(formatted), "%d-%s", 42, "x") == 4 && std::strcmp(formatted, "42-x") == 0);
}

static void CheckSscanfS() {
#ifndef _WIN32
    int number = 0;
    char word[4] = "zz";
    char letter = 0;
    char value[8] = {};
    Require(sscanf_s_nid_postfix(" 12 abc x", "%d %s %c", &number, word, 4u, &letter, 1u) == 3 && number == 12 && std::strcmp(word, "abc") == 0 && letter == 'x');
    Require(sscanf_s_nid_postfix("12 abcd", "%d %s", &number, word, 4u) == 1 && word[0] == '\0');
    Require(sscanf_s_nid_postfix("key=val", "%3[a-z]=%3s", word, 4u, value, 8u) == 2 && std::strcmp(word, "key") == 0 && std::strcmp(value, "val") == 0);
    Require(sscanf_s_nid_postfix("abcdef", "%3s", word, 4u) == 1 && std::strcmp(word, "abc") == 0);
    Require(sscanf_s_nid_postfix("abcdef", "%3s", value, 8u) == 1 && std::strcmp(value, "abc") == 0);
    Require(sscanf_s_nid_postfix("abcdef", "%3[a-z]", value, 8u) == 1 && std::strcmp(value, "abc") == 0);
    Require(sscanf_s_nid_postfix("2024ABCD 7", "%4s%s", word, 4u, value, 8u) == 0 && word[0] == '\0');
    Require(sscanf_s_nid_postfix("2024ABCD", "%3s%4s", word, 4u, value, 8u) == 2 && std::strcmp(word, "202") == 0 && std::strcmp(value, "4ABC") == 0);
    int position = 0;
    Require(sscanf_s_nid_postfix("7 %", "%d %%%n", &number, &position) == 1 && position == 3);
    Require(sscanf_s_nid_postfix("   ", "%d", &number) == EOF);
    Require(sscanf_s_nid_postfix("x", "%d", &number) == 0);
#endif
}

int main() {
    CheckBoundsCheckedFunctions();
    CheckSscanfS();
    Require(std::strcmp(basename_nid_postfix(nullptr), ".") == 0);
    Require(std::strcmp(basename_nid_postfix(""), ".") == 0);
    Require(std::strcmp(basename_nid_postfix("////"), "/") == 0);
    const char path[] = "/one/two///";
    Require(std::strcmp(basename_nid_postfix(path), "two") == 0);
    Require(std::strcmp(path, "/one/two///") == 0);
    Require(std::strcmp(basename_nid_postfix("one\\two"), "one\\two") == 0);
    const std::string longName(1024, 'x');
    Require(basename_nid_postfix(longName.c_str()) == nullptr && *__error_nid_postfix() == 63);
    const char bounded[] = {'a', 'b', 'c'};
    Require(strnlen_nid_postfix(bounded, 0) == 0);
    Require(strnlen_nid_postfix(bounded, sizeof(bounded)) == 3);
    Require(strnlen_nid_postfix("a", 8) == 1);
    Require(strnlen_s_nid_postfix(nullptr, 8) == 0);
    Require(strnlen_s_nid_postfix("abc", 8) == 3 && strnlen_s_nid_postfix("abcdef", 4) == 4);
    char truncated[] = "abXX";
    Require(strlcat_nid_postfix(truncated, "cd", 2) == 4);
    Require(std::strcmp(truncated, "abXX") == 0);
    char buffer[8] = "ab";
    Require(strlcat_nid_postfix(buffer, "cdefgh", sizeof(buffer)) == 8);
    Require(std::strcmp(buffer, "abcdefg") == 0);
    Require(strlcat_nid_postfix(buffer, "xyz", 0) == 3);
    buffer[0] = '\0';
    Require(strlcat_nid_postfix(buffer, "x", 1) == 1 && buffer[0] == '\0');
    char chained[8] = "zzzzzzz";
    char* end = stpcpy_nid_postfix(chained, "ab");
    Require(end == chained + 2 && *end == '\0' && chained[3] == 'z');
    end = stpcpy_nid_postfix(end, "cd");
    Require(end == chained + 4 && std::strcmp(chained, "abcd") == 0);
    Require(stpcpy_nid_postfix(end, "") == end && chained[5] == 'z');
    Require(strncat_nid_postfix(buffer, "xyz", 2) == buffer);
    Require(std::strcmp(buffer, "xy") == 0);
    Require(strpbrk_nid_postfix(buffer, "ay") == buffer + 1);
    Require(strpbrk_nid_postfix(buffer, "") == nullptr);
    Require(strcspn_nid_postfix(buffer, "y") == 1);
    const auto inetAton = [](const char* text, const char* expected) {
        unsigned char address[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        if (__inet_aton_nid_postfix(text, address) != 1) return false;
        char formatted[16];
        std::snprintf(formatted, sizeof(formatted), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
        return std::strcmp(formatted, expected) == 0;
    };
    Require(inetAton("192.0.2.42", "192.0.2.42"));
    Require(inetAton("10.1.2", "10.1.0.2"));
    Require(inetAton("127.1", "127.0.0.1"));
    Require(inetAton("3232235777", "192.168.1.1"));
    Require(inetAton("0x7f.0.0.0x1", "127.0.0.1"));
    Require(inetAton("0377.0.0.010", "255.0.0.8"));
    Require(inetAton("1.2.3.4 trailing", "1.2.3.4"));
    Require(inetAton("1.2.3.4\n", "1.2.3.4"));
    Require(__inet_aton_nid_postfix("1.2.3.4", nullptr) == 1);
    for (const char* invalid : {"", " 1.2.3.4", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "1.2.65536", "08", "1..2", "a.b.c.d",
             "1.2.3.4x", "0x", "1.2.3.", "-1"}) {
        unsigned char address[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        Require(__inet_aton_nid_postfix(invalid, address) == 0);
        Require(address[0] == 0xA5 && address[3] == 0xA5);
    }
    char first[] = ",a,,b,";
    char second[] = "x:y";
    char* firstState = nullptr;
    char* secondState = nullptr;
    Require(std::strcmp(strtok_r_nid_postfix(first, ",", &firstState), "a") == 0);
    Require(std::strcmp(strtok_r_nid_postfix(second, ":", &secondState), "x") == 0);
    Require(std::strcmp(strtok_r_nid_postfix(nullptr, ",", &firstState), "b") == 0);
    Require(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr);
    Require(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr);
    Require(std::strcmp(strtok_r_nid_postfix(nullptr, "", &secondState), "y") == 0);
    char hostTokens[] = "host:next";
    Require(std::strcmp(std::strtok(hostTokens, ":"), "host") == 0);
    char guestTokens[] = ",one,,two:three";
    Require(std::strcmp(strtok_nid_postfix(guestTokens, ","), "one") == 0);
    Require(std::strcmp(strtok_nid_postfix(nullptr, ":,"), "two") == 0);
    Require(std::strcmp(strtok_nid_postfix(nullptr, ""), "three") == 0);
    Require(strtok_nid_postfix(nullptr, ",") == nullptr);
    Require(strtok_nid_postfix(nullptr, ",") == nullptr);
    Require(std::strcmp(std::strtok(nullptr, ":"), "next") == 0);
    const char text[] = "aABAbC";
    Require(strcasestr_nid_postfix(text, "ababc") == text + 1);
    Require(strcasestr_nid_postfix(text, "") == text);
    Require(strcasestr_nid_postfix(text, "abcdef") == nullptr);
    Require(strcasestr_nid_postfix("", "a") == nullptr);
    const char highBytes[] = {static_cast<char>(0xff), 'A', 0};
    Require(strcasestr_nid_postfix(highBytes, "a") == highBytes + 1);
    return CheckMemcpyOverlap() ? 0 : 1;
}
