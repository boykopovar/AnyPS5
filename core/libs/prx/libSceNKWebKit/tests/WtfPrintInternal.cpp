#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF printInternal check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::PrintStream;
using NKWebKit::WtfAbi::RawPointer;

struct SysVVaList {
    unsigned gpOffset;
    unsigned fpOffset;
    unsigned char* overflowArgArea;
    unsigned char* regSaveArea;
};

static_assert(sizeof(SysVVaList) == 24);

enum class Kind { Integer, Double };

struct FakeStream {
    void* const* vtable;
    FakeStream* beginResult;
    int vprintfCalls;
    int beginCalls;
    int endCalls;
    char format[8];
    std::uint64_t integer;
    double real;
    Kind kind;
};

static_assert(offsetof(FakeStream, vtable) == 0);

std::uint64_t NextInteger(SysVVaList* list) {
    std::uint64_t value;
    if (list->gpOffset < 48) {
        std::memcpy(&value, list->regSaveArea + list->gpOffset, sizeof(value));
        list->gpOffset += 8;
    } else {
        std::memcpy(&value, list->overflowArgArea, sizeof(value));
        list->overflowArgArea += 8;
    }
    return value;
}

double NextDouble(SysVVaList* list) {
    double value;
    if (list->fpOffset < 176) {
        std::memcpy(&value, list->regSaveArea + list->fpOffset, sizeof(value));
        list->fpOffset += 16;
    } else {
        std::memcpy(&value, list->overflowArgArea, sizeof(value));
        list->overflowArgArea += 8;
    }
    return value;
}

void APS5_VABI Unexpected(FakeStream*) {
    std::abort();
}

void APS5_VABI Vprintf(FakeStream* self, const char* format, SysVVaList* list) {
    ++self->vprintfCalls;
    Require(std::strlen(format) < sizeof(self->format));
    std::strcpy(self->format, format);
    if (!std::strcmp(format, "%lf")) {
        self->kind = Kind::Double;
        self->real = NextDouble(list);
    } else {
        self->kind = Kind::Integer;
        self->integer = NextInteger(list);
    }
}

FakeStream* APS5_VABI Begin(FakeStream* self) {
    ++self->beginCalls;
    return self->beginResult ? self->beginResult : self;
}

void APS5_VABI End(FakeStream* self) {
    ++self->endCalls;
}

void* const Table[] = {
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Vprintf),
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Begin),
    reinterpret_cast<void*>(&End),
};

FakeStream Make() {
    FakeStream stream{};
    stream.vtable = Table;
    return stream;
}

PrintStream& Out(FakeStream& stream) {
    return *reinterpret_cast<PrintStream*>(&stream);
}

void RequireFormat(const FakeStream& stream, const char* format, Kind kind) {
    Require(stream.vprintfCalls == 1);
    Require(stream.beginCalls == 0 && stream.endCalls == 0);
    Require(!std::strcmp(stream.format, format));
    Require(stream.kind == kind);
}

void RequireInteger(const FakeStream& stream, const char* format, std::uint64_t value) {
    RequireFormat(stream, format, Kind::Integer);
    Require(stream.integer == value);
}

void RequireInt32(const FakeStream& stream, const char* format, std::uint32_t value) {
    RequireFormat(stream, format, Kind::Integer);
    Require(static_cast<std::uint32_t>(stream.integer) == value);
}

void TestPrintf() {
    FakeStream stream = Make();
    _ZN3WTF11PrintStream6printfEPKcz(&Out(stream), "%u", 9u);
    RequireInt32(stream, "%u", 9);

    stream = Make();
    _ZN3WTF11PrintStream6printfEPKcz(&Out(stream), "%lf", 0.125);
    RequireFormat(stream, "%lf", Kind::Double);
    Require(stream.real == 0.125);
}

void TestCString() {
    static const char text[] = "text";
    FakeStream stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEPKc(Out(stream), text);
    RequireInteger(stream, "%s", reinterpret_cast<std::uintptr_t>(text));

    stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEPKc(Out(stream), nullptr);
    RequireInteger(stream, "%s", 0);
}

void TestWtfCString() {
    CString value;
    _ZN3WTF7CStringC1EPKc(&value, "wtf");
    FakeStream stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamERKNS_7CStringE(Out(stream), value);
    RequireInteger(stream, "%s", reinterpret_cast<std::uintptr_t>(value.buffer + 1));
    Require(!std::strcmp(reinterpret_cast<const char*>(stream.integer), "wtf"));

    CString null;
    stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamERKNS_7CStringE(Out(stream), null);
    RequireInteger(stream, "%s", 0);
}

void TestBool() {
    for (bool value : {true, false}) {
        FakeStream target = Make();
        FakeStream stream = Make();
        stream.beginResult = &target;
        _ZN3WTF13printInternalERNS_11PrintStreamEb(Out(stream), value);
        Require(stream.beginCalls == 1 && stream.endCalls == 1 && stream.vprintfCalls == 0);
        Require(target.beginCalls == 0 && target.endCalls == 0 && target.vprintfCalls == 1);
        Require(!std::strcmp(target.format, "%s"));
        Require(!std::strcmp(reinterpret_cast<const char*>(target.integer), value ? "true" : "false"));

        FakeStream self = Make();
        _ZN3WTF13printInternalERNS_11PrintStreamEb(Out(self), value);
        Require(self.beginCalls == 1 && self.endCalls == 1 && self.vprintfCalls == 1);
        Require(!std::strcmp(reinterpret_cast<const char*>(self.integer), value ? "true" : "false"));
    }
}

void TestIntegers() {
    FakeStream stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEi(Out(stream), -7);
    RequireInt32(stream, "%d", static_cast<std::uint32_t>(-7));

    stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEj(Out(stream), 0xFFFFFFFFu);
    RequireInt32(stream, "%u", 0xFFFFFFFFu);

    stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEl(Out(stream), INT64_MIN);
    RequireInteger(stream, "%ld", static_cast<std::uint64_t>(INT64_MIN));

    stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEm(Out(stream), UINT64_MAX);
    RequireInteger(stream, "%lu", UINT64_MAX);
}

void TestDouble() {
    FakeStream stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamEd(Out(stream), -2.5);
    RequireFormat(stream, "%lf", Kind::Double);
    Require(stream.real == -2.5);
}

void TestPointer() {
    int object = 0;
    FakeStream stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamENS_10RawPointerE(Out(stream), RawPointer{&object});
    RequireInteger(stream, "%p", reinterpret_cast<std::uintptr_t>(&object));

    stream = Make();
    _ZN3WTF13printInternalERNS_11PrintStreamENS_10RawPointerE(Out(stream), RawPointer{nullptr});
    RequireInteger(stream, "%p", 0);
}

void TestCharacter() {
    FakeStream stream = Make();
    _ZN3WTF13dumpCharacterERNS_11PrintStreamEc(Out(stream), 'Z');
    RequireInt32(stream, "%c", 'Z');
}

}  // namespace

int main() {
    TestPrintf();
    TestCString();
    TestWtfCString();
    TestBool();
    TestIntegers();
    TestDouble();
    TestPointer();
    TestCharacter();
    std::puts("WTF printInternal tests passed");
    return 0;
}
