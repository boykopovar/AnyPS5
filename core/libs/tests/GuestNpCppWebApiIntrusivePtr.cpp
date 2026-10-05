#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using Deleter = void (APS5_VABI*)(void*);

struct IntrusivePtr {
    void* object;
    Deleter deleter;
    void* libContext;
};

using Construct = void (APS5_VABI*)(IntrusivePtr*);
using Copy = void (APS5_VABI*)(IntrusivePtr*, const IntrusivePtr*);
using Destroy = void (APS5_VABI*)(IntrusivePtr*);
using Access = void* (APS5_VABI*)(const IntrusivePtr*);

extern "C" {
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEC1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEC1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEC1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEC1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEC1Ev(IntrusivePtr*);

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1ERS7_(IntrusivePtr*, const IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1ERS7_(IntrusivePtr*, const IntrusivePtr*);

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEED1Ev(IntrusivePtr*);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEED1Ev(IntrusivePtr*);

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V15EntryEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEptEv(const IntrusivePtr*);
void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEEdeEv(const IntrusivePtr*);
}

static void Require(bool value) { if (!value) std::abort(); }

static constexpr std::size_t ObjectSize = 0x40;
static constexpr std::size_t VectorReferenceCountOffset = 0x30;

struct DestroyCase {
    Destroy destroy;
    std::size_t referenceCountOffset;
};

static void* deletedObject = nullptr;
static int deleterCalls = 0;

static void APS5_VABI RecordDelete(void* object) {
    deletedObject = object;
    ++deleterCalls;
}

static std::int32_t ReadCount(const unsigned char* object, const std::size_t offset) {
    std::int32_t value;
    std::memcpy(&value, object + offset, sizeof(value));
    return value;
}

static void WriteCount(unsigned char* object, const std::size_t offset, const std::int32_t value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

static bool OnlyCountChanged(const unsigned char* object, const std::size_t offset) {
    for (std::size_t index = 0; index < ObjectSize; ++index) {
        if (index >= offset && index < offset + sizeof(std::int32_t)) continue;
        if (object[index] != 0x5a) return false;
    }
    return true;
}

static void TestConstruct(const Construct construct) {
    IntrusivePtr pointer;
    std::memset(&pointer, 0xa5, sizeof(pointer));
    construct(&pointer);
    Require(pointer.object == nullptr);
    Require(pointer.deleter == nullptr);
    Require(pointer.libContext == nullptr);
}

static void TestAccess(const Access access) {
    int object = 0;
    int context = 0;
    const IntrusivePtr empty{nullptr, RecordDelete, &context};
    Require(access(&empty) == nullptr);
    const IntrusivePtr filled{&object, nullptr, nullptr};
    Require(access(&filled) == &object);
}

static void TestCopy(const Copy copy) {
    alignas(8) unsigned char object[ObjectSize];
    std::memset(object, 0x5a, sizeof(object));
    WriteCount(object, 0, 1);
    int context = 0;

    const IntrusivePtr source{object, RecordDelete, &context};
    IntrusivePtr target;
    std::memset(&target, 0xa5, sizeof(target));
    copy(&target, &source);
    Require(target.object == object);
    Require(target.deleter == RecordDelete);
    Require(target.libContext == &context);
    Require(source.object == object);
    Require(ReadCount(object, 0) == 2);
    Require(OnlyCountChanged(object, 0));

    const IntrusivePtr empty{nullptr, RecordDelete, &context};
    std::memset(&target, 0xa5, sizeof(target));
    copy(&target, &empty);
    Require(target.object == nullptr);
    Require(target.deleter == RecordDelete);
    Require(target.libContext == &context);
}

static void TestDestroy(const DestroyCase& testCase) {
    alignas(8) unsigned char object[ObjectSize];
    std::memset(object, 0x5a, sizeof(object));
    int context = 0;
    const std::size_t offset = testCase.referenceCountOffset;

    deleterCalls = 0;
    deletedObject = nullptr;

    IntrusivePtr empty{nullptr, RecordDelete, &context};
    testCase.destroy(&empty);
    Require(empty.object == nullptr);
    Require(empty.deleter == RecordDelete);
    Require(empty.libContext == &context);
    Require(deleterCalls == 0);

    WriteCount(object, offset, 2);
    IntrusivePtr shared{object, RecordDelete, &context};
    testCase.destroy(&shared);
    Require(ReadCount(object, offset) == 1);
    Require(shared.object == object);
    Require(shared.deleter == RecordDelete);
    Require(shared.libContext == &context);
    Require(deleterCalls == 0);

    IntrusivePtr last{object, RecordDelete, &context};
    testCase.destroy(&last);
    Require(ReadCount(object, offset) == 0);
    Require(OnlyCountChanged(object, offset));
    Require(deleterCalls == 1);
    Require(deletedObject == object);
    Require(last.object == nullptr);
    Require(last.deleter == nullptr);
    Require(last.libContext == &context);

    WriteCount(object, offset, 1);
    IntrusivePtr pooled{object, nullptr, &context};
    bool thrown = false;
    try {
        testCase.destroy(&pooled);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(deleterCalls == 1);
}

static void TestSharedOwnership() {
    alignas(8) unsigned char object[ObjectSize];
    std::memset(object, 0x5a, sizeof(object));
    WriteCount(object, 0, 1);
    deleterCalls = 0;
    deletedObject = nullptr;

    IntrusivePtr owner;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1Ev(&owner);
    owner.object = object;
    owner.deleter = RecordDelete;

    IntrusivePtr argument;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1ERS7_(&argument, &owner);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv(&argument) == object);
    Require(ReadCount(object, 0) == 2);

    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev(&argument);
    Require(ReadCount(object, 0) == 1);
    Require(deleterCalls == 0);

    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev(&owner);
    Require(ReadCount(object, 0) == 0);
    Require(deleterCalls == 1);
    Require(deletedObject == object);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv(&owner) == nullptr);
}

int main() {
    Require(sizeof(IntrusivePtr) == 0x18);

    const Construct constructors[] = {
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEC1Ev,
    };
    for (const auto construct : constructors) TestConstruct(construct);

    const Access accessors[] = {
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V15EntryEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEEdeEv,
    };
    for (const auto access : accessors) TestAccess(access);

    const Copy copies[] = {
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1ERS7_,
    };
    for (const auto copy : copies) TestCopy(copy);

    const DestroyCase destructors[] = {
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEED1Ev, VectorReferenceCountOffset},
    };
    for (const auto& testCase : destructors) TestDestroy(testCase);

    TestSharedOwnership();
    return 0;
}
