#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

struct TestAsn1Object {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

namespace {

constexpr int Dynamic = 0x01;
constexpr int DynamicStrings = 0x04;
constexpr int DynamicData = 0x08;

const unsigned char* wipedData;
std::size_t wipedLength;
bool wipeChecked;
bool tailChecked;
int freeCalls;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceLibreSsl3 ASN1_OBJECT_free failed: %s\n", message);
    std::abort();
}

void* Allocate(std::size_t size) {
    void* pointer = std::malloc(size);
    if (!pointer) std::abort();
    std::memset(pointer, 0xA5, size);
    return pointer;
}

TestAsn1Object* AllocateObject(int flags, int length, const unsigned char* data, const char* shortName, const char* longName) {
    auto* object = static_cast<TestAsn1Object*>(Allocate(sizeof(TestAsn1Object)));
    object->shortName = shortName;
    object->longName = longName;
    object->nid = 7;
    object->length = length;
    object->data = data;
    object->flags = flags;
    return object;
}

void Reset() {
    wipedData = nullptr;
    wipedLength = 0;
    wipeChecked = false;
    tailChecked = false;
    freeCalls = 0;
}

void TestNullAndStaticObject() {
    Reset();
    ASN1_OBJECT_free_nid_postfix(nullptr);
    unsigned char data[] {1, 2, 3};
    TestAsn1Object object {"sn", "ln", 7, 3, data, 0x02};
    ASN1_OBJECT_free_nid_postfix(&object);
    Require(freeCalls == 0, "null and static objects do not free memory");
    Require(data[0] == 1 && data[1] == 2 && data[2] == 3, "static OID data remains unchanged");
}

void TestDynamicObjectOnly() {
    Reset();
    auto* object = AllocateObject(Dynamic, 0, nullptr, "sn", "ln");
    ASN1_OBJECT_free_nid_postfix(object);
    Require(freeCalls == 1, "dynamic object frees only its allocation");
}

void TestDynamicStringsAndData() {
    Reset();
    auto* shortName = static_cast<char*>(Allocate(3));
    auto* longName = static_cast<char*>(Allocate(4));
    auto* data = static_cast<unsigned char*>(Allocate(6));
    data[4] = 0x71;
    data[5] = 0x82;
    wipedData = data;
    wipedLength = 4;
    auto* object = AllocateObject(Dynamic | DynamicStrings | DynamicData, 4, data, shortName, longName);
    ASN1_OBJECT_free_nid_postfix(object);
    Require(freeCalls == 4, "dynamic object frees both names, OID data, and itself");
    Require(wipeChecked, "OID bytes are zeroed before their allocation is freed");
    Require(tailChecked, "OID free leaves bytes beyond the encoded data length unchanged");
}

void TestDynamicDataWithoutDynamicObject() {
    Reset();
    auto* data = static_cast<unsigned char*>(Allocate(2));
    wipedData = data;
    wipedLength = 2;
    TestAsn1Object object {nullptr, nullptr, 7, 2, data, DynamicData};
    ASN1_OBJECT_free_nid_postfix(&object);
    Require(freeCalls == 1 && wipeChecked, "dynamic data is erased and freed independently of the object");
    Require(object.data == nullptr && object.length == 0, "freed data fields are cleared");
}

void TestNegativeDataLength() {
    Reset();
    auto* data = static_cast<unsigned char*>(Allocate(4));
    auto* object = AllocateObject(Dynamic | DynamicData, -1, data, nullptr, nullptr);
    try {
        ASN1_OBJECT_free_nid_postfix(object);
    } catch (const std::invalid_argument&) {
        Require(freeCalls == 0, "invalid data length is rejected before any allocation is freed");
        std::free(data);
        std::free(object);
        return;
    }
    Require(false, "negative length with data must throw");
}

void TestNegativeLengthWithoutData() {
    Reset();
    auto* object = AllocateObject(Dynamic | DynamicData, -1, nullptr, nullptr, nullptr);
    ASN1_OBJECT_free_nid_postfix(object);
    Require(freeCalls == 1, "negative length is harmless when no data allocation exists");
}

}

extern "C" {

void APS5_VABI free_nid_postfix(void* pointer) {
    if (pointer == wipedData) {
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        for (std::size_t index = 0; index < wipedLength; ++index)
            if (bytes[index] != 0) std::abort();
        wipeChecked = true;
        if (wipedLength == 4) tailChecked = bytes[4] == 0x71 && bytes[5] == 0x82;
    }
    ++freeCalls;
    std::free(pointer);
}

int APS5_VABI memset_s_nid_postfix(void* destination, std::size_t destinationSize, int value, std::size_t count) {
    if (!destination || value != 0 || destinationSize != count) return 22;
    auto* bytes = static_cast<volatile unsigned char*>(destination);
    for (std::size_t index = 0; index < count; ++index) bytes[index] = 0;
    return 0;
}

}

int main() {
    static_assert(sizeof(TestAsn1Object) == 40);
    TestNullAndStaticObject();
    TestDynamicObjectOnly();
    TestDynamicStringsAndData();
    TestDynamicDataWithoutDynamicObject();
    TestNegativeDataLength();
    TestNegativeLengthWithoutData();
    std::puts("libSceLibreSsl3 ASN1_OBJECT_free tests passed");
}
