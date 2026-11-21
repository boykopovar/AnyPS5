#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucnv.h"
#include "unicode/ucnv_cb.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU converter substitution writer: %s\n", message);
    std::abort();
}

struct ConverterClose {
    void operator()(UConverter* converter) const { ucnv_close(converter); }
};

using Converter = std::unique_ptr<UConverter, ConverterClose>;

Converter Open(const char* name) {
    UErrorCode status = U_ZERO_ERROR;
    Converter converter{ucnv_open(name, &status)};
    Require(U_SUCCESS(status) && converter, "converter open");
    return converter;
}

enum class Substitute {
    Default,
    Bytes,
    Unicode,
    Empty
};

void Configure(UConverter* converter, Substitute substitute) {
    UErrorCode status = U_ZERO_ERROR;
    if (substitute == Substitute::Bytes) {
        constexpr std::array<char, 2> bytes{'[', ']'};
        ucnv_setSubstChars(converter, bytes.data(), static_cast<std::int8_t>(bytes.size()), &status);
    } else if (substitute == Substitute::Unicode) {
        constexpr std::array<char16_t, 1> text{0x03a9};
        ucnv_setSubstString(converter, text.data(), static_cast<std::int32_t>(text.size()), &status);
    } else if (substitute == Substitute::Empty) {
        constexpr char16_t text{};
        ucnv_setSubstString(converter, &text, 0, &status);
    }
    Require(U_SUCCESS(status), "substitution configuration");
}

struct Outcome {
    std::array<char, 16> output;
    std::array<std::int32_t, 16> offsets;
    std::array<char, 16> drained;
    std::int32_t status;
    std::int32_t drainStatus;
    std::size_t targetAdvance;
    std::size_t offsetAdvance;
    std::size_t drainedSize;
    int savedErrno;

    bool operator==(const Outcome&) const = default;
};

Outcome Write(bool guest, const char* name, Substitute substitute, std::size_t capacity, std::int32_t offsetIndex) {
    auto converter = Open(name);
    Configure(converter.get(), substitute);
    Outcome result{};
    result.output.fill(static_cast<char>(0x6d));
    result.offsets.fill(0x12345678);
    result.drained.fill(static_cast<char>(0x2c));
    auto* target = result.output.data() + 2;
    auto* offsets = result.offsets.data() + 2;
    UConverterFromUnicodeArgs arguments{
        sizeof(UConverterFromUnicodeArgs),
        false,
        converter.get(),
        reinterpret_cast<const char16_t*>(std::uintptr_t{1}),
        reinterpret_cast<const char16_t*>(std::uintptr_t{1}),
        target,
        target + capacity,
        offsets
    };
    UErrorCode status = U_ZERO_ERROR;
    errno = 73;
    if (guest) {
        ucnv_cbFromUWriteSub_nid_postfix(reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(&arguments), offsetIndex,
                                         reinterpret_cast<std::int32_t*>(&status));
    } else {
        ucnv_cbFromUWriteSub(&arguments, offsetIndex, &status);
    }
    result.savedErrno = errno;
    result.status = status;
    result.targetAdvance = static_cast<std::size_t>(arguments.target - target);
    result.offsetAdvance = static_cast<std::size_t>(arguments.offsets - offsets);
    const char16_t* source = nullptr;
    auto* drainTarget = result.drained.data() + 2;
    UErrorCode drainStatus = U_ZERO_ERROR;
    ucnv_fromUnicode(converter.get(), &drainTarget, result.drained.data() + result.drained.size() - 2,
                     &source, source, nullptr, false, &drainStatus);
    result.drainStatus = drainStatus;
    result.drainedSize = static_cast<std::size_t>(drainTarget - result.drained.data() - 2);
    return result;
}

void CheckGuards(const Outcome& outcome) {
    Require(outcome.output[0] == static_cast<char>(0x6d) && outcome.output[1] == static_cast<char>(0x6d), "output prefix guard");
    Require(outcome.output[14] == static_cast<char>(0x6d) && outcome.output[15] == static_cast<char>(0x6d), "output suffix guard");
    Require(outcome.offsets[0] == 0x12345678 && outcome.offsets[1] == 0x12345678, "offset prefix guard");
    Require(outcome.offsets[14] == 0x12345678 && outcome.offsets[15] == 0x12345678, "offset suffix guard");
    Require(outcome.drained[0] == static_cast<char>(0x2c) && outcome.drained[1] == static_cast<char>(0x2c), "drain prefix guard");
    Require(outcome.drained[14] == static_cast<char>(0x2c) && outcome.drained[15] == static_cast<char>(0x2c), "drain suffix guard");
    Require(outcome.savedErrno == 73, "errno preservation");
}

