#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

using Jsc::ScriptCallAbi::ScriptCallArgumentHandler;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ScriptCallArgumentHandler check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename E, typename F> bool Throws(F function) {
    try {
        function();
    } catch (const E&) {
        return true;
    }
    return false;
}

std::uint64_t Double(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits + (1ull << 49);
}

constexpr std::uint64_t Int32(std::int32_t value) {
    return 0xfffe000000000000ull | static_cast<std::uint32_t>(value);
}

void TestConstructAndDestroy() {
    ScriptCallArgumentHandler handler;
    std::memset(&handler, 0xcd, sizeof(handler));
    int globalObject = 0;
    _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(&handler, &globalObject);
    Require(handler.arguments.overflowed == 0);
    Require(handler.arguments.size == 0);
    Require(handler.arguments.capacity == 8);
    Require(handler.arguments.buffer == handler.arguments.inlineBuffer);
    Require(handler.arguments.markSet == nullptr);
    Require(handler.globalObject == &globalObject);
    _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(&handler);

    std::memset(&handler, 0xcd, sizeof(handler));
    _ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE(&handler, nullptr);
    Require(handler.arguments.buffer == handler.arguments.inlineBuffer && handler.globalObject == nullptr);
    _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev(&handler);

    int markSet = 0;
    handler.arguments.markSet = &markSet;
    Require(Throws<std::runtime_error>([&] { _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(&handler); }));
    handler.arguments.markSet = nullptr;
    std::uint64_t heap[16] = {};
    handler.arguments.buffer = heap;
    Require(Throws<std::runtime_error>([&] { _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev(&handler); }));

    Require(Throws<std::invalid_argument>([] { _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(nullptr, nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(nullptr); }));
}

void TestAppend() {
    ScriptCallArgumentHandler handler;
    _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(&handler, nullptr);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE(&handler, 0x0a);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, true);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, false);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi(&handler, -1);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj(&handler, 0x80000000u);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(&handler, std::numeric_limits<std::int32_t>::min());
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(&handler, 0x7fffffffull);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx(&handler, -0x80000001ll);
    const std::uint64_t expected[8] = {0x0a, 0x07, 0x06, Int32(-1), Double(2147483648.0), Int32(std::numeric_limits<std::int32_t>::min()), Int32(0x7fffffff), Double(-2147483649.0)};
    Require(handler.arguments.size == 8);
    Require(std::memcmp(handler.arguments.inlineBuffer, expected, sizeof(expected)) == 0);
    Require(Throws<std::runtime_error>([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi(&handler, 1); }));
    Require(handler.arguments.size == 8);
    _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(&handler);

    _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(&handler, nullptr);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(&handler, std::numeric_limits<std::uint64_t>::max());
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(&handler, std::numeric_limits<std::int64_t>::max());
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj(&handler, 0x7fffffffu);
    Require(handler.arguments.inlineBuffer[0] == Double(18446744073709551616.0));
    Require(handler.arguments.inlineBuffer[1] == Double(9223372036854775808.0));
    Require(handler.arguments.inlineBuffer[2] == Int32(0x7fffffff));

    handler.arguments.size = 9;
    Require(Throws<std::invalid_argument>([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, true); }));
    handler.arguments.size = -1;
    Require(Throws<std::invalid_argument>([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, true); }));
    handler.arguments.size = 0;
    std::uint64_t heap[16] = {};
    handler.arguments.buffer = heap;
    handler.arguments.capacity = 16;
    Require(Throws<std::runtime_error>([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, true); }));
    Require(heap[0] == 0);
    Require(Throws<std::invalid_argument>([] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx(nullptr, 0); }));
}

void TestAppendDoubleRounding() {
    ScriptCallArgumentHandler handler;
    _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(&handler, nullptr);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(&handler, 9007199254740992ull);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(&handler, 9007199254740993ull);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(&handler, 9007199254740994ull);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(&handler, -9007199254740992ll);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(&handler, -9007199254740993ll);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(&handler, -9007199254740994ll);
    const std::uint64_t expected[6] = {0x4342000000000000ull, 0x4342000000000000ull, 0x4342000000000001ull, 0xc342000000000000ull, 0xc342000000000000ull, 0xc342000000000001ull};
    Require(handler.arguments.size == 6);
    Require(handler.arguments.buffer == handler.arguments.inlineBuffer);
    Require(std::memcmp(handler.arguments.inlineBuffer, expected, sizeof(expected)) == 0);
    _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(&handler);
}

} // namespace

int main() {
    TestConstructAndDestroy();
    TestAppend();
    TestAppendDoubleRounding();
    std::puts("ScriptCallArgumentHandler tests passed");
    return 0;
}
