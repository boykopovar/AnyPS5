#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeFilterText.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceImeFilterText(char16_t* dst, int32_t* out_length, uint32_t capacity,
                               const ime_filter::FilterParam* param, const char16_t* src, uint32_t src_length);
}

namespace {

constexpr int kInvalidParam = static_cast<int>(0x80bc0030u);
constexpr int kInvalidAddress = static_cast<int>(0x80bc0031u);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

struct Result {
    int code;
    std::vector<char16_t> text;
};

Result Run(uint64_t mask, uint32_t type, uint32_t capacity, std::initializer_list<char16_t> input,
           uint32_t length = UINT32_MAX) {
    std::vector<char16_t> src(input);
    src.push_back(0);
    if (length == UINT32_MAX) length = static_cast<uint32_t>(input.size());
    std::vector<char16_t> dst(capacity + 4, 0xcccc);
    int32_t count = -1;
    const ime_filter::FilterParam param{mask, type, 0, 0};
    const int code = sceImeFilterText(dst.data(), &count, capacity, &param, src.data(), length);
    Result result{code, {}};
    if (code == 0) {
        Require(count >= 0 && static_cast<uint32_t>(count) < capacity, "length must leave room for the terminator");
        Require(dst[static_cast<size_t>(count)] == 0, "output must be terminated");
        result.text.assign(dst.begin(), dst.begin() + count);
    }
    return result;
}

void Expect(const Result& result, std::initializer_list<char16_t> expected, const char* message) {
    Require(result.code == 0, message);
    Require(result.text == std::vector<char16_t>(expected), message);
}

void CheckArguments() {
    char16_t dst[4];
    int32_t count = 0;
    const char16_t src[2] = {u'a', 0};
    const ime_filter::FilterParam param{0, 1, 0, 0};
    Require(sceImeFilterText(nullptr, &count, 4, &param, src, 1) == kInvalidAddress, "null destination");
    Require(sceImeFilterText(dst, nullptr, 4, &param, src, 1) == kInvalidAddress, "null length");
    Require(sceImeFilterText(dst, &count, 4, nullptr, src, 1) == kInvalidAddress, "null parameter");
    Require(sceImeFilterText(dst, &count, 4, &param, nullptr, 1) == kInvalidAddress, "null source");
    Require(Run(0, 1, 0, {u'a'}).code == kInvalidParam, "capacity 0");
    Require(Run(0, 1, 1, {u'a'}).code == kInvalidParam, "capacity below 2");
    Require(Run(0, 5, 16, {u'a'}).code == kInvalidParam, "type 5");
    Require(Run(0, 0xff, 16, {u'a'}).code == kInvalidParam, "type 0xff");
    Require(Run(0, 0x104, 16, {u'a'}).code == kInvalidParam, "type 0x104");
}

void CheckSimpleTypes() {
    Expect(Run(0, 1, 16, {0x41, 0x7f, 0x42, 0xe9, 0x20ac, 0x7e}), {0x41, 0x42, 0x7e}, "ascii type");
    Expect(Run(0, 3, 16, {0x41, 0x7f, 0x42}), {0x41, 0x42}, "type 3");
    Expect(Run(0, 0x102, 16, {0x41, 0xe9, 0x7e}), {0x41, 0x7e}, "type 0x102");
    Expect(Run(0, 4, 16, {0x31, 0x2c, 0x41, 0x2e, 0x2d, 0x0a, 0xff11}), {0x31, 0x2c, 0x2e, 0x2d, 0x0a}, "number type");
}

void CheckLatinType() {
    Expect(Run(0, 0x100, 16, {0x41, 0x17e, 0x17f, 0x1a0, 0x218}), {0x41, 0x17e, 0x1a0, 0x218}, "latin type");
    Expect(Run(0x4000001f, 0x100, 16, {0x41, 0x17f, 0x1a0, 0x1af, 0x20ab, 0x300, 0x323, 0x1ea0, 0x218}),
           {0x41, 0x1a0, 0x1af, 0x20ab, 0x300, 0x323, 0x1ea0}, "vietnamese latin");
    Expect(Run(0x8000001f, 0x100, 16, {0x41, 0x218, 0x219, 0x21a, 0x21b, 0x21c}),
           {0x41, 0x218, 0x219, 0x21a, 0x21b}, "romanian latin");
}

void CheckClassifiedType() {
    Expect(Run(0, 0, 32, {0x31, 0x2c, 0x42, 0x39, 0xe9, 0x3b1, 0x430, 0x4e2d, 0x3042, 0xe01, 0x627, 0xac00}),
           {0x31, 0x2c, 0x42, 0x39, 0xe9, 0x3b1, 0x430, 0x4e2d, 0x3042, 0xe01, 0x627, 0xac00}, "default classes");
    Expect(Run(0x400001f, 0, 16, {0x41, 0xe01, 0xe5b, 0xe80, 0x627}), {0x41, 0xe01, 0xe5b}, "thai class");
    Expect(Run(0x20000041full, 0, 16, {0x41, 0x410, 0x411}), {0x41, 0x410, 0x411}, "cyrillic class");
}

void CheckBounds() {
    Expect(Run(0, 1, 4, {0x41, 0x42, 0x43, 0x44, 0x45}), {0x41, 0x42, 0x43}, "capacity limit");
    Expect(Run(0, 1, 16, {0x41, 0, 0x42}), {0x41}, "stops at terminator");
    Expect(Run(0, 1, 16, {0x41, 0x42, 0x43}, 2), {0x41, 0x42}, "source length limit");
    Expect(Run(0, 1, 16, {0x41, 0x42, 0x43}, 0), {}, "empty source");
}

}

int main() {
    CheckArguments();
    CheckSimpleTypes();
    CheckLatinType();
    CheckClassifiedType();
    CheckBounds();
    return 0;
}
