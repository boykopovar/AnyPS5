#include "prx/libSceNKWebKit/include/WtfPrintfVariableFormatExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using Jsc::WtfAbi::PrintStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WtfPrintfVariableFormat check failed at line %u\n", location.line());
        std::abort();
    }
}

struct CaptureStream {
    const void* const* vtable;
    std::string text;
    int calls = 0;
    const char* lastFormat = nullptr;
};

void APS5_VABI CaptureVprintf(PrintStream* stream, const char* format, VaList* args) {
    auto* capture = reinterpret_cast<CaptureStream*>(stream);
    char buffer[256];
    const int written = vsnprintf_nid_postfix(buffer, sizeof(buffer), format, args);
    Check(written >= 0 && static_cast<std::size_t>(written) < sizeof(buffer));
    capture->text.append(buffer, static_cast<std::size_t>(written));
    capture->lastFormat = format;
    ++capture->calls;
}

void APS5_VABI ThrowingVprintf(PrintStream*, const char*, VaList*) {
    throw std::runtime_error("vprintf failed");
}

void APS5_VABI Unused(PrintStream*) {
    std::abort();
}

const void* const CaptureVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&CaptureVprintf),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
};

const void* const ThrowingVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&ThrowingVprintf),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
};

const void* const MissingVprintfVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
};

PrintStream* AsStream(CaptureStream& stream) {
    return reinterpret_cast<PrintStream*>(&stream);
}

template <typename TException, typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

void PassesTheFormatAndArgumentsToTheVirtualVprintf() {
    CaptureStream stream{CaptureVtable + 2};
    const std::string format = "%s=%d";
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), format.c_str(), "answer", 42);
    Check(stream.calls == 1);
    Check(stream.lastFormat == format.c_str());
    Check(stream.text == "answer=42");
}

void ForwardsMixedIntegerAndFloatingArguments() {
    CaptureStream stream{CaptureVtable + 2};
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), "%d %.2f %lld %u %.3f %s %c %llu %.1f %x",
        -7, 1.5, static_cast<long long>(-9000000000LL), 4000000000U, 2.25, "tail", 'z', static_cast<unsigned long long>(18446744073709551615ULL), -0.5, 0xbeefU);
    Check(stream.text == "-7 1.50 -9000000000 4000000000 2.250 tail z 18446744073709551615 -0.5 beef");
}

void ForwardsArgumentsBeyondTheRegisterSaveArea() {
    CaptureStream stream{CaptureVtable + 2};
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), "%d,%d,%d,%d,%d,%d,%d,%d|%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f",
        1, 2, 3, 4, 5, 6, 7, 8, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    Check(stream.text == "1,2,3,4,5,6,7,8|1,2,3,4,5,6,7,8,9,10");
}

void AppendsAcrossCallsWithoutArguments() {
    CaptureStream stream{CaptureVtable + 2};
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), "plain");
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), " 100%%");
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), "");
    Check(stream.calls == 3);
    Check(stream.text == "plain 100%");
}

void DispatchesThroughEachStreamsOwnVtable() {
    CaptureStream first{CaptureVtable + 2};
    CaptureStream second{CaptureVtable + 2};
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(first), "%d", 1);
    _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(second), "%d", 2);
    Check(first.text == "1" && second.text == "2");
    Check(first.calls == 1 && second.calls == 1);
}

void PropagatesExceptionsFromTheVirtualVprintf() {
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(stream), "%d", 1); }));
}

void RejectsANullStreamVtableOrVprintf() {
    Check(Throws<std::invalid_argument>([] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(nullptr, "%d", 1); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(noVtable), "%d", 1); }));
    CaptureStream noVprintf{MissingVprintfVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(AsStream(noVprintf), "%d", 1); }));
    Check(noVtable.calls == 0 && noVprintf.calls == 0);
}

} // namespace

int main() {
    PassesTheFormatAndArgumentsToTheVirtualVprintf();
    ForwardsMixedIntegerAndFloatingArguments();
    ForwardsArgumentsBeyondTheRegisterSaveArea();
    AppendsAcrossCallsWithoutArguments();
    DispatchesThroughEachStreamsOwnVtable();
    PropagatesExceptionsFromTheVirtualVprintf();
    RejectsANullStreamVtableOrVprintf();
    std::puts("WtfPrintfVariableFormat tests passed");
    return 0;
}
