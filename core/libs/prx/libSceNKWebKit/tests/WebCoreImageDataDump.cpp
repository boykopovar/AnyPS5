#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ImageDataDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ImageData dump check failed at line %u\n", location.line());
        std::abort();
    }
}

TextStream NewStream(bool multiLine, unsigned flags, int indent) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = multiLine;
    stream.formattingFlags = flags;
    stream.indent = indent;
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

WebCore::ImageData MakeImageData(std::uintptr_t data, int width, int height) {
    WebCore::ImageData imageData;
    imageData.m_size = {width, height};
    imageData.m_data = reinterpret_cast<JSC::Uint8ClampedArray*>(data);
    return imageData;
}

std::string Dump(const WebCore::ImageData& imageData, bool multiLine = true, unsigned flags = 0, int indent = 0) {
    TextStream stream = NewStream(multiLine, flags, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE(stream, imageData);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

std::string DumpPointer(const void* pointer) {
    TextStream stream = NewStream(true, 0, 0);
    _ZN3WTF10TextStreamlsEPKv(&stream, pointer);
    return Release(stream);
}

void TestWritesDataArrayAddress() {
    Check(Dump(MakeImageData(0x1234abcd, 4, 2)) == "1234abcd");
    Check(Dump(MakeImageData(0x7f00deadbeef, 1, 1)) == "7f00deadbeef");
}

void TestMatchesPointerWriter() {
    unsigned char storage[16]{};
    const auto address = reinterpret_cast<std::uintptr_t>(&storage[8]);
    Check(Dump(MakeImageData(address, 2, 2)) == DumpPointer(&storage[8]));
}

void TestIgnoresSizeColorSpaceAndRefCount() {
    WebCore::ImageData imageData = MakeImageData(0xabc0, 640, 480);
    imageData.m_refCount = 7;
    imageData.m_colorSpace = 1;
    Check(Dump(imageData) == "abc0");
}

void TestNoIndentOrLineBreak() {
    Check(Dump(MakeImageData(0x10, 1, 1), true, 0, 3) == "10");
    Check(Dump(MakeImageData(0x10, 1, 1), false, 2, 0) == "10");
}

void TestAppendsToExistingText() {
    TextStream stream = NewStream(false, 0, 0);
    _ZN3WTF10TextStreamlsEPKc(&stream, "data ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE(stream, MakeImageData(0xff00, 1, 1));
    _ZN3WTF10TextStreamlsEPKc(&stream, "!");
    Check(Release(stream) == "data ff00!");
}

}  // namespace

int main() {
    TestWritesDataArrayAddress();
    TestMatchesPointerWriter();
    TestIgnoresSizeColorSpaceAndRefCount();
    TestNoIndentOrLineBreak();
    TestAppendsToExistingText();
    return 0;
}
