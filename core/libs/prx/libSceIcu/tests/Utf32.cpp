#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __linux__
#include <sys/resource.h>
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t Illegal = 1;
constexpr std::int32_t Invalid = 10;
constexpr std::int32_t Overflow = 15;
constexpr std::int32_t NotTerminated = -124;
constexpr char16_t Fill = 0xa55a;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTF-32 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

struct FromResult {
    std::u16string text;
    std::int32_t length = -77;
    std::int32_t substitutions = -77;
    std::int32_t status = 0;
    bool null = false;
    char16_t after = 0;
};

FromResult From(const std::vector<std::int32_t>& source, std::int32_t sourceLength, std::int32_t capacity,
                std::int32_t substitution, std::int32_t status = 0, bool withSub = true) {
    std::array<char16_t, 16> buffer;
    buffer.fill(Fill);
    FromResult result;
    result.status = status;
    char16_t* returned = withSub
        ? u_strFromUTF32WithSub_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, substitution, &result.substitutions, &result.status)
        : u_strFromUTF32_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, &result.status);
    result.null = returned == nullptr;
    if (returned) Require(returned == buffer.data());
    const auto size = capacity < 0 ? 0 : std::min<std::size_t>(capacity, buffer.size());
    result.text.assign(buffer.data(), size);
    result.after = size < buffer.size() ? buffer[size] : Fill;
    return result;
}

void Valid() {
    const std::vector<std::int32_t> mixed{0x41, 0x1f600, 0xe000, 0xffff};
    auto result = From(mixed, 4, 8, -1);
    Require(!result.null && result.status == 0 && result.length == 5 && result.substitutions == 0);
    Require(result.text.substr(0, 6) == std::u16string({0x41, 0xd83d, 0xde00, 0xe000, 0xffff, 0}));
    Require(result.text[6] == Fill);

    result = From({0x41, 0x1f600, 0, 0x42}, -1, 8, -1);
    Require(result.status == 0 && result.length == 3 && result.text.substr(0, 4) == std::u16string({0x41, 0xd83d, 0xde00, 0}));

    result = From({0x10000, 0x10ffff, 0x7f, 0x80}, 4, 8, -1);
    Require(result.length == 6 && result.text.substr(0, 7) == std::u16string({0xd800, 0xdc00, 0xdbff, 0xdfff, 0x7f, 0x80, 0}));

    result = From({}, 0, 4, -1);
    Require(!result.null && result.status == 0 && result.length == 0 && result.text[0] == 0 && result.text[1] == Fill);
    result = From({0}, -1, 1, -1);
    Require(result.status == 0 && result.length == 0 && result.text[0] == 0);

    result = From(mixed, 4, 8, -1, 0, false);
    Require(result.status == 0 && result.length == 5 && result.substitutions == -77);
}

void Capacity() {
    const std::vector<std::int32_t> mixed{0x41, 0x1f600, 0xe000, 0xffff};
    auto result = From(mixed, 4, 5, -1);
    Require(result.status == NotTerminated && result.length == 5 && result.after == Fill);
    Require(result.text == std::u16string({0x41, 0xd83d, 0xde00, 0xe000, 0xffff}));

    result = From(mixed, 4, 4, -1);
    Require(result.status == Overflow && result.length == 5);
    Require(result.text == std::u16string({0x41, 0xd83d, 0xde00, 0xe000}));

    result = From(mixed, 4, 2, -1);
    Require(result.status == Overflow && result.length == 5 && result.text == std::u16string({0x41, 0xe000}));

    result = From(mixed, 4, 0, -1);
    Require(result.status == Overflow && result.length == 5 && result.after == Fill);

    result = From({0x1f600}, 1, 1, -1);
    Require(result.status == Overflow && result.length == 2 && result.text == std::u16string({Fill}));

    result = From({0x41, 0}, -1, 1, -1);
    Require(result.status == NotTerminated && result.length == 1 && result.text == std::u16string({0x41}));

    std::int32_t length = -77;
    std::int32_t status = 0;
    const std::vector<std::int32_t> text{0x61, 0x1f600};
    Require(u_strFromUTF32_nid_postfix(nullptr, 0, &length, text.data(), 2, &status) == nullptr);
    Require(status == Overflow && length == 3);
    status = 0;
    Require(u_strFromUTF32_nid_postfix(nullptr, 0, nullptr, nullptr, 0, &status) == nullptr);
    Require(status == NotTerminated);
}

