#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucnv.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <type_traits>

namespace {

using WriteUChars = void (APS5_VABI*)(Icu::ConverterFromUnicodeArgs*, const char16_t**, const char16_t*, std::int32_t, std::int32_t*);
static_assert(std::is_same_v<decltype(&ucnv_cbFromUWriteUChars_nid_postfix), WriteUChars>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU from-U UChar writer: %s\n", message);
    std::abort();
}

template <typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

struct Converter {
    UConverter* value;

    Converter() {
        UErrorCode status = U_ZERO_ERROR;
        value = ucnv_open("UTF-8", &status);
        Require(U_SUCCESS(status) && value, "open UTF-8 converter");
    }

    ~Converter() {
        ucnv_close(value);
    }
};

Icu::ConverterFromUnicodeArgs* Guest(UConverterFromUnicodeArgs* arguments) {
    return reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(arguments);
}

void SuccessfulWrite(Converter& converter) {
    const std::array<char16_t, 3> input{u'A', u'\u20ac', u'B'};
    const char16_t* source = input.data();
    std::array<char, 8> target;
    target.fill(static_cast<char>(0x5a));
    std::array<std::int32_t, 8> offsets;
    offsets.fill(0x13579bdf);
    UConverterFromUnicodeArgs arguments{};
    arguments.size = sizeof(arguments);
    arguments.converter = converter.value;
    arguments.source = reinterpret_cast<const UChar*>(input.data());
    arguments.sourceLimit = reinterpret_cast<const UChar*>(input.data() + input.size());
    arguments.target = target.data() + 1;
    arguments.targetLimit = target.data() + 7;
    arguments.offsets = offsets.data() + 1;
    std::int32_t status = U_ZERO_ERROR;
    errno = 73;
    ucnv_cbFromUWriteUChars_nid_postfix(Guest(&arguments), &source, input.data() + input.size(), -7, &status);
    Require(status == U_ZERO_ERROR, "successful status");
    Require(source == input.data() + input.size(), "successful source advance");
    Require(arguments.target == target.data() + 6, "successful target advance");
    Require(arguments.offsets == offsets.data() + 6, "successful offset advance");
    const std::array<unsigned char, 5> expected{'A', 0xe2, 0x82, 0xac, 'B'};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Require(static_cast<unsigned char>(target[index + 1]) == expected[index], "successful UTF-8 bytes");
        Require(offsets[index + 1] == -7, "successful offsets");
    }
    Require(target[0] == static_cast<char>(0x5a) && target[6] == static_cast<char>(0x5a) && target[7] == static_cast<char>(0x5a), "successful target guards");
    Require(offsets[0] == 0x13579bdf && offsets[6] == 0x13579bdf && offsets[7] == 0x13579bdf, "successful offset guards");
    Require(arguments.source == reinterpret_cast<const UChar*>(input.data()) && arguments.sourceLimit == reinterpret_cast<const UChar*>(input.data() + input.size()), "callback source fields unchanged");
    Require(errno == 73, "successful write preserves errno");
}

void EmptyWrite(Converter& converter) {
    ucnv_resetFromUnicode(converter.value);
    const std::array<char16_t, 1> input{u'Q'};
    const char16_t* source = input.data();
    std::array<char, 4> target;
    target.fill('x');
    std::array<std::int32_t, 4> offsets;
    offsets.fill(31);
    UConverterFromUnicodeArgs arguments{};
    arguments.size = sizeof(arguments);
    arguments.converter = converter.value;
    arguments.target = target.data() + 1;
    arguments.targetLimit = target.data() + 3;
    arguments.offsets = offsets.data() + 1;
    std::int32_t status = U_ZERO_ERROR;
    errno = 37;
    ucnv_cbFromUWriteUChars_nid_postfix(Guest(&arguments), &source, source, 12, &status);
    Require(status == U_ZERO_ERROR && source == input.data(), "empty source result");
    Require(arguments.target == target.data() + 1 && arguments.offsets == offsets.data() + 1, "empty output pointers");
    Require(target == std::array<char, 4>{'x', 'x', 'x', 'x'}, "empty target unchanged");
    Require(offsets == std::array<std::int32_t, 4>{31, 31, 31, 31}, "empty offsets unchanged");
    Require(errno == 37, "empty write preserves errno");
}

void OverflowWrite(Converter& converter) {
    ucnv_resetFromUnicode(converter.value);
    const std::array<char16_t, 3> input{u'A', u'B', u'C'};
    const char16_t* source = input.data();
    std::array<char, 3> target;
    target.fill('x');
    std::array<std::int32_t, 3> offsets;
    offsets.fill(41);
    UConverterFromUnicodeArgs arguments{};
    arguments.size = sizeof(arguments);
    arguments.converter = converter.value;
    arguments.target = target.data() + 1;
    arguments.targetLimit = target.data() + 2;
    arguments.offsets = offsets.data() + 1;
    std::int32_t status = U_ZERO_ERROR;
    errno = 19;
    ucnv_cbFromUWriteUChars_nid_postfix(Guest(&arguments), &source, input.data() + input.size(), 23, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR, "overflow status");
    Require(source == input.data() + input.size(), "overflow source buffered");
    Require(arguments.target == target.data() + 2 && target[1] == 'A', "overflow direct target");
    Require(arguments.offsets == offsets.data() + 2 && offsets[1] == 23, "overflow direct offset");
    Require(target[0] == 'x' && target[2] == 'x' && offsets[0] == 41 && offsets[2] == 41, "overflow guards");
    Require(errno == 19, "overflow write preserves errno");

    std::array<char, 5> drained;
    drained.fill('y');
    char* drainTarget = drained.data() + 1;
    const UChar* drainSource = reinterpret_cast<const UChar*>(source);
    UErrorCode drainStatus = U_ZERO_ERROR;
    ucnv_fromUnicode(converter.value, &drainTarget, drained.data() + 4, &drainSource, drainSource, nullptr, false, &drainStatus);
    Require(U_SUCCESS(drainStatus), "drain status");
    Require(drainTarget == drained.data() + 3 && drained[1] == 'B' && drained[2] == 'C', "drain buffered bytes");
    Require(drained[0] == 'y' && drained[3] == 'y' && drained[4] == 'y', "drain guards");
    Require(drainSource == reinterpret_cast<const UChar*>(input.data() + input.size()), "drain source unchanged");
}

void FailedStatusNoAccess() {
    std::array<std::int32_t, 3> status{0x12345678, U_ILLEGAL_ARGUMENT_ERROR, 0x76543210};
    errno = 91;
    ucnv_cbFromUWriteUChars_nid_postfix(
        reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(std::uintptr_t{1}),
        reinterpret_cast<const char16_t**>(std::uintptr_t{1}),
        reinterpret_cast<const char16_t*>(std::uintptr_t{1}), INT32_MIN, &status[1]);
    Require(status == std::array<std::int32_t, 3>{0x12345678, U_ILLEGAL_ARGUMENT_ERROR, 0x76543210}, "failed status and guards unchanged");
    Require(errno == 91, "failed status preserves errno");
}

void NullPointers() {
    std::int32_t status = U_ZERO_ERROR;
    RequireThrows([&] {
        ucnv_cbFromUWriteUChars_nid_postfix(nullptr, nullptr, nullptr, 0, &status);
    }, "null arguments did not throw");
    RequireThrows([] {
        ucnv_cbFromUWriteUChars_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr);
    }, "null status did not throw");
}

}

int main() {
    Converter converter;
    SuccessfulWrite(converter);
    EmptyWrite(converter);
    OverflowWrite(converter);
    FailedStatusNoAccess();
    NullPointers();
}
