#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
char* APS5_VABI basename_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
std::size_t APS5_VABI strnlen_nid_postfix(const char*, std::size_t);
char* APS5_VABI strncat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI strpbrk_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strcspn_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strlcat_nid_postfix(char*, const char*, std::size_t);
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
}

static void Require(bool condition) {
    if (!condition) {
        std::fputs("Guest string check failed\n", stderr);
        std::abort();
    }
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
    char truncated[] = "abXX";
    Require(strlcat_nid_postfix(truncated, "cd", 2) == 4);
    Require(std::strcmp(truncated, "abXX") == 0);
    char buffer[8] = "ab";
    Require(strlcat_nid_postfix(buffer, "cdefgh", sizeof(buffer)) == 8);
    Require(std::strcmp(buffer, "abcdefg") == 0);
    Require(strlcat_nid_postfix(buffer, "xyz", 0) == 3);
    buffer[0] = '\0';
    Require(strlcat_nid_postfix(buffer, "x", 1) == 1 && buffer[0] == '\0');
    Require(strncat_nid_postfix(buffer, "xyz", 2) == buffer);
    Require(std::strcmp(buffer, "xy") == 0);
    Require(strpbrk_nid_postfix(buffer, "ay") == buffer + 1);
    Require(strpbrk_nid_postfix(buffer, "") == nullptr);
    Require(strcspn_nid_postfix(buffer, "y") == 1);
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
}
