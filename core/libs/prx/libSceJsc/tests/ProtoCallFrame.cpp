#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::JscAbi::ProtoCallFrame;
using CodeBlockFunction = void* (APS5_VABI*)(const ProtoCallFrame*);
using NeedArityCheckFunction = bool (APS5_VABI*)(ProtoCallFrame*);
using InitFunction = void (APS5_VABI*)(ProtoCallFrame*, void*, void*, void*, std::uint64_t, int, std::uint64_t*);

static_assert(std::is_same_v<decltype(&_ZNK3JSC14ProtoCallFrame9codeBlockEv), CodeBlockFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC14ProtoCallFrame14needArityCheckEv), NeedArityCheckFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_), InitFunction>);

constexpr unsigned char kPoison = 0xA5;

struct alignas(8) FrameBytes {
    unsigned char bytes[sizeof(ProtoCallFrame)];

    FrameBytes() { std::memset(bytes, kPoison, sizeof(bytes)); }
    ProtoCallFrame* Frame() { return reinterpret_cast<ProtoCallFrame*>(bytes); }
    template <typename T>
    T Read(std::size_t offset) const {
        T value{};
        std::memcpy(&value, bytes + offset, sizeof(T));
        return value;
    }
    template <typename T>
    void Write(std::size_t offset, T value) {
        std::memcpy(bytes + offset, &value, sizeof(T));
    }
};

