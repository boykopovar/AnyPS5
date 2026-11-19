#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/GraphicsLayerIdVectorDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;
using LayerIds = NKWebKit::WtfAbi::Vector<std::uint64_t>;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GraphicsLayer ID vector dump check failed at line %u\n", location.line());
        std::abort();
    }
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

TextStream MakeStream(bool multiLine) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = multiLine;
    return stream;
}

std::string Dump(std::initializer_list<std::uint64_t> ids, bool multiLine = true) {
    std::vector<std::uint64_t> storage(ids);
    LayerIds layers;
    layers.buffer = storage.empty() ? nullptr : storage.data();
    layers.capacity = static_cast<unsigned>(storage.size());
    layers.size = static_cast<unsigned>(storage.size());
    TextStream stream = MakeStream(multiLine);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(&stream, &layers);
    Check(&returned == &stream);
    std::string result = Release(stream);
    layers.buffer = nullptr;
    layers.size = 0;
    return result;
}

void TestEmpty() {
    Check(Dump({}).empty());
}

void TestSingle() {
    Check(Dump({0}) == "0");
    Check(Dump({42}, false) == "42");
}

void TestSeparators() {
    Check(Dump({1, 2, 3}) == "1 2 3");
    Check(Dump({7, 0, 7}, false) == "7 0 7");
}

void TestFullRange() {
    Check(Dump({UINT64_MAX, 0x100000000ull}) == "18446744073709551615 4294967296");
}

void TestAppendsToExistingText() {
    std::uint64_t ids[] = {5, 6};
    LayerIds layers;
    layers.buffer = ids;
    layers.capacity = 4;
    layers.size = 2;
    TextStream stream = MakeStream(true);
    _ZN3WTF10TextStreamlsEPKc(&stream, "(layers ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(&stream, &layers);
    _ZN3WTF10TextStreamlsEPKc(&stream, ")");
    Check(Release(stream) == "(layers 5 6)");
    layers.buffer = nullptr;
    layers.size = 0;
}

template <typename F>
bool Throws(F function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestNullArguments() {
    LayerIds layers;
    TextStream stream = MakeStream(true);
    Check(Throws([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(nullptr, &layers); }));
    Check(Throws([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(&stream, nullptr); }));
    layers.size = 1;
    Check(Throws([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(&stream, &layers); }));
    layers.size = 0;
    Check(Release(stream).empty());
}

}  // namespace

int main() {
    TestEmpty();
    TestSingle();
    TestSeparators();
    TestFullRange();
    TestAppendsToExistingText();
    TestNullArguments();
    return 0;
}
