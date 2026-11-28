#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using NKWebKit::JscBytecodeDumpAbi::CacheableIdentifier;
using NKWebKit::JscBytecodeDumpAbi::CodeOrigin;
using NKWebKit::JscBytecodeDumpAbi::OutOfLineCodeOrigin;
using NKWebKit::WtfAbi::StringImpl;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC bytecode dump check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

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

std::uintptr_t Inline(std::uint32_t bytecodeIndexBits, std::uintptr_t inlineCallFrame = 0) {
    return (static_cast<std::uintptr_t>(bytecodeIndexBits) << 48) | inlineCallFrame;
}

std::string DumpOrigin(std::uintptr_t compositeValue) {
    Streams streams;
    const CodeOrigin origin{compositeValue};
    _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, streams.Out());
    Require(origin.compositeValue == compositeValue);
    return streams.log;
}

bool OriginThrows(std::uintptr_t compositeValue) {
    Streams streams;
    const CodeOrigin origin{compositeValue};
    const bool threw = Throws([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, streams.Out()); });
    return threw && streams.log.empty();
}

std::string Offset(const char* offset) {
    return std::string("<A<B[B%s:bc#][B%u:") + offset + "]B>A>";
}

std::string OffsetAndCheckpoint(const char* offset, const char* checkpoint) {
    return std::string("<A<B[B%s:bc#][B%u:") + offset + "]B><B[B%s:cp#][B%u:" + checkpoint + "]B>A>";
}

void TestCodeOrigin() {
    Require(DumpOrigin(2) == "<A[B%s:<none>]A>");
    Require(DumpOrigin(Inline(0xFFFFFFFF, 2)) == "<A[B%s:<none>]A>");
    Require(DumpOrigin((1 << 3) | 2) == "<A[B%s:<none>]A>");
    Require(DumpOrigin(0) == Offset("0"));
    Require(DumpOrigin(4) == Offset("0"));
    Require(DumpOrigin(Inline(5 << 2)) == Offset("5"));
    Require(DumpOrigin(Inline((5 << 2) | 2)) == OffsetAndCheckpoint("5", "2"));
    Require(DumpOrigin(Inline((7 << 2) | 1)) == OffsetAndCheckpoint("7", "1"));
    Require(DumpOrigin(Inline(0xFFFF)) == OffsetAndCheckpoint("16383", "3"));

    alignas(8) static OutOfLineCodeOrigin outOfLine{nullptr, (70000u << 2) | 3};
    const std::uintptr_t outOfLineValue = reinterpret_cast<std::uintptr_t>(&outOfLine) | 1;
    Require(DumpOrigin(outOfLineValue) == OffsetAndCheckpoint("70000", "3"));
    outOfLine.bytecodeIndex = 0x10000u << 2;
    Require(DumpOrigin(outOfLineValue) == Offset("65536"));

    alignas(8) static const char inlineCallFrame[16] = {};
    const std::uintptr_t frame = reinterpret_cast<std::uintptr_t>(&inlineCallFrame);
    Require(OriginThrows(Inline(5 << 2, frame)));
    Require(OriginThrows(Inline(0, 8)));
    Require(OriginThrows(1));
    outOfLine.inlineCallFrame = &inlineCallFrame;
    Require(OriginThrows(outOfLineValue));
}

StringImpl Latin1(const char* characters, unsigned length) {
    return StringImpl{2, length, characters, 4};
}

StringImpl Utf16(const char16_t* characters, unsigned length) {
    return StringImpl{2, length, characters, 0};
}

std::string DumpIdentifier(std::uintptr_t bits) {
    Streams streams;
    const CacheableIdentifier identifier{bits};
    _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(&identifier, streams.Out());
    Require(identifier.bits == bits);
    return streams.log;
}

std::string DumpUid(const StringImpl& uid) {
    const StringImpl copy = uid;
    std::string log = DumpIdentifier(reinterpret_cast<std::uintptr_t>(&uid) | 1);
    Require(uid.refCount == copy.refCount && uid.length == copy.length && uid.data == copy.data && uid.hashAndFlags == copy.hashAndFlags);
    return log;
}

std::string Uid(const std::string& text) {
    return "<A[B%s:uid:(][B%s:" + text + "][B%s:)]A>";
}

void TestCacheableIdentifier() {
    Require(DumpIdentifier(0) == "<A[B%s:]A>");
    Require(DumpIdentifier(1) == Uid("(null StringImpl*)"));
    Require(DumpUid(Latin1("length", 6)) == Uid("length"));
    Require(DumpUid(Latin1("prototype", 9)) == Uid("prototype"));
    Require(DumpUid(Latin1("", 0)) == Uid(""));
    Require(DumpUid(Latin1("caf\xE9", 4)) == Uid("caf\xC3\xA9"));
    Require(DumpUid(Latin1("lengthy", 6)) == Uid("length"));

    static const char16_t wide[] = {u'x', 0x00E9, 0x4E2D, 0xD83D, 0xDE00};
    Require(DumpUid(Utf16(wide, 5)) == Uid("x\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"));
    Require(DumpUid(Utf16(u"name", 4)) == Uid("name"));
    Require(DumpUid(Utf16(u"", 0)) == Uid(""));

    alignas(16) static const char cell[16] = {};
    Streams streams;
    const CacheableIdentifier identifier{reinterpret_cast<std::uintptr_t>(&cell)};
    Require(Throws([&] { _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(&identifier, streams.Out()); }));
    Require(streams.log.empty());

    const StringImpl broken{2, 3, nullptr, 4};
    const CacheableIdentifier brokenIdentifier{reinterpret_cast<std::uintptr_t>(&broken) | 1};
    Require(Throws([&] { _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(&brokenIdentifier, streams.Out()); }));
    Require(streams.log.empty());
}

void TestNullArguments() {
    Streams streams;
    const CodeOrigin origin{};
    const CacheableIdentifier identifier{};
    Require(Throws([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, nullptr); }));
    Require(Throws([&] { _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(&identifier, nullptr); }));
    Require(streams.log.empty());
}

}

int main() {
    TestCodeOrigin();
    TestCacheableIdentifier();
    TestNullArguments();
    return 0;
}