void Substitution() {
    auto result = From({0xd800, 0x41, 0x110000, -5, 0xdfff}, 5, 8, 0xfffd);
    Require(result.status == 0 && result.length == 5 && result.substitutions == 4);
    Require(result.text.substr(0, 6) == std::u16string({0xfffd, 0x41, 0xfffd, 0xfffd, 0xfffd, 0}));

    result = From({0xdfff, 0x41}, 2, 8, 0x1f600);
    Require(result.status == 0 && result.length == 3 && result.substitutions == 1);
    Require(result.text.substr(0, 4) == std::u16string({0xd83d, 0xde00, 0x41, 0}));

    result = From({0xd800}, 1, 1, 0x1f600);
    Require(result.status == Overflow && result.length == 2 && result.substitutions == 1 && result.text == std::u16string({Fill}));

    result = From({0xd800, 0x0}, -1, 4, 0x3f);
    Require(result.status == 0 && result.length == 1 && result.substitutions == 1 && result.text.substr(0, 2) == std::u16string({0x3f, 0}));

    result = From({0x41, 0xd800, 0x42}, 3, 8, -1);
    Require(result.null && result.status == Invalid && result.length == -77 && result.substitutions == 0);
    Require(result.text[0] == 0x41 && result.text[1] == Fill);

    result = From({0x41, 0xd800}, 2, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
    result = From({0x110000}, 1, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
    result = From({-1}, 1, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
}

void Arguments() {
    const std::vector<std::int32_t> text{0x41};
    for (auto substitution : {0x110000, 0xd800, 0xdfff, 0x7fffffff}) {
        const auto result = From(text, 1, 8, substitution);
        Require(result.null && result.status == Illegal && result.length == -77 && result.substitutions == -77 && result.text[0] == Fill);
    }
    for (auto length : {-2, std::numeric_limits<std::int32_t>::min()}) {
        const auto result = From(text, length, 8, -1);
        Require(result.null && result.status == Illegal && result.length == -77);
    }
    Require(From(text, 1, -1, -1).status == Illegal);
    std::int32_t status = 0;
    std::int32_t length = -77;
    std::array<char16_t, 4> buffer{};
    Require(!u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, nullptr, 1, &status) && status == Illegal);
    status = 0;
    Require(!u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, nullptr, -1, &status) && status == Illegal);
    status = 0;
    Require(!u_strFromUTF32_nid_postfix(nullptr, 1, &length, text.data(), 1, &status) && status == Illegal);
    Require(length == -77);
}

void Status() {
    auto result = From({0x41}, 1, 4, -1, Illegal);
    Require(result.null && result.status == Illegal && result.text[0] == Fill && result.length == -77);
    result = From({0x41}, 1, 4, -1, Invalid);
    Require(result.null && result.status == Invalid);

    result = From({0x41}, 1, 4, -1, NotTerminated);
    Require(!result.null && result.status == 0 && result.text.substr(0, 2) == std::u16string({0x41, 0}));
    result = From({0x41}, 1, 1, -1, NotTerminated);
    Require(result.status == NotTerminated && result.length == 1);
    result = From({0x41, 0x42}, 2, 1, -1, NotTerminated);
    Require(result.status == Overflow);
    result = From({0x41}, 1, 4, -1, -5);
    Require(result.status == -5 && result.text.substr(0, 2) == std::u16string({0x41, 0}));

    const std::vector<std::int32_t> text{0x41};
    std::array<char16_t, 4> buffer{};
    std::int32_t length = 0;
    RequireThrows([&] { u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, text.data(), 1, nullptr); });
    RequireThrows([&] { u_strFromUTF32WithSub_nid_postfix(buffer.data(), 4, &length, text.data(), 1, -1, nullptr, nullptr); });
}

struct ToResult {
    std::vector<std::int32_t> values;
    std::int32_t length = -77;
    std::int32_t substitutions = -77;
    std::int32_t status = 0;
    bool null = false;
    std::int32_t after = 0;
};

constexpr std::int32_t Fill32 = 0x5a5aa5a5;

ToResult To(const std::u16string& source, std::int32_t sourceLength, std::int32_t capacity,
            std::int32_t substitution, std::int32_t status = 0, bool withSub = true) {
    std::array<std::int32_t, 16> buffer;
    buffer.fill(Fill32);
    ToResult result;
    result.status = status;
    std::int32_t* returned = withSub
        ? u_strToUTF32WithSub_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, substitution, &result.substitutions, &result.status)
        : u_strToUTF32_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, &result.status);
    result.null = returned == nullptr;
    if (returned) Require(returned == buffer.data());
    const auto size = capacity < 0 ? 0 : std::min<std::size_t>(capacity, buffer.size());
    result.values.assign(buffer.begin(), buffer.begin() + size);
    result.after = size < buffer.size() ? buffer[size] : Fill32;
    return result;
}

