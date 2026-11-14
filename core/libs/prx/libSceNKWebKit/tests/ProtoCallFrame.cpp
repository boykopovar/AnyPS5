#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using NKWebKit::JscAbi::ProtoCallFrame;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ProtoCallFrame check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

struct alignas(8) Frame {
    std::array<std::uint8_t, 72> bytes;

    explicit Frame(std::uint8_t fill) { bytes.fill(fill); }

    ProtoCallFrame* Get() { return reinterpret_cast<ProtoCallFrame*>(bytes.data()); }

    template <typename T>
    void Put(std::size_t offset, T value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); }

    template <typename T>
    T Read(std::size_t offset) const {
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
};

bool SameExcept(const Frame& a, const Frame& b, std::size_t begin, std::size_t end) {
    for (std::size_t i = 0; i < a.bytes.size(); ++i) {
        if (i >= begin && i < end) continue;
        if (a.bytes[i] != b.bytes[i]) return false;
    }
    return true;
}

void TestArgumentCount() {
    Frame frame(0xa5);
    frame.Put<std::int32_t>(16, 5);
    const Frame before = frame;
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == 4);
    Require(SameExcept(frame, before, 0, 0));

    frame.Put<std::int32_t>(16, 1);
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == 0);
    frame.Put<std::int32_t>(16, 0);
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == -1);
    frame.Put<std::int32_t>(16, INT_MIN);
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == INT_MAX);
    frame.Put<std::int32_t>(16, INT_MAX);
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == INT_MAX - 1);

    frame.Put<std::int32_t>(16, 3);
    frame.Put<std::int32_t>(20, 0x7eadbeef);
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == 2);

    Frame viaSetter(0x5a);
    _ZN3JSC14ProtoCallFrame29setArgumentCountIncludingThisEi(viaSetter.Get(), 7);
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(viaSetter.Get()) == 6);
    Require(static_cast<int>(_ZNK3JSC14ProtoCallFrame26argumentCountIncludingThisEv(viaSetter.Get())) == 7);
}

void TestArgument() {
    std::array<std::uint64_t, 4> arguments{0x1111111111111111ull, 0xfff0000000000002ull, 0x0002000000000000ull, 0xa5a5a5a5a5a5a5a5ull};
    Frame frame(0xa5);
    frame.Put<std::uint64_t*>(40, arguments.data());
    frame.Put<std::int32_t>(16, 4);
    const Frame before = frame;
    const auto argumentsBefore = arguments;

    Require(_ZN3JSC14ProtoCallFrame8argumentEm(frame.Get(), 0) == 0x1111111111111111ull);
    Require(_ZN3JSC14ProtoCallFrame8argumentEm(frame.Get(), 1) == 0xfff0000000000002ull);
    Require(_ZN3JSC14ProtoCallFrame8argumentEm(frame.Get(), 2) == 0x0002000000000000ull);
    Require(SameExcept(frame, before, 0, 0));
    Require(arguments == argumentsBefore);

    std::array<std::uint64_t, 2> other{1, 2};
    frame.Put<std::uint64_t*>(40, other.data());
    Require(_ZN3JSC14ProtoCallFrame8argumentEm(frame.Get(), 1) == 2);
}

void TestSetArgument() {
    std::array<std::uint64_t, 4> arguments{};
    arguments.fill(0x5a5a5a5a5a5a5a5aull);
    Frame frame(0xa5);
    frame.Put<std::uint64_t*>(40, arguments.data());
    const Frame before = frame;

    _ZN3JSC14ProtoCallFrame11setArgumentEmNS_7JSValueE(frame.Get(), 2, 0x0123456789abcdefull);
    Require(arguments[0] == 0x5a5a5a5a5a5a5a5aull);
    Require(arguments[1] == 0x5a5a5a5a5a5a5a5aull);
    Require(arguments[2] == 0x0123456789abcdefull);
    Require(arguments[3] == 0x5a5a5a5a5a5a5a5aull);
    Require(SameExcept(frame, before, 0, 0));

    _ZN3JSC14ProtoCallFrame11setArgumentEmNS_7JSValueE(frame.Get(), 0, 0);
    Require(arguments[0] == 0);
    Require(_ZN3JSC14ProtoCallFrame8argumentEm(frame.Get(), 2) == 0x0123456789abcdefull);
    Require(_ZN3JSC14ProtoCallFrame8argumentEm(frame.Get(), 0) == 0);
}

void TestClearCurrentVPC() {
    Frame frame(0xa5);
    frame.Put<std::int32_t>(16, 9);
    frame.Put<std::int32_t>(20, -1);
    const Frame before = frame;

    _ZN3JSC14ProtoCallFrame15clearCurrentVPCEv(frame.Get());
    Require(frame.Read<std::int32_t>(20) == 0);
    Require(frame.Read<std::int32_t>(16) == 9);
    Require(SameExcept(frame, before, 20, 24));
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(frame.Get()) == 8);

    _ZN3JSC14ProtoCallFrame15clearCurrentVPCEv(frame.Get());
    Require(frame.Read<std::int32_t>(20) == 0);
    Require(SameExcept(frame, before, 20, 24));
}

void TestAssignment() {
    std::uint64_t arguments[1] = {};
    Frame source(0x00);
    for (std::size_t i = 0; i < 56; ++i) source.bytes[i] = static_cast<std::uint8_t>(0x10 + i);
    source.Put<std::uint64_t*>(40, arguments);
    const Frame sourceBefore = source;

    Frame destination(0xa5);
    const Frame destinationBefore = destination;
    ProtoCallFrame& result = _ZN3JSC14ProtoCallFrameaSERKS0_(destination.Get(), *source.Get());
    Require(&result == destination.Get());
    for (std::size_t i = 0; i < 56; ++i) {
        if (i >= 36 && i < 40) continue;
        Require(destination.bytes[i] == source.bytes[i]);
    }
    for (std::size_t i = 56; i < destination.bytes.size(); ++i) Require(destination.bytes[i] == destinationBefore.bytes[i]);
    Require(SameExcept(source, sourceBefore, 0, 0));
    Require(destination.Read<std::uint64_t*>(40) == arguments);
    Require(destination.Read<std::uint32_t>(32) == source.Read<std::uint32_t>(32));
    Require(_ZNK3JSC14ProtoCallFrame13argumentCountEv(destination.Get()) == _ZNK3JSC14ProtoCallFrame13argumentCountEv(source.Get()));

    const Frame selfBefore = destination;
    ProtoCallFrame& selfResult = _ZN3JSC14ProtoCallFrameaSERKS0_(destination.Get(), *destination.Get());
    Require(&selfResult == destination.Get());
    Require(SameExcept(destination, selfBefore, 0, 0));
}

}

int main() {
    TestArgumentCount();
    TestArgument();
    TestSetArgument();
    TestClearCurrentVPC();
    TestAssignment();
    return 0;
}