void Init(FrameBytes& frame, void* globalObject, void* callee, std::uint64_t thisValue, int count, std::uint64_t* args) {
    _ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_(
        frame.Frame(), nullptr, globalObject, callee, thisValue, count, args);
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestInitWritesEveryField() {
    FrameBytes frame;
    int globalObject = 0;
    int callee = 0;
    std::uint64_t args[3] = {0x1111, 0x2222, 0x3333};
    const std::uint64_t thisValue = 0xFFFE000000000007ull;
    Init(frame, &globalObject, &callee, thisValue, 4, args);
    Require(frame.Read<std::uint64_t>(0) == 0);
    Require(frame.Read<std::uintptr_t>(8) == reinterpret_cast<std::uintptr_t>(&callee));
    Require(frame.Read<std::int32_t>(16) == 4);
    Require(frame.Read<std::int32_t>(20) == 0);
    Require(frame.Read<std::uint64_t>(24) == thisValue);
    Require(frame.Read<std::uint32_t>(32) == 5);
    Require(frame.Read<unsigned char>(36) == 0);
    for (std::size_t offset = 37; offset < 40; ++offset) Require(frame.bytes[offset] == kPoison);
    Require(frame.Read<std::uint64_t*>(40) == args);
    Require(frame.Read<std::uintptr_t>(48) == reinterpret_cast<std::uintptr_t>(&globalObject));
    Require(args[0] == 0x1111 && args[1] == 0x2222 && args[2] == 0x3333);
}

void TestInitAcceptsNullPointers() {
    FrameBytes frame;
    Init(frame, nullptr, nullptr, 0, 1, nullptr);
    Require(frame.Read<std::uint64_t>(0) == 0);
    Require(frame.Read<std::uint64_t>(8) == 0);
    Require(frame.Read<std::int32_t>(16) == 1);
    Require(frame.Read<std::uint64_t>(24) == 0);
    Require(frame.Read<std::uint32_t>(32) == 1);
    Require(frame.Read<std::uint64_t*>(40) == nullptr);
    Require(frame.Read<std::uint64_t>(48) == 0);
}

void TestPaddedArgumentCount() {
    struct Case {
        int count;
        std::uint32_t padded;
    };
    const Case cases[] = {
        {0, 1u},
        {1, 1u},
        {2, 3u},
        {3, 3u},
        {6, 7u},
        {7, 7u},
        {INT_MAX, 0x7FFFFFFFu},
        {INT_MAX - 1, 0x7FFFFFFFu},
        {INT_MIN, 0x80000001u},
        {-1, 0xFFFFFFFFu},
        {-4, 0xFFFFFFFDu},
        {-5, 0xFFFFFFFBu},
        {-6, 0xFFFFFFFBu},
    };
    for (const Case& test : cases) {
        FrameBytes frame;
        Init(frame, nullptr, nullptr, 0, test.count, nullptr);
        Require(frame.Read<std::uint32_t>(32) == test.padded);
        Require(frame.Read<std::int32_t>(16) == test.count);
    }
}

void TestInitClearsArityMismatchAndTag() {
    FrameBytes frame;
    frame.Write<bool>(36, true);
    frame.Write<std::int32_t>(20, 0x1234);
    Require(_ZN3JSC14ProtoCallFrame14needArityCheckEv(frame.Frame()));
    Init(frame, nullptr, nullptr, 0, 2, nullptr);
    Require(!_ZN3JSC14ProtoCallFrame14needArityCheckEv(frame.Frame()));
    Require(frame.Read<std::int32_t>(20) == 0);
}

void TestNeedArityCheckReadsTheFlag() {
    FrameBytes frame;
    frame.Write<bool>(36, false);
    Require(!_ZN3JSC14ProtoCallFrame14needArityCheckEv(frame.Frame()));
    frame.Write<bool>(36, true);
    FrameBytes before = frame;
    Require(_ZN3JSC14ProtoCallFrame14needArityCheckEv(frame.Frame()));
    Require(std::memcmp(before.bytes, frame.bytes, sizeof(frame.bytes)) == 0);
}

void TestCodeBlockReadsSlotZero() {
    FrameBytes frame;
    int codeBlock = 0;
    frame.Write<std::uintptr_t>(0, reinterpret_cast<std::uintptr_t>(&codeBlock));
    FrameBytes before = frame;
    Require(_ZNK3JSC14ProtoCallFrame9codeBlockEv(frame.Frame()) == &codeBlock);
    Require(std::memcmp(before.bytes, frame.bytes, sizeof(frame.bytes)) == 0);
    Init(frame, nullptr, nullptr, 0, 1, nullptr);
    Require(_ZNK3JSC14ProtoCallFrame9codeBlockEv(frame.Frame()) == nullptr);
}

void TestReinitReplacesFields() {
    FrameBytes frame;
    int firstGlobal = 0;
    int secondGlobal = 0;
    std::uint64_t args[2] = {};
    Init(frame, &firstGlobal, &firstGlobal, 0xA, 3, args);
    Init(frame, &secondGlobal, nullptr, 0xB, 6, nullptr);
    Require(frame.Read<std::uint64_t>(8) == 0);
    Require(frame.Read<std::int32_t>(16) == 6);
    Require(frame.Read<std::uint64_t>(24) == 0xB);
    Require(frame.Read<std::uint32_t>(32) == 7);
    Require(frame.Read<std::uint64_t*>(40) == nullptr);
    Require(frame.Read<std::uintptr_t>(48) == reinterpret_cast<std::uintptr_t>(&secondGlobal));
}

void TestNullFrameThrows() {
    Require(Throws([] { _ZNK3JSC14ProtoCallFrame9codeBlockEv(nullptr); }));
    Require(Throws([] { _ZN3JSC14ProtoCallFrame14needArityCheckEv(nullptr); }));
    Require(Throws([] {
        _ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_(
            nullptr, nullptr, nullptr, nullptr, 0, 1, nullptr);
    }));
}

} // namespace

int main() {
    TestInitWritesEveryField();
    TestInitAcceptsNullPointers();
    TestPaddedArgumentCount();
    TestInitClearsArityMismatchAndTag();
    TestNeedArityCheckReadsTheFlag();
    TestCodeBlockReadsSlotZero();
    TestReinitReplacesFields();
    TestNullFrameThrows();
    return 0;
}