using Values = std::vector<std::int32_t>;

void ToValid() {
    const std::u16string mixed{0x41, 0xd83d, 0xde00, 0xe000};
    auto result = To(mixed, 4, 8, -1);
    Require(!result.null && result.status == 0 && result.length == 3 && result.substitutions == 0);
    Require(Values(result.values.begin(), result.values.begin() + 4) == Values({0x41, 0x1f600, 0xe000, 0}));
    Require(result.values[4] == Fill32);

    result = To(std::u16string({0x41, 0xd83d, 0xde00, 0, 0x42}), -1, 8, -1);
    Require(result.status == 0 && result.length == 2 && result.values[2] == 0 && result.values[3] == Fill32);

    result = To(std::u16string({0x41, 0, 0x42}), 3, 8, -1);
    Require(result.status == 0 && result.length == 3 && result.values[1] == 0 && result.values[2] == 0x42 && result.values[3] == 0);

    result = To(std::u16string({0xd800, 0xdc00, 0xdbff, 0xdfff, 0xffff}), 5, 8, -1);
    Require(result.length == 3 && Values(result.values.begin(), result.values.begin() + 4) == Values({0x10000, 0x10ffff, 0xffff, 0}));

    result = To({}, 0, 4, -1);
    Require(!result.null && result.status == 0 && result.length == 0 && result.values[0] == 0 && result.values[1] == Fill32);
    result = To(mixed, 4, 8, -1, 0, false);
    Require(result.status == 0 && result.length == 3 && result.substitutions == -77);
}

void ToCapacity() {
    const std::u16string mixed{0x41, 0xd83d, 0xde00, 0xe000};
    auto result = To(mixed, 4, 3, -1);
    Require(result.status == NotTerminated && result.length == 3 && result.after == Fill32);
    Require(result.values == Values({0x41, 0x1f600, 0xe000}));

    result = To(mixed, 4, 2, -1);
    Require(result.status == Overflow && result.length == 3 && result.values == Values({0x41, 0x1f600}));

    result = To(mixed, 4, 0, -1);
    Require(result.status == Overflow && result.length == 3 && result.after == Fill32);

    result = To(std::u16string({0x41, 0}), -1, 1, -1);
    Require(result.status == NotTerminated && result.length == 1 && result.values == Values({0x41}));

    std::int32_t length = -77;
    std::int32_t status = 0;
    Require(u_strToUTF32_nid_postfix(nullptr, 0, &length, mixed.data(), 4, &status) == nullptr);
    Require(status == Overflow && length == 3);
    status = 0;
    Require(u_strToUTF32_nid_postfix(nullptr, 0, nullptr, nullptr, 0, &status) == nullptr);
    Require(status == NotTerminated);
}

