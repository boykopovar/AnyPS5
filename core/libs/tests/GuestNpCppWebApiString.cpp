#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::size_t kStringSize = 0x20;
constexpr std::size_t kGuardSize = 0x10;
constexpr std::size_t kReferenceCountOffset = 0x00;
constexpr std::size_t kBufferOffset = 0x08;
constexpr std::size_t kBufferSizeOffset = 0x10;
constexpr std::size_t kLibContextOffset = 0x18;
constexpr unsigned char kFill = 0xA5;

constexpr std::size_t kVectorSize = 0x18;
constexpr std::size_t kObjectOffset = 0x00;
constexpr std::size_t kBodyMagicOffset = 0x00;
constexpr std::size_t kBodyReferenceCountOffset = 0x30;
constexpr std::size_t kBodyElementKindOffset = 0x34;
constexpr std::size_t kBodyElementsOffset = 0x38;
constexpr std::size_t kBodySizeOffset = 0x40;

struct Storage {
    alignas(8) unsigned char bytes[kStringSize + kGuardSize];
};

struct VectorStorage {
    alignas(8) unsigned char bytes[kVectorSize + kGuardSize];
};

template <typename TValue>
TValue Load(const Storage& storage, std::size_t offset) {
    TValue value;
    std::memcpy(&value, storage.bytes + offset, sizeof(value));
    return value;
}

template <typename TValue>
void Store(Storage& storage, std::size_t offset, TValue value) {
    std::memcpy(storage.bytes + offset, &value, sizeof(value));
}

template <typename TValue>
TValue LoadBody(const void* body, std::size_t offset) {
    TValue value;
    std::memcpy(&value, static_cast<const unsigned char*>(body) + offset, sizeof(value));
    return value;
}

bool GuardIntact(const Storage& storage) {
    for (std::size_t i = kStringSize; i < sizeof(storage.bytes); i++) {
        if (storage.bytes[i] != kFill) return false;
    }
    return true;
}

bool VectorGuardIntact(const VectorStorage& storage) {
    for (std::size_t i = kVectorSize; i < sizeof(storage.bytes); i++) {
        if (storage.bytes[i] != kFill) return false;
    }
    return true;
}

void TestConstructedStringIsEmpty() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(storage.bytes), &libContext);
    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kBufferSizeOffset) == 0);
    Require(Load<void*>(storage, kLibContextOffset) == &libContext);
    Require(GuardIntact(storage));

    const char* text = _ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes));
    Require(text != nullptr && text[0] == '\0');

    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(storage.bytes));
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kBufferSizeOffset) == 0);
    Require(GuardIntact(storage));
}

void TestNullLibContextIsStored() {
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(storage.bytes), nullptr);
    Require(Load<void*>(storage, kLibContextOffset) == nullptr);
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes))[0] == '\0');
    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(storage.bytes));

}

void TestAppendBuildsAndClearsBuffer() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));
    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(storage.bytes), &libContext);

    Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(storage.bytes), "rank") == 0);
    Require(std::strcmp(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes)), "rank") == 0);
    Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(storage.bytes), "ing") == 0);
    Require(std::strcmp(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes)), "ranking") == 0);
    Require(Load<char*>(storage, kBufferOffset) != nullptr);
    Require(GuardIntact(storage));

    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(storage.bytes));
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kBufferSizeOffset) == 0);
    Require(GuardIntact(storage));
}

void TestAppendNullKeepsEmpty() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(storage.bytes), &libContext);
    Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(storage.bytes), nullptr) == 0);
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes))[0] == '\0');
    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(storage.bytes));

}

void TestVectorPushBackCopiesStringContent() {
    std::uint64_t libContext = 1;
    VectorStorage vector;
    Storage element;
    std::memset(vector.bytes, kFill, sizeof(vector.bytes));
    std::memset(element.bytes, kFill, sizeof(element.bytes));

    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEEC1EPNS2_10LibContextE(reinterpret_cast<IntrusivePtr*>(vector.bytes), &libContext);
    Require(VectorGuardIntact(vector));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(element.bytes), &libContext);
    Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(element.bytes), "privilege") == 0);
    char* elementBuffer = Load<char*>(element, kBufferOffset);
    Require(elementBuffer != nullptr);

    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(reinterpret_cast<IntrusivePtr*>(vector.bytes), element.bytes) == 0);

    void* body;
    std::memcpy(&body, vector.bytes + kObjectOffset, sizeof(body));
    Require(body != nullptr);
    Require(LoadBody<std::int32_t>(body, kBodyReferenceCountOffset) == 1);
    Require(LoadBody<std::int32_t>(body, kBodyElementKindOffset) == 2);
    Require(LoadBody<std::uint64_t>(body, kBodySizeOffset) == 1);

    void* elements = LoadBody<void*>(body, kBodyElementsOffset);
    char* storedBuffer;
    std::memcpy(&storedBuffer, static_cast<unsigned char*>(elements) + kBufferOffset, sizeof(storedBuffer));
    Require(storedBuffer != nullptr);
    Require(storedBuffer != elementBuffer);
    Require(std::strcmp(storedBuffer, "privilege") == 0);

    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(element.bytes));
    Require(std::strcmp(storedBuffer, "privilege") == 0);

    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEED1Ev(reinterpret_cast<IntrusivePtr*>(vector.bytes));
    Require(VectorGuardIntact(vector));
}

