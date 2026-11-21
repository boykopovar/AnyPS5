#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

using WTF::BitVector;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF object dump check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << 63;

struct TestStream {
    void* const* vtable;
    std::string* log;
    char tag;
    TestStream* target;
};

void APS5_VABI Unexpected(TestStream*) {
    std::abort();
}

void APS5_VABI Vprintf(TestStream* self, const char* format, VaList* arguments) {
    char buffer[128];
    Require(vsnprintf_nid_postfix(buffer, sizeof(buffer), format, arguments) >= 0);
    *self->log += '[';
    *self->log += self->tag;
    *self->log += format;
    *self->log += ':';
    *self->log += buffer;
    *self->log += ']';
}

TestStream* APS5_VABI Begin(TestStream* self) {
    *self->log += '<';
    *self->log += self->tag;
    return self->target;
}

void APS5_VABI End(TestStream* self) {
    *self->log += self->tag;
    *self->log += '>';
}

void* const testVtable[] = {
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Vprintf),
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Begin),
    reinterpret_cast<void*>(&End),
};

struct Streams {
    std::string log;
    TestStream inner{testVtable, &log, 'B', nullptr};
    TestStream outer{testVtable, &log, 'A', &inner};

    Streams() {
        inner.target = &inner;
    }

    NKWebKit::WtfDumpAbi::PrintStream* Out() {
        return reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&outer);
    }
};

template<typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

std::string Expected(std::string_view bits) {
    std::string result;
    for (char bit : bits) {
        result += "<A[B%s:";
        result += bit;
        result += "]A>";
    }
    return result;
}

std::string Dump(const BitVector& vector) {
    Streams streams;
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, streams.Out());
    return streams.log;
}

std::string Bits(std::uintptr_t word, std::size_t count) {
    std::string result;
    for (std::size_t i = 0; i < count; ++i) result += ((word >> i) & 1) ? '1' : '-';
    return result;
}

void TestInlineBitVector() {
    Require(Dump(BitVector{InlineFlag}) == Expected(std::string(63, '-')));
    Require(Dump(BitVector{InlineFlag | 0x5}) == Expected("1-1" + std::string(60, '-')));
    const std::uintptr_t all = ~std::uintptr_t{0};
    Require(Dump(BitVector{all}) == Expected(std::string(63, '1')));
    const std::uintptr_t pattern = 0x00F0000000000301ull;
    Require(Dump(BitVector{InlineFlag | pattern}) == Expected(Bits(pattern, 63)));
}

void TestOutOfLineBitVector() {
    BitVector vector{InlineFlag | 0x3};
    _ZN3WTF9BitVector6resizeEm(&vector, 65);
    Require(!(vector.bitsOrPointer & InlineFlag));
    auto* bits = reinterpret_cast<BitVector::OutOfLineBits*>(vector.bitsOrPointer << 1);
    Require(bits->numBits == 128);
    auto* words = reinterpret_cast<std::uintptr_t*>(bits + 1);
    words[1] = (std::uintptr_t{1} << 0) | (std::uintptr_t{1} << 63);
    const std::uintptr_t before0 = words[0];
    const std::uintptr_t before1 = words[1];
    Require(Dump(vector) == Expected(Bits(words[0], 64) + Bits(words[1], 64)));
    Require(Dump(vector).rfind(Expected("11--"), 0) == 0);
    Require(words[0] == before0 && words[1] == before1 && bits->numBits == 128);
    _ZN3WTF8fastFreeEPv(bits);
}

void TestThreadDump() {
    alignas(16) unsigned char thread[64] = {};
    const auto* self = reinterpret_cast<const NKWebKit::WtfAbi::Thread*>(thread);
    Streams streams;
    _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(self, streams.Out());
    char pointer[64];
    Require(snprintf_nid_postfix(pointer, sizeof(pointer), "%p", static_cast<const void*>(thread)) > 0);
    Require(streams.log == std::string("<A[B%s:Thread:][B%p:") + pointer + "]A>");
    for (unsigned char byte : thread) Require(byte == 0);
}

void TestInvalidArguments() {
    Streams streams;
    const BitVector vector{InlineFlag};
    Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, nullptr); }));
    const BitVector empty{0};
    const BitVector deleted{1};
    Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&empty, streams.Out()); }));
    Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&deleted, streams.Out()); }));
    unsigned char thread[8] = {};
    Require(Throws([&] { _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(reinterpret_cast<const NKWebKit::WtfAbi::Thread*>(thread), nullptr); }));
    Require(streams.log.empty());
}

}

int main() {
    TestInlineBitVector();
    TestOutOfLineBitVector();
    TestThreadDump();
    TestInvalidArguments();
    return 0;
}