void ToSubstitution() {
    auto result = To(std::u16string({0x41, 0xd800}), 2, 8, 0xfffd);
    Require(result.status == 0 && result.length == 2 && result.substitutions == 1);
    Require(Values(result.values.begin(), result.values.begin() + 3) == Values({0x41, 0xfffd, 0}));

    result = To(std::u16string({0xd800, 0x41, 0xdc00}), 3, 8, 0xfffd);
    Require(result.length == 3 && result.substitutions == 2 && Values(result.values.begin(), result.values.begin() + 4) == Values({0xfffd, 0x41, 0xfffd, 0}));

    result = To(std::u16string({0xd800, 0xd800, 0xdc00}), 3, 8, 0x3f);
    Require(result.length == 2 && result.substitutions == 1 && Values(result.values.begin(), result.values.begin() + 3) == Values({0x3f, 0x10000, 0}));

    result = To(std::u16string({0xdc00, 0xd800}), 2, 8, 0x1f600);
    Require(result.length == 2 && result.substitutions == 2 && Values(result.values.begin(), result.values.begin() + 3) == Values({0x1f600, 0x1f600, 0}));

    result = To(std::u16string({0xdc00, 0xdc00}), 2, 8, 0x3f);
    Require(result.length == 2 && result.substitutions == 2 && Values(result.values.begin(), result.values.begin() + 3) == Values({0x3f, 0x3f, 0}));

    result = To(std::u16string({0xd800, 0}), -1, 8, 0x3f);
    Require(result.length == 1 && result.substitutions == 1 && result.values[0] == 0x3f && result.values[1] == 0);

    result = To(std::u16string({0xd800, 0xdc00}), 1, 1, 0x3f);
    Require(result.status == NotTerminated && result.length == 1 && result.substitutions == 1 && result.values == Values({0x3f}));

    result = To(std::u16string({0x41, 0xdc00, 0x42}), 3, 8, -1);
    Require(result.null && result.status == Invalid && result.length == -77 && result.substitutions == 0);
    Require(result.values[0] == 0x41 && result.values[1] == Fill32);

    result = To(std::u16string({0xd800}), 1, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
    result = To(std::u16string({0xd800}), -1, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
}

void ToArguments() {
    const std::u16string text{0x41};
    for (auto substitution : {0x110000, 0xd800, 0xdfff, 0x7fffffff}) {
        const auto result = To(text, 1, 8, substitution);
        Require(result.null && result.status == Illegal && result.length == -77 && result.substitutions == -77 && result.values[0] == Fill32);
    }
    for (auto length : {-2, std::numeric_limits<std::int32_t>::min()}) {
        const auto result = To(text, length, 8, -1);
        Require(result.null && result.status == Illegal && result.length == -77);
    }
    Require(To(text, 1, -1, -1).status == Illegal);
    std::int32_t status = 0;
    std::int32_t length = -77;
    std::array<std::int32_t, 4> buffer{};
    Require(!u_strToUTF32_nid_postfix(buffer.data(), 4, &length, nullptr, 1, &status) && status == Illegal);
    status = 0;
    Require(!u_strToUTF32_nid_postfix(buffer.data(), 4, &length, nullptr, -1, &status) && status == Illegal);
    status = 0;
    Require(!u_strToUTF32_nid_postfix(nullptr, 1, &length, text.data(), 1, &status) && status == Illegal);
    Require(length == -77);
}

void ToStatus() {
    auto result = To(u"A", 1, 4, -1, Illegal);
    Require(result.null && result.status == Illegal && result.values[0] == Fill32 && result.length == -77);
    result = To(u"A", 1, 4, -1, NotTerminated);
    Require(!result.null && result.status == 0 && result.values[0] == 0x41 && result.values[1] == 0);
    result = To(u"A", 1, 1, -1, NotTerminated);
    Require(result.status == NotTerminated && result.length == 1);
    result = To(u"AB", 2, 1, -1, NotTerminated);
    Require(result.status == Overflow);

    std::array<std::int32_t, 4> buffer{};
    std::int32_t length = 0;
    RequireThrows([&] { u_strToUTF32_nid_postfix(buffer.data(), 4, &length, u"A", 1, nullptr); });
    RequireThrows([&] { u_strToUTF32WithSub_nid_postfix(buffer.data(), 4, &length, u"A", 1, -1, nullptr, nullptr); });
}

}

long PeakKilobytes() {
#ifdef __linux__
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
#else
    return 0;
#endif
}

void GuestBuffers() {
    const std::int32_t wide[] = {0x41, 0x1f600, 0};
    const char16_t narrow[] = {0x41, 0xd83d, 0xde00, 0};
    auto* const unmappedUnits = reinterpret_cast<char16_t*>(0x10);
    auto* const unmappedScalars = reinterpret_cast<std::int32_t*>(0x10);
    std::array<char16_t, 8> units{};
    std::array<std::int32_t, 8> scalars{};
    std::int32_t length = -1;
    std::int32_t substitutions = -1;
    std::int32_t status = 0;
    errno = 71;
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, reinterpret_cast<const std::int32_t*>(0x10), -1, 0xfffd, nullptr, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(unmappedUnits, 8, &length, wide, -1, 0xfffd, nullptr, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, unmappedScalars, wide, -1, 0xfffd, nullptr, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, wide, -1, 0xfffd, unmappedScalars, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, reinterpret_cast<const char16_t*>(0x10), -1, 0xfffd, nullptr, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strToUTF32WithSub_nid_postfix(unmappedScalars, 8, &length, narrow, -1, 0xfffd, nullptr, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, narrow, -1, 0xfffd, unmappedScalars, &status) == nullptr && status == 1);
    const long peakBefore = PeakKilobytes();
    status = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, reinterpret_cast<const std::int32_t*>(0x10), 0x7fffffff, 0xfffd, nullptr, &status) == nullptr && status == 1);
    status = 0;
    Require(u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, reinterpret_cast<const char16_t*>(0x10), 0x7fffffff, 0xfffd, nullptr, &status) == nullptr && status == 1);
    Require(PeakKilobytes() - peakBefore < 64 * 1024);
    Require(errno == 71);
    const std::vector<std::int32_t> many(3000, 0x42);
    std::vector<char16_t> converted(3001, 0xffff);
    status = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(converted.data(), 3001, &length, many.data(), 3000, 0xfffd, nullptr, &status) == converted.data());
    Require(status == 0 && length == 3000 && converted[0] == 0x42 && converted[2999] == 0x42 && converted[3000] == 0);
    status = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, wide, -1, 0xfffd, &substitutions, &status) == units.data());
    Require(status == 0 && length == 3 && substitutions == 0 && units[0] == 0x41 && units[1] == 0xd83d && units[2] == 0xde00 && units[3] == 0);
    status = 0;
    Require(u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, narrow, -1, 0xfffd, &substitutions, &status) == scalars.data());
    Require(status == 0 && length == 2 && substitutions == 0 && scalars[0] == 0x41 && scalars[1] == 0x1f600 && scalars[2] == 0);
}

