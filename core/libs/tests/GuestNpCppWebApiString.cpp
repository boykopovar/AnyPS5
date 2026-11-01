#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <source_location>
#include <string>

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

constexpr std::size_t kStringSize = 0x20;
constexpr std::size_t kGuardSize = 0x10;
constexpr std::size_t kReferenceCountOffset = 0x00;
constexpr std::size_t kBufferOffset = 0x08;
constexpr std::size_t kBufferSizeOffset = 0x10;
constexpr std::size_t kLibContextOffset = 0x18;
constexpr unsigned char kFill = 0xA5;

constexpr std::size_t kVectorSize = 0x48;
constexpr std::size_t kVectorReferenceCountOffset = 0x38;
constexpr std::size_t kObjectOffset = 0x00;
constexpr std::size_t kBodyMagicOffset = 0x00;
constexpr std::size_t kBodyReferenceCountOffset = 0x30;
constexpr std::size_t kBodyElementKindOffset = 0x34;
constexpr std::size_t kBodyElementsOffset = 0x38;
constexpr std::size_t kBodySizeOffset = 0x40;

constexpr std::size_t kIteratorSize = 0x10;
constexpr std::size_t kIteratorReferenceCountOffset = 0x00;
constexpr std::size_t kIteratorElementOffset = 0x08;

struct Storage {
    alignas(8) unsigned char bytes[kStringSize + kGuardSize];
};

struct VectorStorage {
    alignas(8) unsigned char bytes[kVectorSize + kGuardSize];
};

struct IteratorStorage {
    alignas(8) unsigned char bytes[kIteratorSize + kGuardSize];
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

bool IteratorGuardIntact(const IteratorStorage& storage) {
    for (std::size_t i = kIteratorSize; i < sizeof(storage.bytes); i++) {
        if (storage.bytes[i] != kFill) return false;
    }
    return true;
}

template <typename TValue>
TValue LoadIterator(const IteratorStorage& storage, std::size_t offset) {
    TValue value;
    std::memcpy(&value, storage.bytes + offset, sizeof(value));
    return value;
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
    std::int32_t referenceCount;
    std::memcpy(&referenceCount, vector.bytes + kVectorReferenceCountOffset, sizeof(referenceCount));
    Require(referenceCount == 0);

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

void TestVectorIterationVisitsStringsInOrder() {
    std::uint64_t libContext = 1;
    VectorStorage vector;
    IteratorStorage first;
    IteratorStorage last;
    std::memset(vector.bytes, kFill, sizeof(vector.bytes));
    std::memset(first.bytes, kFill, sizeof(first.bytes));
    std::memset(last.bytes, kFill, sizeof(last.bytes));
    auto* self = reinterpret_cast<IntrusivePtr*>(vector.bytes);
    auto* firstIterator = reinterpret_cast<CommonIterator*>(first.bytes);
    auto* lastIterator = reinterpret_cast<CommonIterator*>(last.bytes);

    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEEC1EPNS2_10LibContextE(self, &libContext);
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(firstIterator, self) == firstIterator);
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(lastIterator, self) == lastIterator);
    Require(!_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEneERKS5_(firstIterator, lastIterator));

    const char* const texts[] = {"first", "second", "third"};
    for (const char* text : texts) {
        Storage element;
        std::memset(element.bytes, kFill, sizeof(element.bytes));
        _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(element.bytes), &libContext);
        Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(element.bytes), text) == 0);
        Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(self, element.bytes) == 0);
        _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(element.bytes));
    }

    std::memset(first.bytes, kFill, sizeof(first.bytes));
    std::memset(last.bytes, kFill, sizeof(last.bytes));
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(firstIterator, self);
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(lastIterator, self);
    Require(LoadIterator<std::int32_t>(first, kIteratorReferenceCountOffset) == 0);
    Require(LoadIterator<std::int32_t>(last, kIteratorReferenceCountOffset) == 0);
    Require(LoadIterator<unsigned char*>(last, kIteratorElementOffset) - LoadIterator<unsigned char*>(first, kIteratorElementOffset) == 3 * kStringSize);
    Require(IteratorGuardIntact(first) && IteratorGuardIntact(last));

    std::size_t visited = 0;
    while (_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEneERKS5_(firstIterator, lastIterator)) {
        Require(visited < 3);
        const CommonString* element = _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEdeEv(firstIterator);
        Require(static_cast<const void*>(element) == LoadIterator<void*>(first, kIteratorElementOffset));
        Require(std::strcmp(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(element), texts[visited]) == 0);
        Require(_ZN3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEppEv(firstIterator) == firstIterator);
        ++visited;
    }
    Require(visited == 3);
    Require(IteratorGuardIntact(first));

    _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_6StringEED2Ev(firstIterator);
    _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_6StringEED2Ev(lastIterator);
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEED1Ev(self);
    Require(VectorGuardIntact(vector));
}

