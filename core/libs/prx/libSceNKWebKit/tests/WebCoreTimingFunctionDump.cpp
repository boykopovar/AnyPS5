#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/TimingFunctionDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;
using Type = WebCore::TimingFunction::Type;
using StepPosition = WebCore::StepsTimingFunction::StepPosition;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "TimingFunction dump check failed at line %u\n", location.line());
        std::abort();
    }
}

int typeCalls = 0;

template <Type type>
Type APS5_VABI TypeOf(const WebCore::TimingFunction*) {
    ++typeCalls;
    return type;
}

template <Type type>
struct Vtable {
    static inline void* const slots[5] = {nullptr, nullptr, nullptr, reinterpret_cast<void*>(&TypeOf<type>), nullptr};
};

TextStream NewStream(unsigned flags) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = true;
    stream.formattingFlags = flags;
    stream.indent = 3;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

template <typename T>
std::string Dump(const T& function, unsigned flags = 0) {
    TextStream stream = NewStream(flags);
    int before = typeCalls;
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_14TimingFunctionE(stream, reinterpret_cast<const WebCore::TimingFunction&>(function));
    Check(&returned == &stream);
    Check(typeCalls == before + 1);
    Check(stream.indent == 3);
    return Release(stream);
}

constexpr unsigned NumberRespectingIntegers = 1u << 1;

WebCore::StepsTimingFunction Steps(int steps, StepPosition position, bool hasPosition) {
    return {Vtable<Type::StepsFunction>::slots, 1, steps, position, hasPosition};
}

void TestLinear() {
    WebCore::TimingFunction linear{Vtable<Type::LinearFunction>::slots, 1};
    Check(Dump(linear) == "linear");
}

void TestCubicBezier() {
    WebCore::CubicBezierTimingFunction ease{Vtable<Type::CubicBezierFunction>::slots, 1, 0.25, 0.1, 0.25, 1.0, 0};
    Check(Dump(ease) == "cubic-bezier(0.25, 0.10, 0.25, 1.00)");
    Check(Dump(ease, NumberRespectingIntegers) == "cubic-bezier(0.25, 0.10, 0.25, 1)");
}

void TestSteps() {
    Check(Dump(Steps(4, StepPosition::JumpStart, false)) == "steps(4)");
    Check(Dump(Steps(-2, StepPosition::End, false)) == "steps(-2)");
    Check(Dump(Steps(1, StepPosition::JumpStart, true)) == "steps(1, jump-start)");
    Check(Dump(Steps(2, StepPosition::JumpEnd, true)) == "steps(2, jump-end)");
    Check(Dump(Steps(3, StepPosition::JumpNone, true)) == "steps(3, jump-none)");
    Check(Dump(Steps(5, StepPosition::JumpBoth, true)) == "steps(5, jump-both)");
    Check(Dump(Steps(6, StepPosition::Start, true)) == "steps(6, start)");
    Check(Dump(Steps(7, StepPosition::End, true)) == "steps(7, end)");
}

void TestSpring() {
    WebCore::SpringTimingFunction spring{Vtable<Type::SpringFunction>::slots, 1, 1.0, 100.0, 10.0, 0.5};
    Check(Dump(spring) == "spring(1.00 100.00 10.00 0.50)");
    Check(Dump(spring, NumberRespectingIntegers) == "spring(1 100 10 0.50)");
}

void TestUnknownTypeWritesNothing() {
    WebCore::TimingFunction unknown{Vtable<static_cast<Type>(4)>::slots, 1};
    Check(Dump(unknown).empty());
}

void TestAppendsToExistingText() {
    TextStream stream = NewStream(NumberRespectingIntegers);
    _ZN3WTF10TextStreamlsEPKc(&stream, "timing function ");
    auto steps = Steps(3, StepPosition::JumpNone, true);
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_14TimingFunctionE(stream, reinterpret_cast<const WebCore::TimingFunction&>(steps));
    _ZN3WTF10TextStreamlsEPKc(&stream, "!");
    Check(Release(stream) == "timing function steps(3, jump-none)!");
}

}  // namespace

int main() {
    TestLinear();
    TestCubicBezier();
    TestSteps();
    TestSpring();
    TestUnknownTypeWritesNothing();
    TestAppendsToExistingText();
    return 0;
}
