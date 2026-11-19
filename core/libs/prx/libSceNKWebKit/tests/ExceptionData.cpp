#include "prx/libSceNKWebKit/include/ExceptionDataExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::ExceptionAbi::ExceptionData;
using NKWebKit::WtfAbi::String;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ExceptionData check failed at line %u\n", location.line());
        std::abort();
    }
}

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

bool Is8Bit(const String& string) {
    return string.impl->hashAndFlags & 4;
}

std::u16string Units(const String& string) {
    Check(string.impl != nullptr);
    std::u16string units;
    for (unsigned i = 0; i < string.impl->length; ++i) {
        units.push_back(Is8Bit(string) ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i]);
    }
    return units;
}

ExceptionData Copy(const ExceptionData& data) {
    return _ZNK7WebCore13ExceptionData12isolatedCopyEv(&data);
}

void TestEightBitMessage() {
    const ExceptionData source {9, Make("The string did not match the expected pattern.")};
    const auto references = source.message.impl->refCount;
    const ExceptionData copy = Copy(source);
    Check(copy.code == 9);
    Check(copy.message.impl != source.message.impl);
    Check(copy.message.impl->refCount == 2);
    Check(source.message.impl->refCount == references);
    Check(Is8Bit(copy.message));
    Check(Units(copy.message) == u"The string did not match the expected pattern.");
}

void TestSixteenBitMessage() {
    const ExceptionData source {34, Make16(u"café ☃")};
    const ExceptionData copy = Copy(source);
    Check(copy.code == 34);
    Check(copy.message.impl != source.message.impl);
    Check(!Is8Bit(copy.message));
    Check(Units(copy.message) == u"café ☃");
}

void TestNullMessage() {
    const ExceptionData source {37, String()};
    const ExceptionData copy = Copy(source);
    Check(copy.code == 37);
    Check(copy.message.impl == nullptr);
}

void TestEmptyMessage() {
    const ExceptionData source {0, Make("")};
    const ExceptionData copy = Copy(source);
    Check(copy.code == 0);
    Check(copy.message.impl && copy.message.impl->length == 0);
}

void TestCodeIsCopiedUnchanged() {
    for (const int code : {-1, 1, 33, 0x7fffffff}) {
        const ExceptionData source {code, Make("m")};
        Check(Copy(source).code == code);
    }
}

void TestCopyOutlivesSource() {
    ExceptionData copy {};
    {
        const ExceptionData source {24, Make("transient")};
        copy = Copy(source);
    }
    Check(copy.code == 24);
    Check(copy.message.impl->refCount == 2);
    Check(Units(copy.message) == u"transient");
}

void TestNullSelfThrows() {
    bool thrown = false;
    try {
        _ZNK7WebCore13ExceptionData12isolatedCopyEv(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Check(thrown);
}

}  // namespace

int main() {
    TestEightBitMessage();
    TestSixteenBitMessage();
    TestNullMessage();
    TestEmptyMessage();
    TestCodeIsCopiedUnchanged();
    TestCopyOutlivesSource();
    TestNullSelfThrows();
    std::puts("ExceptionData: isolatedCopy checks passed");
    return 0;
}
