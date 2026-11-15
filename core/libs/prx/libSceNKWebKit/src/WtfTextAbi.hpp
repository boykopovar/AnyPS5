#pragma once

#include "WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace NKWebKit::WtfAbi {

struct StringBuilder {
    mutable String string;
    StringImpl* buffer;
    union {
        unsigned char* characters8;
        char16_t* characters16;
    };
    bool overflowed;
    bool shouldCrashOnOverflow;
    std::int32_t length;
    bool is8Bit;
};

struct TextStream {
    StringBuilder text;
    unsigned formattingFlags;
    int indent;
    bool multiLineMode;
};

void AppendString(StringBuilder& builder, const StringImpl* string);
void AppendCharacter(StringBuilder& builder, unsigned char character);
void AppendCString(StringBuilder& builder, const char* characters);
void ReserveCapacity(StringBuilder& builder, unsigned newCapacity);
void AppendAdapted(StringBuilder& builder, const char* characters, unsigned length);
String ToString(StringBuilder& builder);
void Clear(StringBuilder& builder);
void DidOverflow(StringBuilder& builder);
void AllocateBufferUpConvert(StringBuilder& builder, const unsigned char* characters, unsigned requiredLength);
std::string NumberToString(float number);
std::string NumberToFixedWidthString(double number, unsigned decimalPlaces);

static_assert(sizeof(StringBuilder) == 40);
static_assert(alignof(StringBuilder) == 8);
static_assert(offsetof(StringBuilder, string) == 0);
static_assert(offsetof(StringBuilder, buffer) == 8);
static_assert(offsetof(StringBuilder, characters8) == 16);
static_assert(offsetof(StringBuilder, characters16) == 16);
static_assert(offsetof(StringBuilder, overflowed) == 24);
static_assert(offsetof(StringBuilder, shouldCrashOnOverflow) == 25);
static_assert(offsetof(StringBuilder, length) == 28);
static_assert(offsetof(StringBuilder, is8Bit) == 32);
static_assert(sizeof(TextStream) == 56);
static_assert(offsetof(TextStream, text) == 0);
static_assert(offsetof(TextStream, formattingFlags) == 40);
static_assert(offsetof(TextStream, indent) == 44);
static_assert(offsetof(TextStream, multiLineMode) == 48);

}  // namespace NKWebKit::WtfAbi