void TestStringEmptyAndEquals() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(storage.bytes), &libContext);
    Require(_ZNK3sce2Np9CppWebApi6Common6String5emptyEv(reinterpret_cast<const CommonString*>(storage.bytes)) == 1);
    Require(_ZN3sce2Np9CppWebApi6Common6StringeqEPKc(reinterpret_cast<const CommonString*>(storage.bytes), "rank") == 0);

    Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(storage.bytes), "rank") == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common6String5emptyEv(reinterpret_cast<const CommonString*>(storage.bytes)) == 0);
    Require(_ZN3sce2Np9CppWebApi6Common6StringeqEPKc(reinterpret_cast<const CommonString*>(storage.bytes), "rank") == 1);
    Require(_ZN3sce2Np9CppWebApi6Common6StringeqEPKc(reinterpret_cast<const CommonString*>(storage.bytes), "rank2") == 0);
    Require(_ZN3sce2Np9CppWebApi6Common6StringeqEPKc(reinterpret_cast<const CommonString*>(storage.bytes), nullptr) == 0);
    Require(GuardIntact(storage));

    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(storage.bytes));
}

int g_elementDeleterCalls = 0;
void APS5_VABI ElementDeleter(void*) { ++g_elementDeleterCalls; }

void TestIntrusivePtrVectorPushBack() {
    std::uint64_t libContext = 1;
    VectorStorage vector;
    std::memset(vector.bytes, kFill, sizeof(vector.bytes));

    alignas(8) unsigned char element[0x18];
    alignas(8) unsigned char elementObject[0x10];
    std::memset(element, 0, sizeof(element));
    std::memset(elementObject, 0, sizeof(elementObject));
    g_elementDeleterCalls = 0;

    void* object = elementObject;
    auto* deleter = &ElementDeleter;
    std::memcpy(element + 0x00, &object, sizeof(void*));
    std::memcpy(element + 0x08, &deleter, sizeof(void*));
    std::memcpy(element + 0x10, &libContext, sizeof(void*));

    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEEC1EPNS2_10LibContextE(reinterpret_cast<IntrusivePtr*>(vector.bytes), &libContext);
    Require(VectorGuardIntact(vector));

    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEE8pushBackERKS8_(reinterpret_cast<IntrusivePtr*>(vector.bytes), element) == 0);

    void* body;
    std::memcpy(&body, vector.bytes + kObjectOffset, sizeof(body));
    Require(body != nullptr);
    Require(LoadBody<std::int32_t>(body, kBodyReferenceCountOffset) == 1);
    Require(LoadBody<std::int32_t>(body, kBodyElementKindOffset) == 1);
    Require(LoadBody<std::uint64_t>(body, kBodySizeOffset) == 1);
    Require(*reinterpret_cast<std::int32_t*>(elementObject) == 1);

    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEED1Ev(reinterpret_cast<IntrusivePtr*>(vector.bytes));
    Require(g_elementDeleterCalls == 1);
    Require(VectorGuardIntact(vector));
}

void TestOfflineResponses() {
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V130PostPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() == 0);
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi17getPlayerSessionsEiRKNS4_28ParameterToGetPlayerSessionsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_29GetPlayerSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() == static_cast<int>(0x80552907));
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi26setPlayerSessionPropertiesEiRKNS4_37ParameterToSetPlayerSessionPropertiesERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE() == static_cast<int>(0x80552907));
}

}

int main() {
    TestConstructedStringIsEmpty();
    TestNullLibContextIsStored();
    TestAppendBuildsAndClearsBuffer();
    TestAppendNullKeepsEmpty();
    TestVectorPushBackCopiesStringContent();
    TestStringEmptyAndEquals();
    TestIntrusivePtrVectorPushBack();
    TestOfflineResponses();
}
