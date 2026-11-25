#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/utf8.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <source_location>
#include <stdexcept>
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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTF-8 check failed at line %u\n", location.line());
    std::abort();
}

template<class TCall>
void Rejects(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

std::int32_t Error(int consumed, int strict) {
    if (strict == -3) return 0xfffd;
    if (strict < 0) return -1;
    constexpr std::array<std::int32_t, 4> legacy{0x15, 0x9f, 0xffff, 0x10ffff};
    return legacy[consumed];
}

void Sequence(std::initializer_list<std::uint8_t> bytes, int strict, std::int32_t nextValue, int nextIndex,
              std::int32_t previousValue, int previousIndex, int backIndex) {
    for (int origin : {0, 3}) {
        std::array<std::uint8_t, 12> text;
        text.fill(0x80);
        std::copy(bytes.begin(), bytes.end(), text.begin() + origin);
        const auto original = text;
        const auto length = static_cast<std::int32_t>(bytes.size());
        auto index = origin + 1;
        Require(utf8_nextCharSafeBody_nid_postfix(text.data(), &index, origin + length, text[origin], static_cast<std::int8_t>(strict)) == nextValue);
        Require(index == origin + nextIndex);
        index = origin + length - 1;
        Require(utf8_prevCharSafeBody_nid_postfix(text.data(), origin, &index, text[index], static_cast<std::int8_t>(strict)) == previousValue);
        Require(index == origin + previousIndex);
        Require(utf8_back1SafeBody_nid_postfix(text.data(), origin, origin + length - 1) == origin + backIndex);
        Require(text == original);
    }
}

void ErrorModes() {
    for (int strict = -128; strict <= 127; ++strict) {
        const auto e0 = Error(0, strict);
        const auto e1 = Error(1, strict);
        const auto e2 = Error(2, strict);
        const auto e3 = Error(3, strict);
        Sequence({0x41}, strict, e0, 1, e0, 0, 0);
        Sequence({0xc2, 0xa2}, strict, 0xa2, 2, 0xa2, 0, 0);
        Sequence({0xe2, 0x82, 0xac}, strict, 0x20ac, 3, 0x20ac, 0, 0);
        Sequence({0xf0, 0x9f, 0x98, 0x80}, strict, 0x1f600, 4, 0x1f600, 0, 0);
        Sequence({0xc0, 0x80}, strict, e0, 1, e0, 1, 1);
        Sequence({0xc1, 0xbf}, strict, e0, 1, e0, 1, 1);
        Sequence({0xe0, 0x80, 0x80}, strict, e0, 1, e0, 2, 2);
        Sequence({0xf0, 0x80, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xf4, 0x90, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xf5, 0x80, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xff, 0xbf, 0xbf, 0xbf}, strict, e0, 1, e0, 3, 3);
        Sequence({0xe1, 0xa0}, strict, e1, 2, e1, 0, 0);
        Sequence({0xf1, 0x90, 0x80}, strict, e2, 3, e2, 0, 0);
        Sequence({0xf1, 0x90}, strict, e1, 2, e1, 0, 0);
        Sequence({0xe1, 0xa0, 0x41}, strict, e1, 2, e0, 2, 2);
        Sequence({0xf0, 0x9f, 0x98, 0x41}, strict, e2, 3, e0, 3, 3);
        Sequence({0xef, 0xb7, 0x90}, strict, strict > 0 ? e2 : 0xfdd0, 3, strict > 0 ? e2 : 0xfdd0, 0, 0);
        Sequence({0xf4, 0x8f, 0xbf, 0xbf}, strict, strict > 0 ? e3 : 0x10ffff, 4, strict > 0 ? e3 : 0x10ffff, 0, 0);
        Sequence({0xed, 0xa0, 0x80}, strict, strict == -2 ? 0xd800 : e0, strict == -2 ? 3 : 1,
                 strict == -2 ? 0xd800 : e0, strict == -2 ? 0 : 2, 2);
        Sequence({0xed, 0xa0}, strict, strict == -2 ? e1 : e0, strict == -2 ? 2 : 1, e0, 1, 1);
        Sequence({0x80, 0x80, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xe0, 0xa0, 0x80}, strict, 0x800, 3, 0x800, 0, 0);
        Sequence({0xe0, 0x9f, 0xbf}, strict, e0, 1, e0, 2, 2);
        Sequence({0xef, 0xbf, 0xbe}, strict, strict > 0 ? e2 : 0xfffe, 3, strict > 0 ? e2 : 0xfffe, 0, 0);
        Sequence({0xf0, 0x9f, 0xbf, 0xbe}, strict, strict > 0 ? e3 : 0x1fffe, 4, strict > 0 ? e3 : 0x1fffe, 0, 0);
    }
}

int Encode(std::uint32_t value, std::uint8_t* output) {
    if (value <= 0x7f) {
        output[0] = static_cast<std::uint8_t>(value);
        return 1;
    }
    if (value <= 0x7ff) {
        output[0] = static_cast<std::uint8_t>(0xc0 + value / 64);
        output[1] = static_cast<std::uint8_t>(0x80 + value % 64);
        return 2;
    }
    if (value <= 0xffff) {
        output[0] = static_cast<std::uint8_t>(0xe0 + value / 4096);
        output[1] = static_cast<std::uint8_t>(0x80 + value / 64 % 64);
        output[2] = static_cast<std::uint8_t>(0x80 + value % 64);
        return 3;
    }
    output[0] = static_cast<std::uint8_t>(0xf0 + value / 262144);
    output[1] = static_cast<std::uint8_t>(0x80 + value / 4096 % 64);
    output[2] = static_cast<std::uint8_t>(0x80 + value / 64 % 64);
    output[3] = static_cast<std::uint8_t>(0x80 + value % 64);
    return 4;
}

void ScalarRoundTrips() {
    for (std::int32_t value = 0x80; value <= 0x10ffff; value += value < 0x10000 || value > 0x10fff8 ? 1 : 7) {
        if (value >= 0xd800 && value <= 0xdfff) continue;
        std::array<std::uint8_t, 6> encoded{};
        const int length = Encode(value, encoded.data() + 1);
        auto index = std::int32_t{2};
        Require(utf8_nextCharSafeBody_nid_postfix(encoded.data(), &index, length + 1, encoded[1], -1) == value);
        Require(index == length + 1);
        index = length;
        Require(utf8_prevCharSafeBody_nid_postfix(encoded.data(), 1, &index, encoded[length], -1) == value);
        Require(index == 1);
        for (int last = 2; last <= length; ++last) {
            Require(utf8_back1SafeBody_nid_postfix(encoded.data(), 1, last) == 1);
            Require(utf8_back1SafeBody_nid_postfix(encoded.data(), 2, last) == last);
        }
        std::array<std::uint8_t, 6> actual;
        actual.fill(0x55);
        std::int8_t error = -37;
        Require(utf8_appendCharSafeBody_nid_postfix(actual.data(), 1, length + 1, value, &error) == length + 1);
        Require(error == -37);
        Require(std::equal(encoded.begin() + 1, encoded.begin() + 1 + length, actual.begin() + 1));
        Require(actual[0] == 0x55 && actual[length + 1] == 0x55);
    }
    for (std::int32_t value = 0; value <= 0x7f; ++value) {
        std::array<std::uint8_t, 4> text{0x55, 0x55, 0x55, 0x55};
        std::int8_t error = 29;
        Require(utf8_appendCharSafeBody_nid_postfix(text.data(), 1, 3, value, &error) == 3);
        Require(text[0] == 0x55 && text[1] == 0xc0 + value / 64 && text[2] == 0x80 + value % 64 && text[3] == 0x55);
        Require(error == 29);
    }
}

void AppendFailures() {
    constexpr std::array<std::int32_t, 12> values{-1, INT32_MIN, 0, 0x7f, 0x80, 0x7ff, 0x800, 0xd800, 0xdfff, 0x10000, 0x110000, INT32_MAX};
    constexpr std::array<std::array<std::uint8_t, 3>, 4> replacements{{{}, {0x15}, {0xc2, 0x9f}, {0xef, 0xbf, 0xbf}}};
    for (const auto value : values) {
        const bool valid = value >= 0 && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
        const int width = value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4;
        for (int space = -2; space <= 5; ++space) {
            if (valid && space >= width) continue;
            for (bool hasError : {false, true}) {
                std::array<std::uint8_t, 10> text;
                text.fill(0x55);
                std::int8_t error = 47;
                const int count = hasError ? 0 : std::clamp(space, 0, 3);
                Require(utf8_appendCharSafeBody_nid_postfix(text.data(), 2, 2 + space, value, hasError ? &error : nullptr) == 2 + count);
                Require(error == (hasError ? 1 : 47));
                for (int index = 0; index < static_cast<int>(text.size()); ++index) {
                    Require(text[index] == (index >= 2 && index < 2 + count ? replacements[count][index - 2] : 0x55));
                }
            }
        }
    }
}

void TerminatedAndBounded() {
    constexpr std::array<std::uint8_t, 4> sequence{0xf0, 0x90, 0x80, 0x80};
    for (int size = 1; size <= 4; ++size) {
        auto text = std::make_unique<std::uint8_t[]>(size + 1);
        std::copy_n(sequence.begin(), size, text.get());
        text[size] = 0;
        for (std::int32_t length : {-1, -7, INT32_MIN, size}) {
            auto index = std::int32_t{1};
            Require(utf8_nextCharSafeBody_nid_postfix(text.get(), &index, length, sequence[0], -3) == (size == 4 ? 0x10000 : 0xfffd));
            Require(index == size);
        }
        auto index = static_cast<std::int32_t>(size - 1);
        const auto last = index;
        Require(utf8_prevCharSafeBody_nid_postfix(text.get(), last, &index, text[last], -1) == -1);
        Require(index == last);
        Require(utf8_back1SafeBody_nid_postfix(text.get(), last, last) == last);
    }
    for (int byte = 0; byte <= 255; ++byte) {
        std::array<std::uint8_t, 2> text{static_cast<std::uint8_t>(byte), 0x80};
        auto index = std::int32_t{1};
        Require(utf8_nextCharSafeBody_nid_postfix(text.data(), &index, 1, byte, -1) == -1 && index == 1);
    }
}

void InvalidArguments() {
    std::array<std::uint8_t, 4> text{0xf0, 0x90, 0x80, 0x80};
    std::int32_t index = 1;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), nullptr, 4, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(text.data(), 0, nullptr, 0x80, -1); });
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(nullptr, &index, 4, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(nullptr, 0, &index, 0x80, -1); });
    Rejects([&] { utf8_back1SafeBody_nid_postfix(nullptr, 0, 0); });
    Rejects([&] { utf8_appendCharSafeBody_nid_postfix(nullptr, 0, 4, 0x10000, nullptr); });
    Rejects([&] { utf8_appendCharSafeBody_nid_postfix(text.data(), -1, 4, 0x80, nullptr); });
    Require(utf8_appendCharSafeBody_nid_postfix(text.data(), 0, 4, 0x10000, reinterpret_cast<std::int8_t*>(0x10)) == 4);
    Rejects([&] { utf8_appendCharSafeBody_nid_postfix(text.data(), 0, 3, 0x10000, reinterpret_cast<std::int8_t*>(0x10)); });
    index = -1;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), &index, 4, 0xf0, -1); });
    index = 3;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), &index, 2, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(text.data(), 4, &index, 0x80, -1); });
    Rejects([&] { utf8_back1SafeBody_nid_postfix(text.data(), -1, 0); });
    index = 0;
    Require(utf8_nextCharSafeBody_nid_postfix(nullptr, &index, 0, 0xf0, -1) == -1 && index == 0);
    Require(utf8_prevCharSafeBody_nid_postfix(nullptr, 0, &index, 0x80, -1) == -1 && index == 0);
    for (const auto value : {0, 0x80, 0x800, 0xd800, 0x10000}) {
        const int width = value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4;
        const int position = INT32_MAX - (width - 1);
        std::int8_t error = 0;
        Require(utf8_appendCharSafeBody_nid_postfix(nullptr, position, INT32_MAX, value, &error) == position && error == 1);
        Rejects([&] { utf8_appendCharSafeBody_nid_postfix(nullptr, position + 1, INT32_MAX, value, &error); });
    }
    Rejects([] { utf8_appendCharSafeBody_nid_postfix(nullptr, INT32_MAX, INT32_MIN, -1, nullptr); });
    std::int8_t error = 0;
    Require(utf8_appendCharSafeBody_nid_postfix(nullptr, INT32_MAX, INT32_MIN, -1, &error) == INT32_MAX && error == 1);
    Require(utf8_appendCharSafeBody_nid_postfix(nullptr, 0, INT32_MIN, -1, nullptr) == 0);
}

