#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfDumpAbi::PrintStream;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "typed array mode dump check failed at line %u\n", location.line());
    std::abort();
}

struct TestStream {
    void* const* vtable;
    TestStream* target;
    std::string* log;
    char tag;
    void* const* replacement = nullptr;
};

void APS5_VABI Write(TestStream* self, const char* format, VaList* arguments) {
    Require(std::strcmp(format, "%s") == 0);
    char text[80];
    const int length = vsnprintf_nid_postfix(text, sizeof(text), format, arguments);
    Require(length > 0 && static_cast<std::size_t>(length) < sizeof(text));
    *self->log += '[';
    *self->log += self->tag;
    *self->log += ':';
    *self->log += text;
    *self->log += ']';
}

TestStream* APS5_VABI Begin(TestStream* self) {
    *self->log += '<';
    *self->log += self->tag;
    if (self->replacement) self->vtable = self->replacement;
    return self->target;
}

void APS5_VABI End(TestStream* self) {
    *self->log += self->tag;
    *self->log += '>';
}

void APS5_VABI ReplacementEnd(TestStream* self) {
    *self->log += '!';
    End(self);
}

const std::array<void*, 6> vtable{
    nullptr, nullptr, reinterpret_cast<void*>(&Write), nullptr,
    reinterpret_cast<void*>(&Begin), reinterpret_cast<void*>(&End)
};

PrintStream* Stream(TestStream& value) {
    return reinterpret_cast<PrintStream*>(&value);
}

template<typename TFunction>
void RequireThrows(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false);
}

void AllModes() {
    std::array<const char*, 256> names{};
    names[0x10] = "FastTypedArray";
    names[0x30] = "OversizeTypedArray";
    names[0x58] = "WastefulTypedArray";
    names[0x5A] = "GrowableSharedWastefulTypedArray";
    names[0x5B] = "GrowableSharedAutoLengthWastefulTypedArray";
    names[0x5C] = "ResizableNonSharedWastefulTypedArray";
    names[0x5D] = "ResizableNonSharedAutoLengthWastefulTypedArray";
    names[0x88] = "DataViewMode";
    names[0x8A] = "GrowableSharedDataViewMode";
    names[0x8B] = "GrowableSharedAutoLengthDataViewMode";
    names[0x8C] = "ResizableNonSharedDataViewMode";
    names[0x8D] = "ResizableNonSharedAutoLengthDataViewMode";
    for (std::size_t mode = 0; mode < names.size(); ++mode) {
        std::string log;
        TestStream inner{vtable.data(), nullptr, &log, 'B'};
        TestStream outer{vtable.data(), &inner, &log, 'A'};
        const auto value = static_cast<std::uint8_t>(mode);
        if (names[mode]) {
            _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(Stream(outer), value);
            Require(log == std::string("<A[B:") + names[mode] + "]A>");
            log.clear();
            outer.target = &outer;
            _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(Stream(outer), value);
            Require(log == std::string("<A[A:") + names[mode] + "]A>");
        } else {
            errno = 71;
            RequireThrows([&] { _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(Stream(outer), value); });
            Require(log.empty() && errno == 71);
        }
    }
}

void InvalidStreams() {
    const auto dump = _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE;
    RequireThrows([&] { dump(nullptr, 0x10); });
    RequireThrows([&] { dump(reinterpret_cast<PrintStream*>(1), 0x10); });
    std::string log;
    TestStream inner{vtable.data(), nullptr, &log, 'B'};
    TestStream outer{nullptr, &inner, &log, 'A'};
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    outer.vtable = reinterpret_cast<void* const*>(1);
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    outer.vtable = reinterpret_cast<void* const*>(std::numeric_limits<std::uintptr_t>::max() - 1);
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    Require(log.empty());

    auto missing = vtable;
    missing[4] = nullptr;
    outer.vtable = missing.data();
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    Require(log.empty());
    outer.vtable = vtable.data();
    outer.target = nullptr;
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    Require(log == "<A");
    log.clear();
    outer.target = &inner;
    missing = vtable;
    missing[2] = nullptr;
    inner.vtable = missing.data();
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    Require(log == "<A");
    log.clear();
    inner.vtable = vtable.data();
    missing = vtable;
    missing[5] = nullptr;
    outer.vtable = missing.data();
    RequireThrows([&] { dump(Stream(outer), 0x10); });
    Require(log == "<A[B:FastTypedArray]");
}

void UpdatedVtable() {
    std::string log;
    auto replacement = vtable;
    replacement[5] = reinterpret_cast<void*>(&ReplacementEnd);
    TestStream inner{vtable.data(), nullptr, &log, 'B'};
    TestStream outer{vtable.data(), &inner, &log, 'A', replacement.data()};
    _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(Stream(outer), 0x88);
    Require(log == "<A[B:DataViewMode]!A>");
}

}

int main() {
    AllModes();
    InvalidStreams();
    UpdatedVtable();
}
