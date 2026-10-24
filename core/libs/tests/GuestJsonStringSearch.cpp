#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>

struct SearchText { char* characters; std::uint32_t length; std::uint32_t capacity; std::uint64_t reserved; };
struct String { SearchText* text; };
extern "C" {
char APS5_VABI _ZNK3sce4Json6String2atEm(const String*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcm(const String*, const char*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcmm(const String*, const char*, std::size_t, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findERKS1_m(const String*, const String*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcm(const String*, const char*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcmm(const String*, const char*, std::size_t, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindERKS1_m(const String*, const String*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const String*, char, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const String*, char, std::size_t);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
int main() {
    constexpr auto none = std::numeric_limits<std::size_t>::max();
    char bytes[]{'a', 'b', 'X', '\0', 'X', static_cast<char>(0xff), '\0'};
    SearchText header{bytes, 6, 7, UINT64_C(0x123456789abcdef0)};
    String text{&header}, missing{};
    char needleBytes[]{'X', '\0', 'q', '\0'};
    SearchText needleHeader{needleBytes, 3, 4, 0};
    String needle{&needleHeader};
    const auto before = header;
    char snapshot[sizeof(bytes)]; std::memcpy(snapshot, bytes, sizeof(bytes));
    for (std::size_t i = 0; i < sizeof(bytes); ++i) {
        Require(_ZNK3sce4Json6String2atEm(&text, i) == bytes[i], "String at must return the indexed byte or zero at the boundary");
    }
    Require(_ZNK3sce4Json6String2atEm(&text, none) == 0 && _ZNK3sce4Json6String2atEm(&missing, 0) == 0, "Out-of-range and missing-storage access must return zero");
    Require(_ZNK3sce4Json6String4findEPKcm(&text, "X", 0) == 2 && _ZNK3sce4Json6String4findEPKcm(&text, "X", 3) == none && _ZNK3sce4Json6String4findEPKcm(&text, "X", 4) == 4, "Forward searches must stop at an embedded zero and honor the starting position");
    Require(_ZNK3sce4Json6String5rfindEPKcm(&text, "X", none) == 4 && _ZNK3sce4Json6String5rfindEPKcm(&text, "X", 3) == 2, "Reverse searches must scan bytes across embedded zeros");
    Require(_ZNK3sce4Json6String4findERKS1_m(&text, &needle, 0) == 2 && _ZNK3sce4Json6String5rfindERKS1_m(&text, &needle, none) == 4, "String needles must use their C-string prefix rather than their stored length");
    Require(_ZNK3sce4Json6String4findEPKcmm(&text, needleBytes, 0, 3) == 2 && _ZNK3sce4Json6String5rfindEPKcmm(&text, needleBytes, none, 3) == 2, "Counted matching must stop comparing at a shared zero");
    for (std::size_t position : {std::size_t{0}, std::size_t{6}, std::size_t{7}, none}) {
        const auto forward = position <= 6 ? position : none;
        const auto reverse = position < 6 ? position : 6;
        Require(_ZNK3sce4Json6String4findEPKcm(&text, nullptr, position) == forward && _ZNK3sce4Json6String4findEPKcmm(&text, nullptr, position, 0) == forward && _ZNK3sce4Json6String4findERKS1_m(&text, &missing, position) == forward, "Empty forward needles must validate the starting position");
        Require(_ZNK3sce4Json6String5rfindEPKcm(&text, nullptr, position) == reverse && _ZNK3sce4Json6String5rfindEPKcmm(&text, nullptr, position, 0) == reverse && _ZNK3sce4Json6String5rfindERKS1_m(&text, &missing, position) == reverse, "Empty reverse needles must clamp the starting position");
    }
    Require(_ZNK3sce4Json6String4findEPKcm(&missing, "", 0) == none && _ZNK3sce4Json6String5rfindEPKcm(&missing, "", 0) == none, "Missing haystack storage must reject even empty needles");
    Require(_ZNK3sce4Json6String4findEPKcmm(&text, nullptr, 0, none) == none && _ZNK3sce4Json6String5rfindEPKcmm(&text, nullptr, none, none) == none, "Oversized counts must be rejected before touching the needle");
    Require(_ZNK3sce4Json6String4findEcm(&text, '\0', 0) == 3 && _ZNK3sce4Json6String4findEcm(&text, 'X', 3) == none && _ZNK3sce4Json6String5rfindEcm(&text, '\0', none) == 3, "Character search must preserve forward C-string and reverse byte semantics");
    Require(_ZNK3sce4Json6String4findEcm(&text, static_cast<char>(0xff), 4) == 5 && _ZNK3sce4Json6String5rfindEcm(&text, static_cast<char>(0xff), none) == 5, "Character search must preserve high-bit byte values");
    char tailBytes[]{'a', 'b', 'X', '\0'};
    SearchText tailHeader{tailBytes, 3, 4, 0}; String tail{&tailHeader};
    Require(_ZNK3sce4Json6String4findEPKcmm(&tail, "X", 0, 2) == 2, "Forward candidates must retain native zero-terminated comparison beyond the initial length check");
    Require(_ZNK3sce4Json6String4findEcm(&tail, '\0', 0) == 3 && _ZNK3sce4Json6String4findEcm(&tail, '\0', 3) == none && _ZNK3sce4Json6String5rfindEcm(&tail, '\0', none) == none, "Only forward character search may include the trailing terminator after an admitted start");
    Require(std::memcmp(&header, &before, sizeof(header)) == 0 && std::memcmp(bytes, snapshot, sizeof(bytes)) == 0 && text.text == &header, "Read-only string operations must preserve all storage and metadata");
    return 0;
}