void CheckParity(const char* name, Substitute substitute, std::size_t capacity, std::int32_t offsetIndex) {
    const auto expected = Write(false, name, substitute, capacity, offsetIndex);
    const auto actual = Write(true, name, substitute, capacity, offsetIndex);
    Require(actual == expected, "bundled ICU parity");
    CheckGuards(actual);
}

void SuccessfulWrites() {
    const auto ascii = Write(true, "US-ASCII", Substitute::Default, 8, -19);
    CheckParity("US-ASCII", Substitute::Default, 8, -19);
    Require(ascii.status == U_ZERO_ERROR && ascii.targetAdvance == 1 && ascii.offsetAdvance == 1, "ASCII default advance");
    Require(static_cast<unsigned char>(ascii.output[2]) == 0x1a && ascii.offsets[2] == -19, "ASCII default bytes");
    Require(ascii.drainedSize == 0 && ascii.drainStatus == U_ZERO_ERROR, "ASCII default drain");

    const auto utf8 = Write(true, "UTF-8", Substitute::Default, 8, 37);
    CheckParity("UTF-8", Substitute::Default, 8, 37);
    Require(utf8.status == U_ZERO_ERROR && utf8.targetAdvance == 3 && utf8.offsetAdvance == 3, "UTF-8 default advance");
    Require(static_cast<unsigned char>(utf8.output[2]) == 0xef && static_cast<unsigned char>(utf8.output[3]) == 0xbf
            && static_cast<unsigned char>(utf8.output[4]) == 0xbd, "UTF-8 default bytes");
    Require(utf8.offsets[2] == 37 && utf8.offsets[3] == 37 && utf8.offsets[4] == 37, "UTF-8 default offsets");

    const auto bytes = Write(true, "UTF-8", Substitute::Bytes, 8, INT32_MIN);
    CheckParity("UTF-8", Substitute::Bytes, 8, INT32_MIN);
    Require(bytes.status == U_ZERO_ERROR && bytes.targetAdvance == 2 && bytes.offsetAdvance == 2, "custom byte advance");
    Require(bytes.output[2] == '[' && bytes.output[3] == ']' && bytes.offsets[2] == INT32_MIN && bytes.offsets[3] == INT32_MIN,
            "custom byte output");

    const auto unicode = Write(true, "UTF-8", Substitute::Unicode, 8, INT32_MAX);
    CheckParity("UTF-8", Substitute::Unicode, 8, INT32_MAX);
    Require(unicode.status == U_ZERO_ERROR && unicode.targetAdvance == 2 && unicode.offsetAdvance == 2, "Unicode substitution advance");
    Require(static_cast<unsigned char>(unicode.output[2]) == 0xce && static_cast<unsigned char>(unicode.output[3]) == 0xa9,
            "Unicode substitution bytes");
    Require(unicode.offsets[2] == INT32_MAX && unicode.offsets[3] == INT32_MAX, "Unicode substitution offsets");
}

void EmptyWrite() {
    const auto empty = Write(true, "UTF-8", Substitute::Empty, 0, 91);
    CheckParity("UTF-8", Substitute::Empty, 0, 91);
    Require(empty.status == U_ZERO_ERROR && empty.targetAdvance == 0 && empty.offsetAdvance == 0 && empty.drainedSize == 0,
            "empty substitution");
}

void OverflowWrite() {
    const auto overflow = Write(true, "UTF-8", Substitute::Default, 1, 52);
    CheckParity("UTF-8", Substitute::Default, 1, 52);
    Require(overflow.status == U_BUFFER_OVERFLOW_ERROR && overflow.targetAdvance == 1 && overflow.offsetAdvance == 1,
            "overflow advance");
    Require(static_cast<unsigned char>(overflow.output[2]) == 0xef && overflow.offsets[2] == 52, "overflow first byte");
    Require(overflow.drainStatus == U_ZERO_ERROR && overflow.drainedSize == 2, "overflow drain advance");
    Require(static_cast<unsigned char>(overflow.drained[2]) == 0xbf && static_cast<unsigned char>(overflow.drained[3]) == 0xbd,
            "overflow drain bytes");
}

void PrefailedWrite() {
    std::int32_t status = U_ILLEGAL_ARGUMENT_ERROR;
    errno = 73;
    ucnv_cbFromUWriteSub_nid_postfix(reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(std::uintptr_t{1}), INT32_MIN, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "prefailed status");
    Require(errno == 73, "prefailed errno");
}

}

int main() {
    SuccessfulWrites();
    EmptyWrite();
    OverflowWrite();
    PrefailedWrite();
}