void GuestStatus() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    std::memset(page, 0, pageSize);
    static_cast<std::int32_t*>(page)[1] = Illegal;
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0);
#endif
    auto* const readOnlyStatus = static_cast<std::int32_t*>(page);
    auto* const readOnlyFailure = readOnlyStatus + 1;
    auto* const unmappedStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto rejectsKeepingErrno = [](auto call) {
        errno = 71;
        bool caught = false;
        try { call(); } catch (const std::invalid_argument&) { caught = true; }
        return caught && errno == 71;
    };
    const std::int32_t wide[] = {0x41, 0x1f600, 0};
    const char16_t narrow[] = {0x41, 0xd83d, 0xde00, 0};
    std::array<char16_t, 8> units;
    std::array<std::int32_t, 8> scalars;
    std::int32_t length = -77;
    units.fill(Fill);
    scalars.fill(Fill32);
    Require(rejectsKeepingErrno([&] { u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, wide, -1, 0xfffd, nullptr, unmappedStatus); }));
    Require(rejectsKeepingErrno([&] { u_strFromUTF32_nid_postfix(units.data(), 8, &length, wide, -1, unmappedStatus); }));
    Require(rejectsKeepingErrno([&] { u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, narrow, -1, 0xfffd, nullptr, unmappedStatus); }));
    Require(rejectsKeepingErrno([&] { u_strToUTF32_nid_postfix(scalars.data(), 8, &length, narrow, -1, unmappedStatus); }));
    Require(length == -77 && units[0] == Fill && scalars[0] == Fill32);
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, wide, -1, 0xfffd, nullptr, readOnlyStatus) == units.data());
    Require(length == 3 && units[0] == 0x41 && units[1] == 0xd83d && units[2] == 0xde00 && units[3] == 0);
    Require(u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, narrow, -1, 0xfffd, nullptr, readOnlyStatus) == scalars.data());
    Require(length == 2 && scalars[0] == 0x41 && scalars[1] == 0x1f600 && scalars[2] == 0);
    length = -77;
    Require(u_strFromUTF32WithSub_nid_postfix(units.data(), 8, &length, wide, -1, 0xfffd, nullptr, readOnlyFailure) == nullptr);
    Require(u_strToUTF32WithSub_nid_postfix(scalars.data(), 8, &length, narrow, -1, 0xfffd, nullptr, readOnlyFailure) == nullptr);
    Require(length == -77);
    Require(rejectsKeepingErrno([&] { u_strFromUTF32WithSub_nid_postfix(units.data(), 1, &length, wide, -1, 0xfffd, nullptr, readOnlyStatus); }));
    Require(rejectsKeepingErrno([&] { u_strToUTF32WithSub_nid_postfix(scalars.data(), 1, &length, narrow, -1, 0xfffd, nullptr, readOnlyStatus); }));
    Require(*readOnlyStatus == 0 && *readOnlyFailure == Illegal);
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif
}

int main() {
    Valid();
    Capacity();
    Substitution();
    Arguments();
    Status();
    ToValid();
    ToCapacity();
    ToSubstitution();
    ToArguments();
    ToStatus();
    GuestBuffers();
    GuestStatus();
    std::puts("ICU UTF-32 conversion tests passed");
}