class GuardedPage {
public:
    explicit GuardedPage(std::size_t count = 1) : pages(count) {
#ifdef _WIN32
        SYSTEM_INFO info {};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size * (pages + 2), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(base != nullptr);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size * (pages + 2), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<std::uint8_t*>(mapped);
#endif
        Protect(base, false, false);
        Protect(End(), false, false);
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size * (pages + 2));
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    std::uint8_t* Begin() const { return base + size; }
    std::uint8_t* SecondPage() const { return base + size * 2; }
    std::uint8_t* End() const { return base + size * (pages + 1); }
    void ReadOnly() const { Protect(Begin(), true, false); }

private:
    void Protect(std::uint8_t* page, bool read, bool write) const {
#ifdef _WIN32
        DWORD previous {};
        Require(VirtualProtect(page, size, write ? PAGE_READWRITE : read ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
        Require(mprotect(page, size, (read ? PROT_READ : 0) | (write ? PROT_WRITE : 0)) == 0);
#endif
    }

    std::size_t pages;
    std::uint8_t* base = nullptr;
    std::size_t size = 0;
};

void GuestPointers() {
    std::array<std::uint8_t, 4> text{0xf0, 0x90, 0x80, 0x80};
    auto* unmappedIndex = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto* unmappedText = reinterpret_cast<const std::uint8_t*>(std::uintptr_t{0x10});
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), unmappedIndex, 4, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(text.data(), 0, unmappedIndex, 0x80, -1); });
    std::int32_t index = 1;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(unmappedText, &index, 4, 0xf0, -1); });
    Require(index == 1);
    index = 3;
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(unmappedText, 0, &index, 0x80, -1); });
    Require(index == 3);
    Rejects([&] { utf8_back1SafeBody_nid_postfix(unmappedText, 0, 3); });

    GuardedPage page;
    auto* end = page.End();
    end[-3] = 0xe2;
    end[-2] = 0x82;
    end[-1] = 0xac;
    const auto* forward = end - 3;
    index = 1;
    Require(utf8_nextCharSafeBody_nid_postfix(forward, &index, -1, 0xe2, -1) == 0x20ac && index == 3);
    end[-2] = 0xf0;
    end[-1] = 0x90;
    index = 2;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(forward, &index, -1, 0xf0, -1); });
    Require(index == 2);
    index = 2;
    Require(utf8_nextCharSafeBody_nid_postfix(forward, &index, 3, 0xf0, -1) == -1 && index == 3);

    auto* begin = page.Begin();
    begin[0] = 0xe2;
    begin[1] = 0x82;
    begin[2] = 0xac;
    const auto* backward = begin - 8;
    index = 10;
    Require(utf8_prevCharSafeBody_nid_postfix(backward, 8, &index, 0xac, -1) == 0x20ac && index == 8);
    Require(utf8_back1SafeBody_nid_postfix(backward, 8, 10) == 8);
    begin[0] = 0x80;
    begin[1] = 0x80;
    index = 10;
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(backward, 7, &index, 0x80, -1); });
    Require(index == 10);
    Rejects([&] { utf8_back1SafeBody_nid_postfix(backward, 7, 9); });

    auto* stored = reinterpret_cast<std::int32_t*>(begin + 8);
    *stored = 1;
    begin[16] = 0xe2;
    begin[17] = 0x82;
    begin[18] = 0xac;
    page.ReadOnly();
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(begin + 16, stored, 3, 0xe2, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(begin + 16, 0, stored, 0x82, -1); });
    Require(*stored == 1);
}

