#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError;
using NKWebKit::JscBytecodeCacheErrorAbi::StandardError;
using NKWebKit::JscBytecodeCacheErrorAbi::WriteError;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC::BytecodeCacheError check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static BytecodeCacheError Filled(std::uint32_t index) {
    BytecodeCacheError error;
    std::memset(&error, 0xA5, sizeof(error));
    error.index = index;
    return error;
}

static BytecodeCacheError Parser(std::uint8_t type, StringImpl* message) {
    BytecodeCacheError error = Filled(0);
    error.parserError.message = message;
    error.parserError.type = type;
    return error;
}

static bool BytesFrom(const BytecodeCacheError& error, std::size_t begin, std::size_t end, unsigned char value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&error);
    for (std::size_t offset = begin; offset < end; ++offset) {
        if (bytes[offset] != value) return false;
    }
    return true;
}

static bool Same(const BytecodeCacheError& left, const BytecodeCacheError& right) {
    return std::memcmp(&left, &right, sizeof(BytecodeCacheError)) == 0;
}

int main() {
    {
        BytecodeCacheError none = Parser(0, nullptr);
        Require(!_ZNK3JSC18BytecodeCacheError7isValidEv(&none));
        for (std::uint8_t type = 1; type <= 4; ++type) {
            BytecodeCacheError error = Parser(type, nullptr);
            Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&error));
        }
        BytecodeCacheError standard = Filled(1);
        standard.standardError.error = 0;
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&standard));
        BytecodeCacheError write = Filled(2);
        write.writeError = WriteError{0, 0};
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&write));
        BytecodeCacheError valueless = Filled(0xFFFFFFFFu);
        Require(Throws([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(&valueless); }));
        BytecodeCacheError invalid = Filled(3);
        Require(Throws([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(&invalid); }));
        Require(Throws([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(nullptr); }));
    }
    {
        StringImpl message{4, 0, nullptr, 0};
        BytecodeCacheError error = Parser(4, &message);
        StandardError standard{28};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, &standard) == &error);
        Require(message.refCount == 2);
        Require(error.index == 1);
        Require(error.standardError.error == 28);
        Require(BytesFrom(error, 4, 72, 0xA5));
        Require(error.parserError.message == &message);
        Require(BytesFrom(error, 80, 84, 0xA5));
        Require(error.parserError.type == 4);
        Require(BytesFrom(error, 85, 88, 0xA5));
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&error));

        StandardError other{-5};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, &other) == &error);
        Require(message.refCount == 2);
        Require(error.index == 1);
        Require(error.standardError.error == -5);

        WriteError write{0x1122334455667788ull, 0x99aabbccddeeff00ull};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, &write) == &error);
        Require(message.refCount == 2);
        Require(error.index == 2);
        Require(error.writeError.written == 0x1122334455667788ull);
        Require(error.writeError.expected == 0x99aabbccddeeff00ull);
        Require(BytesFrom(error, 16, 72, 0xA5));
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&error));

        WriteError next{7, 9};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, &next) == &error);
        Require(error.index == 2);
        Require(error.writeError.written == 7);
        Require(error.writeError.expected == 9);

        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, &standard) == &error);
        Require(error.index == 1);
        Require(error.standardError.error == 28);
        Require(error.writeError.expected == 9);
        Require(message.refCount == 2);
    }
    {
        StringImpl message{6, 0, nullptr, 0};
        BytecodeCacheError error = Parser(0, &message);
        WriteError write{3, 4};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, &write) == &error);
        Require(message.refCount == 4);
        Require(error.index == 2);
        Require(error.writeError.written == 3);
        Require(error.writeError.expected == 4);
    }
    {
        BytecodeCacheError nullMessage = Parser(0, nullptr);
        StandardError standard{1};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&nullMessage, &standard) == &nullMessage);
        Require(nullMessage.index == 1);
        Require(nullMessage.standardError.error == 1);

        BytecodeCacheError valueless = Filled(0xFFFFFFFFu);
        WriteError write{11, 12};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&valueless, &write) == &valueless);
        Require(valueless.index == 2);
        Require(valueless.writeError.written == 11);
        Require(valueless.writeError.expected == 12);
        Require(BytesFrom(valueless, 16, 88, 0xA5));

        BytecodeCacheError valuelessStandard = Filled(0xFFFFFFFFu);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&valuelessStandard, &standard) == &valuelessStandard);
        Require(valuelessStandard.index == 1);
        Require(valuelessStandard.standardError.error == 1);
        Require(BytesFrom(valuelessStandard, 4, 88, 0xA5));
    }
    {
        StandardError standard{2};
        WriteError write{1, 2};
        BytecodeCacheError invalid = Filled(3);
        BytecodeCacheError before = invalid;
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&invalid, &standard); }));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&invalid, &write); }));
        Require(Same(invalid, before));
        BytecodeCacheError error = Parser(0, nullptr);
        BytecodeCacheError original = error;
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, nullptr); }));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, nullptr); }));
        Require(Same(error, original));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(nullptr, &standard); }));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(nullptr, &write); }));
    }
    return 0;
}