bool BeginThrows(const IntrusivePtr* vector) {
    IteratorStorage iterator;
    try {
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(reinterpret_cast<CommonIterator*>(iterator.bytes), vector);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool EndThrows(const IntrusivePtr* vector) {
    IteratorStorage iterator;
    try {
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(reinterpret_cast<CommonIterator*>(iterator.bytes), vector);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestVectorIterationRejectsOtherBodies() {
    std::uint64_t libContext = 1;
    VectorStorage vector;
    std::memset(vector.bytes, kFill, sizeof(vector.bytes));
    auto* self = reinterpret_cast<IntrusivePtr*>(vector.bytes);
    const IntrusivePtr empty{nullptr, nullptr, &libContext};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEEC1EPNS2_10LibContextE(self, &libContext);
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEE8pushBackERKS8_(self, &empty) == 0);
    Require(BeginThrows(self));
    Require(EndThrows(self));
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEED1Ev(self);
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

unsigned TestAppendAliased() {
    constexpr std::size_t lengths[] = {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129};
    unsigned cases = 0, failures = 0, grown = 0;
    int context = 0;
    for (const auto length : lengths) {
        std::string initial(length, 'a');
        for (std::size_t index = 0; index < length; ++index) initial[index] = static_cast<char>('a' + index % 26);
        for (std::size_t offset = 0; offset <= length; ++offset) {
            Storage storage;
            std::memset(storage.bytes, kFill, sizeof(storage.bytes));
            _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(storage.bytes), &context);
            Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(storage.bytes), initial.c_str()) == 0);
            const char* source = _ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes)) + offset;
            const std::string expected = initial + initial.substr(offset);
            const auto oldCapacity = Load<std::uint64_t>(storage, kBufferSizeOffset);
            Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(storage.bytes), source) == 0);
            const auto capacity = Load<std::uint64_t>(storage, kBufferSizeOffset);
            grown += capacity != oldCapacity;
            ++cases;
            if (std::memcmp(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(reinterpret_cast<const CommonString*>(storage.bytes)), expected.c_str(), expected.size() + 1) != 0) {
                if (failures == 0) std::fprintf(stderr, "aliased append corrupted length=%zu offset=%zu\n", length, offset);
                ++failures;
            }
            Require(GuardIntact(storage));
            Require(Load<void*>(storage, kLibContextOffset) == &context);
            Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
            _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(storage.bytes));
            Require(GuardIntact(storage));
        }
    }
    std::printf("aliased append: %u cases, %u growths, %u corrupt results\n", cases, grown, failures);
    Require(grown != 0);
    return failures;
}

unsigned TestVectorAliased() {
    unsigned cases = 0, moves = 0;
    for (unsigned sourceIndex = 0; sourceIndex < 4; ++sourceIndex) {
        int context = 0;
        VectorStorage vector;
        Storage element;
        std::memset(vector.bytes, kFill, sizeof(vector.bytes));
        std::memset(element.bytes, kFill, sizeof(element.bytes));
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEEC1EPNS2_10LibContextE(reinterpret_cast<IntrusivePtr*>(vector.bytes), &context);
        _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(reinterpret_cast<CommonString*>(element.bytes), &context);
        Require(_ZN3sce2Np9CppWebApi6Common6String6appendEPKc(reinterpret_cast<CommonString*>(element.bytes), "independent string payload") == 0);
        for (unsigned index = 0; index < 4; ++index)
            Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(reinterpret_cast<IntrusivePtr*>(vector.bytes), element.bytes) == 0);
        void* body = LoadBody<void*>(vector.bytes, kObjectOffset);
        for (unsigned size = 4; size < 129; ++size) {
            auto* before = LoadBody<unsigned char*>(body, kBodyElementsOffset);
            const auto oldAddress = reinterpret_cast<std::uintptr_t>(before);
            const void* source = before + sourceIndex * kStringSize;
            Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(reinterpret_cast<IntrusivePtr*>(vector.bytes), source) == 0);
            auto* after = LoadBody<unsigned char*>(body, kBodyElementsOffset);
            moves += reinterpret_cast<std::uintptr_t>(after) != oldAddress;
            Require(LoadBody<std::uint64_t>(body, kBodySizeOffset) == size + 1);
            const char* original = LoadBody<char*>(after + sourceIndex * kStringSize, kBufferOffset);
            const char* copy = LoadBody<char*>(after + size * kStringSize, kBufferOffset);
            Require(original != copy && std::strcmp(copy, "independent string payload") == 0);
            Require(LoadBody<void*>(after + size * kStringSize, kLibContextOffset) == &context);
            Require(LoadBody<std::int32_t>(after + size * kStringSize, kReferenceCountOffset) == 0);
            ++cases;
        }
        _ZN3sce2Np9CppWebApi6Common6StringD1Ev(reinterpret_cast<CommonString*>(element.bytes));
        const auto* elements = LoadBody<unsigned char*>(body, kBodyElementsOffset);
        for (unsigned index = 0; index < 129; ++index) {
            Require(std::strcmp(LoadBody<char*>(elements + index * kStringSize, kBufferOffset), "independent string payload") == 0);
        }
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEED1Ev(reinterpret_cast<IntrusivePtr*>(vector.bytes));
        Require(GuardIntact(element) && VectorGuardIntact(vector));
    }
    std::printf("aliased string vector: %u insertions, %u moved growths\n", cases, moves);
    return 0;
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "alias-append") == 0) return TestAppendAliased() ? 1 : 0;
    if (argc == 2 && std::strcmp(argv[1], "alias-vector") == 0) return TestVectorAliased();
    TestConstructedStringIsEmpty();
    TestNullLibContextIsStored();
    TestAppendBuildsAndClearsBuffer();
    TestAppendNullKeepsEmpty();
    TestVectorPushBackCopiesStringContent();
    TestVectorIterationVisitsStringsInOrder();
    TestVectorIterationRejectsOtherBodies();
    TestStringEmptyAndEquals();
    TestIntrusivePtrVectorPushBack();
    TestOfflineResponses();
}