void PageBoundaries() {
    constexpr std::array<std::uint32_t, 3> scalars{0xe9, 0x20ac, 0x1f600};
    GuardedPage accessible(2);
    GuardedPage guarded;
    for (const auto scalar : scalars) {
        std::array<std::uint8_t, 6> encoded{};
        const int width = Encode(scalar, encoded.data());
        for (int split = 1; split < width; ++split) {
            auto* text = accessible.SecondPage() - split;
            std::copy_n(encoded.data(), width, text);
            text[width] = 0;
            for (const int length : {width, -1}) {
                std::int32_t index = 1;
                Require(utf8_nextCharSafeBody_nid_postfix(text, &index, length, text[0], -1) == static_cast<std::int32_t>(scalar) && index == width);
            }
            std::int32_t index = width - 1;
            Require(utf8_prevCharSafeBody_nid_postfix(text, 0, &index, text[width - 1], -1) == static_cast<std::int32_t>(scalar) && index == 0);
            Require(utf8_back1SafeBody_nid_postfix(text, 0, width - 1) == 0);

            auto* tail = guarded.End() - split;
            std::copy_n(encoded.data(), split, tail);
            std::array<std::uint8_t, 6> host{};
            std::copy_n(encoded.data(), split, host.data());
            for (const int strict : {-1, 0}) {
                std::int32_t expectedIndex = 1;
                const auto expected = utf8_nextCharSafeBody(host.data(), &expectedIndex, split, host[0], static_cast<std::int8_t>(strict));
                index = 1;
                Require(utf8_nextCharSafeBody_nid_postfix(tail, &index, split, tail[0], static_cast<std::int8_t>(strict)) == expected && index == expectedIndex);
            }
            index = 1;
            Rejects([&] { utf8_nextCharSafeBody_nid_postfix(tail, &index, width, tail[0], -1); });
            Rejects([&] { utf8_nextCharSafeBody_nid_postfix(tail, &index, -1, tail[0], -1); });
            Require(index == 1);
        }
    }
}

