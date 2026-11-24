#include "prx/libSceJsc/include/WtfBase64Exports.hpp"
#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <stdexcept>
#include <string>

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF base64URLDecode check failed at line %d\n", line);
        std::exit(1);
    }
}

std::set<void*> liveAllocations;
unsigned allocateCalls = 0;
unsigned deallocateCalls = 0;
std::size_t lastAllocateSize = 0;
bool failNext = false;

void* TestAllocate(std::size_t size) {
    ++allocateCalls;
    lastAllocateSize = size;
    if (failNext) {
        failNext = false;
        return nullptr;
    }
    void* ptr = std::malloc(size);
    liveAllocations.insert(ptr);
    return ptr;
}

void TestDeallocate(void* ptr) {
    ++deallocateCalls;
    Check(liveAllocations.erase(ptr) == 1, __LINE__);
    std::free(ptr);
}

const PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};

struct Text {
    StringImpl impl;
    String string;
};

Text Make8(const char* text) {
    Text value{{1, static_cast<unsigned>(std::strlen(text)), text, StringImplFlag8Bit}, {}};
    return value;
}

Text Make16(const std::u16string& text) {
    Text value{{1, static_cast<unsigned>(text.size()), text.data(), 0}, {}};
    return value;
}

bool Decode(Text text, CharVector& vector, bool isSigned = false) {
    text.string.impl = &text.impl;
    return _ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE(&text.string, {&vector, isSigned});
}

bool Holds(const CharVector& vector, const char* expected) {
    const std::size_t length = std::strlen(expected);
    return vector.size == length && (!length || std::memcmp(vector.buffer, expected, length) == 0);
}

template<typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void Release(CharVector& vector) {
    if (vector.buffer) TestDeallocate(vector.buffer);
    vector = {};
}

}

int main() {
    RegisterPersistenceAllocator_nid_no_patch(&allocator);

    {
        const struct {
            const char* encoded;
            const char* decoded;
        } cases[] = {
            {"Zg", "f"},
            {"Zg==", "f"},
            {"Zm8", "fo"},
            {"Zm8=", "fo"},
            {"Zm9v", "foo"},
            {"Zm9vYg", "foob"},
            {"Zm9vYmE", "fooba"},
            {"Zm9vYmFy", "foobar"},
            {"aGVsbG8sIHdvcmxkIQ", "hello, world!"},
            {"Zg=", "f"},
            {"Zg===", "f"},
        };
        for (const auto& item : cases) {
            CharVector vector{};
            Check(Decode(Make8(item.encoded), vector), __LINE__);
            Check(Holds(vector, item.decoded), __LINE__);
            const std::size_t length = std::strlen(item.encoded);
            Check(vector.capacity == (length > 16 ? length : 16), __LINE__);
            Release(vector);
        }
    }

    {
        CharVector vector{};
        Check(Decode(Make8("-_-_"), vector), __LINE__);
        const std::uint8_t expected[] = {0xFB, 0xFF, 0xBF};
        Check(vector.size == 3 && std::memcmp(vector.buffer, expected, 3) == 0, __LINE__);
        Release(vector);

        Check(!Decode(Make8("+/+/"), vector), __LINE__);
        Check(vector.size == 0 && vector.capacity == 16, __LINE__);
        Release(vector);
    }

    {
        CharVector vector{};
        const std::string longText(40, 'A');
        Check(Decode(Make8(longText.c_str()), vector), __LINE__);
        Check(vector.size == 30 && vector.capacity == 40, __LINE__);
        for (unsigned i = 0; i < 30; ++i) Check(vector.buffer[i] == 0, __LINE__);
        Release(vector);
    }

    {
        CharVector vector{};
        Check(!Decode(Make8("Zm9v YmFy"), vector), __LINE__);
        const std::uint8_t sextets[] = {0x19, 0x26, 0x3D, 0x2F};
        Check(vector.size == 4 && std::memcmp(vector.buffer, sextets, 4) == 0, __LINE__);
        Release(vector);

        Check(!Decode(Make8("Zm9v\nYmFy"), vector), __LINE__);
        Release(vector);
        Check(!Decode(Make8("Zg=A"), vector), __LINE__);
        Check(vector.size == 2, __LINE__);
        Release(vector);
        Check(!Decode(Make8("Z"), vector), __LINE__);
        Check(vector.size == 1, __LINE__);
        Release(vector);
        Check(!Decode(Make8("Zm9vY"), vector), __LINE__);
        Release(vector);
        Check(!Decode(Make8("=="), vector), __LINE__);
        Check(vector.size == 0 && vector.capacity == 16, __LINE__);
        Release(vector);
    }

    {
        CharVector vector{};
        Check(Decode(Make16(u"Zm9vYmFy"), vector, true), __LINE__);
        Check(Holds(vector, "foobar"), __LINE__);
        Release(vector);

        Check(!Decode(Make16(u"Z\u0141=="), vector), __LINE__);
        Check(vector.size == 1, __LINE__);
        Release(vector);
    }

    {
        CharVector vector{};
        vector.buffer = static_cast<std::uint8_t*>(TestAllocate(64));
        vector.capacity = 64;
        vector.size = 7;
        void* old = vector.buffer;
        const unsigned freedBefore = deallocateCalls;
        Check(Decode(Make8(""), vector), __LINE__);
        Check(deallocateCalls == freedBefore + 1 && !liveAllocations.count(old), __LINE__);
        Check(!vector.buffer && vector.capacity == 0 && vector.size == 0, __LINE__);

        Text nullString{};
        Check(_ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE(&nullString.string, {&vector, false}), __LINE__);
        Check(!vector.buffer && vector.size == 0, __LINE__);
    }

    {
        CharVector vector{};
        failNext = true;
        Check(Throws<std::bad_alloc>([&] { Decode(Make8("Zm9v"), vector); }), __LINE__);
        Check(!vector.buffer && vector.size == 0, __LINE__);
        Check(Throws<std::invalid_argument>([&] {
            _ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE(nullptr, {&vector, false});
        }), __LINE__);
        Text text = Make8("Zm9v");
        text.string.impl = &text.impl;
        Check(Throws<std::invalid_argument>([&] {
            _ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE(&text.string, {nullptr, false});
        }), __LINE__);
    }

    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    {
        CharVector vector{};
        Check(Decode(Make8(""), vector), __LINE__);
        Check(Throws<std::runtime_error>([&] { Decode(Make8("Zm9v"), vector); }), __LINE__);
        Check(!vector.buffer && vector.size == 0, __LINE__);
    }

    Check(liveAllocations.empty(), __LINE__);
    std::puts("jsc_wtf_base64: all checks passed");
    return 0;
}
