#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>

namespace {

struct LongString {
    std::uint64_t capacityWithFlag;
    std::uint64_t size;
    char* data;
};

struct ReturnStorage {
    std::uint64_t before;
    std::array<char, 24> bytes;
    std::uint64_t after;
};

static_assert(sizeof(LongString) == 24);

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fputs(failure, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

void Check(float input, std::string_view expected) {
    ReturnStorage result{0x1122334455667788, {}, 0x8877665544332211};
    result.bytes.fill('x');
    Require(_ZNSt3__19to_stringEf_nid_postfix(result.bytes.data(), input) == result.bytes.data(),
            "to_string did not return the guest result address");
    Require(result.before == 0x1122334455667788 && result.after == 0x8877665544332211,
            "to_string wrote outside the guest result object");

    const auto flags = static_cast<unsigned char>(result.bytes[0]);
    if (expected.size() <= 22) {
        Require(flags == expected.size() * 2, "short string has incorrect size or flag");
        Require(std::string_view(result.bytes.data() + 1, expected.size()) == expected &&
                    result.bytes[expected.size() + 1] == '\0', "incorrect short string content");
        return;
    }

    LongString large{};
    std::memcpy(&large, result.bytes.data(), sizeof(large));
    Require(large.capacityWithFlag == 49 && large.size == expected.size() && large.data != nullptr,
            "long string has incorrect capacity, size or pointer");
    Require(std::string_view(large.data, large.size) == expected && large.data[large.size] == '\0',
            "incorrect long string content");
    _ZdlPvm_nid_postfix(large.data, large.capacityWithFlag & ~std::uint64_t{1});
}

}

int main() {
    const std::array<void*, 10> heapApi{};
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    Check(0.0f, "0.000000");
    Check(-0.0f, "-0.000000");
    Check(1.25f, "1.250000");
    Check(1.2345678f, "1.234568");
    Check(0x1p-7f, "0.007813");
    Check(0x1.8p-6f, "0.023438");
    Check(std::numeric_limits<float>::denorm_min(), "0.000000");
    Check(0x1p49f, "562949953421312.000000");
    Check(-0x1p49f, "-562949953421312.000000");
    Check(0x1p50f, "1125899906842624.000000");
    const float largest = std::numeric_limits<float>::max();
    for (float value : {largest, -largest}) {
        std::array<char, 48> formatted{};
        const int length = snprintf_nid_postfix(formatted.data(), formatted.size(), "%f", static_cast<double>(value));
        Require(length == (value < 0 ? 47 : 46), "incorrect length of largest finite float");
        Check(value, std::string_view(formatted.data(), static_cast<std::size_t>(length)));
    }
    Check(std::numeric_limits<float>::infinity(), "inf");
    Check(-std::numeric_limits<float>::infinity(), "-inf");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Check(nan, "nan");
    Check(std::copysign(nan, -1.0f), "-nan");
    std::puts("libc++ floating to_string ABI checks passed");
    return 0;
}