struct NativeChecks {
    std::uint64_t decoding = 0;
    std::uint64_t cursor = 0;
};

void CompareNative(const std::array<std::uint8_t, 8>& text, int length, int strict, NativeChecks& checks) {
    constexpr int start = 2;
    auto expectedNext = start + 1;
    auto actualNext = expectedNext;
    const auto expectedForward = utf8_nextCharSafeBody(text.data(), &expectedNext, start + length, text[start], static_cast<std::int8_t>(strict));
    const auto actualForward = utf8_nextCharSafeBody_nid_postfix(text.data(), &actualNext, start + length, text[start], static_cast<std::int8_t>(strict));
    auto expectedPrevious = start + length - 1;
    auto actualPrevious = expectedPrevious;
    const auto expectedBackward = utf8_prevCharSafeBody(text.data(), start, &expectedPrevious, text[expectedPrevious], static_cast<std::int8_t>(strict));
    const auto actualBackward = utf8_prevCharSafeBody_nid_postfix(text.data(), start, &actualPrevious, text[actualPrevious], static_cast<std::int8_t>(strict));
    checks.decoding += 2;
    if (expectedForward != actualForward || expectedNext != actualNext || expectedBackward != actualBackward || expectedPrevious != actualPrevious) {
        std::fprintf(stderr, "Native UTF-8 mismatch: %02x %02x %02x %02x length=%d strict=%d next=%x/%x next_index=%d/%d prev=%x/%x prev_index=%d/%d\n",
                     static_cast<unsigned>(text[2]), static_cast<unsigned>(text[3]), static_cast<unsigned>(text[4]), static_cast<unsigned>(text[5]), length, strict,
                     static_cast<unsigned>(expectedForward), static_cast<unsigned>(actualForward), expectedNext, actualNext,
                     static_cast<unsigned>(expectedBackward), static_cast<unsigned>(actualBackward), expectedPrevious, actualPrevious);
        Require(false);
    }
    if (strict == 0) {
        const auto expectedBack = utf8_back1SafeBody(text.data(), start, start + length - 1);
        const auto actualBack = utf8_back1SafeBody_nid_postfix(text.data(), start, start + length - 1);
        ++checks.cursor;
        if (expectedBack != actualBack) {
            std::fprintf(stderr, "Native UTF-8 cursor mismatch: %02x %02x %02x %02x length=%d expected=%d actual=%d\n",
                         static_cast<unsigned>(text[2]), static_cast<unsigned>(text[3]), static_cast<unsigned>(text[4]), static_cast<unsigned>(text[5]), length, expectedBack, actualBack);
            Require(false);
        }
    }
}

