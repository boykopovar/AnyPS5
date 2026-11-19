#include "prx/libSceNKWebKit/include/JscBytecodeCacheErrorExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JscBytecodeCacheError check failed at line %u\n", location.line());
        std::abort();
    }
}

BytecodeCacheError Filled(std::uint8_t fill, std::uint8_t index) {
    BytecodeCacheError error;
    std::memset(&error, fill, sizeof(error));
    error.index = index;
    return error;
}

bool IsValid(const BytecodeCacheError& error) {
    return _ZNK3JSC18BytecodeCacheError7isValidEv(&error);
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

void TestDefaultParserErrorIsInvalid() {
    BytecodeCacheError error = Filled(0, 0);
    error.storage.parser.line = -1;
    Check(!IsValid(error));
}

void TestParserErrorTypesAreValid() {
    for (unsigned type = 1; type < 256; ++type) {
        BytecodeCacheError error = Filled(0, 0);
        error.storage.parser.type = static_cast<std::uint8_t>(type);
        Check(IsValid(error));
    }
}

void TestParserErrorReadsOnlyType() {
    BytecodeCacheError error = Filled(0xFF, 0);
    error.storage.parser.type = 0;
    Check(!IsValid(error));
    error.storage.parser.syntaxErrorType = 0;
    error.storage.parser.line = 0;
    error.storage.parser.message = nullptr;
    Check(!IsValid(error));
}

void TestStandardErrorIsValid() {
    BytecodeCacheError error = Filled(0, 1);
    error.storage.standardErrno = 0;
    Check(IsValid(error));
    error.storage.standardErrno = 2;
    Check(IsValid(error));
}

void TestWriteErrorIsValid() {
    BytecodeCacheError error = Filled(0, 2);
    Check(IsValid(error));
    error.storage.write.written = 3;
    error.storage.write.expected = 9;
    Check(IsValid(error));
}

void TestIndexIgnoresParserTypeBytes() {
    BytecodeCacheError standard = Filled(0, 1);
    Check(IsValid(standard));
    BytecodeCacheError write = Filled(0, 2);
    Check(IsValid(write));
}

void TestValuelessOrCorruptIndexThrows() {
    for (unsigned index : {3u, 0x7Fu, 0xFFu}) {
        BytecodeCacheError error = Filled(0, static_cast<std::uint8_t>(index));
        Check(Throws([&] { IsValid(error); }));
    }
}

void TestNullThrows() {
    Check(Throws([] { _ZNK3JSC18BytecodeCacheError7isValidEv(nullptr); }));
}

}  // namespace

int main() {
    TestDefaultParserErrorIsInvalid();
    TestParserErrorTypesAreValid();
    TestParserErrorReadsOnlyType();
    TestStandardErrorIsValid();
    TestWriteErrorIsValid();
    TestIndexIgnoresParserTypeBytes();
    TestValuelessOrCorruptIndexThrows();
    TestNullThrows();
    return 0;
}