void NativeMalformedSequences() {
    NativeChecks checks;
    std::array<std::uint8_t, 8> text{};
    text[0] = text[1] = 0x80;
    constexpr std::array<int, 6> modes{-128, -3, -2, -1, 0, 127};
    constexpr std::array<std::uint8_t, 11> boundaries{0, 0x41, 0x7f, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xbf, 0xc0, 0xff};
    for (int first = 0; first <= 255; ++first) {
        text[2] = static_cast<std::uint8_t>(first);
        for (int second = 0; second <= 255; ++second) {
            text[3] = static_cast<std::uint8_t>(second);
            for (const int strict : modes) CompareNative(text, 2, strict, checks);
        }
    }
    for (int first = 0xe0; first <= 0xf4; ++first) {
        text[2] = static_cast<std::uint8_t>(first);
        for (int second = 0; second <= 255; ++second) {
            text[3] = static_cast<std::uint8_t>(second);
            for (const auto third : boundaries) {
                text[4] = third;
                for (const int strict : modes) CompareNative(text, 3, strict, checks);
            }
        }
    }
    for (int first = 0xf0; first <= 0xf4; ++first) {
        text[2] = static_cast<std::uint8_t>(first);
        for (const auto second : boundaries) {
            text[3] = second;
            for (const auto third : boundaries) {
                text[4] = third;
                for (const auto fourth : boundaries) {
                    text[5] = fourth;
                    for (const int strict : modes) CompareNative(text, 4, strict, checks);
                }
            }
        }
    }
    constexpr std::array<std::array<std::uint8_t, 4>, 9> samples{{
        {0x41, 0, 0, 0}, {0x80, 0x80, 0x80, 0x80}, {0xc2, 0xa2, 0, 0},
        {0xe0, 0xa0, 0x80, 0}, {0xed, 0xa0, 0x80, 0}, {0xef, 0xb7, 0x90, 0},
        {0xf0, 0x90, 0x80, 0x80}, {0xf4, 0x8f, 0xbf, 0xbf}, {0xff, 0xbf, 0xbf, 0xbf},
    }};
    for (int strict = -128; strict <= 127; ++strict) {
        for (const auto& sample : samples) {
            std::copy(sample.begin(), sample.end(), text.begin() + 2);
            for (int length = 1; length <= 4; ++length) CompareNative(text, length, strict, checks);
        }
    }
    std::printf("ICU UTF-8 native comparisons: %llu decoding, %llu cursor\n", static_cast<unsigned long long>(checks.decoding), static_cast<unsigned long long>(checks.cursor));
}

}

int main() {
    ErrorModes();
    ScalarRoundTrips();
    AppendFailures();
    TerminatedAndBounded();
    InvalidArguments();
    GuestPointers();
    PageBoundaries();
    NativeMalformedSequences();
}
