#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <source_location>

using Deleter = void (APS5_VABI*)(void*);

using Construct = void (APS5_VABI*)(IntrusivePtr*);
using Copy = void (APS5_VABI*)(IntrusivePtr*, const IntrusivePtr*);
using Assign = void (APS5_VABI*)(IntrusivePtr*, const IntrusivePtr*);
using Destroy = void (APS5_VABI*)(IntrusivePtr*);
using Access = void* (APS5_VABI*)(const IntrusivePtr*);

static void Require(bool value) { if (!value) std::abort(); }

static constexpr std::size_t ObjectSize = 0x40;
static constexpr std::size_t VectorReferenceCountOffset = 0x30;

struct DestroyCase {
    Destroy destroy;
    std::size_t referenceCountOffset;
};

struct AssignCase {
    Assign assign;
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

static void TestAssign(const AssignCase& testCase) {
    const std::size_t offset = testCase.referenceCountOffset;
    alignas(8) unsigned char first[ObjectSize];
    alignas(8) unsigned char second[ObjectSize];
    std::memset(first, 0x5a, sizeof(first));
    std::memset(second, 0x5a, sizeof(second));
    WriteCount(first, offset, 1);
    WriteCount(second, offset, 1);
    int context = 0;
    deleterCalls = 0;
    deletedObject = nullptr;

    IntrusivePtr pointer{first, RecordDelete, &context};
    testCase.assign(&pointer, &pointer);
    Require(pointer.object == first && pointer.deleter == RecordDelete && pointer.libContext == &context);
    Require(ReadCount(first, offset) == 1);

    const IntrusivePtr alias{first, nullptr, nullptr};
    testCase.assign(&pointer, &alias);
    Require(pointer.object == first && pointer.deleter == RecordDelete && pointer.libContext == &context);
    Require(ReadCount(first, offset) == 1);
    Require(deleterCalls == 0);

    const IntrusivePtr other{second, RecordDelete, &context};
    testCase.assign(&pointer, &other);
    Require(pointer.object == second);
    Require(ReadCount(second, offset) == 2);
    Require(ReadCount(first, offset) == 0);
    Require(deleterCalls == 1 && deletedObject == first);
    Require(OnlyCountChanged(first, offset) && OnlyCountChanged(second, offset));
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

static void TestVectorPushBackReleasesElements() {
    int context = 0;
    alignas(8) unsigned char object[ObjectSize];
    std::memset(object, 0x5a, sizeof(object));
    WriteCount(object, 0, 1);
    deleterCalls = 0;
    deletedObject = nullptr;

    IntrusivePtr vector;
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEEC1EPNS2_10LibContextE(&vector, &context);

    Require(vector.object == nullptr);
    Require(vector.deleter == nullptr);
    Require(vector.libContext == &context);

    const IntrusivePtr empty{nullptr, nullptr, &context};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEE8pushBackERKS8_(&vector, &empty) == 0);
    Require(vector.object != nullptr);
    Require(vector.deleter != nullptr);

    IntrusivePtr filled{object, RecordDelete, &context};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEE8pushBackERKS8_(&vector, &filled) == 0);
    Require(ReadCount(object, 0) == 2);

    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEED1Ev(&vector);
    Require(vector.object == nullptr);
    Require(vector.deleter == nullptr);
    Require(ReadCount(object, 0) == 1);
    Require(deleterCalls == 0);

    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEED1Ev(&filled);
    Require(ReadCount(object, 0) == 0);
    Require(deleterCalls == 1);
    Require(deletedObject == object);
}

static void TestAdoptedVectorBodyDestroyedThroughDeleter() {
    int context = 0;
    alignas(8) unsigned char body[ObjectSize];
    std::memset(body, 0x5a, sizeof(body));
    WriteCount(body, VectorReferenceCountOffset, 1);
    deleterCalls = 0;
    deletedObject = nullptr;

    IntrusivePtr vector;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEC1EPS6_PFvS8_EPNS2_10LibContextE(&vector, body, RecordDelete, &context);

    Require(vector.object == body);
    Require(vector.deleter == RecordDelete);
    Require(vector.libContext == &context);

    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEED1Ev(&vector);
    Require(ReadCount(body, VectorReferenceCountOffset) == 0);
    Require(deleterCalls == 1);
    Require(deletedObject == body);
    Require(vector.object == nullptr);
}

static void TestVectorAliasedGrowth() {
    unsigned cases = 0, moves = 0;
    for (unsigned sourceIndex = 0; sourceIndex < 4; ++sourceIndex) {
        int context = 0;
        alignas(8) unsigned char object[ObjectSize];
        std::memset(object, 0x5a, sizeof(object));
        WriteCount(object, 0, 1);
        deleterCalls = 0;
        deletedObject = nullptr;
        IntrusivePtr vector{};
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEEC1EPNS2_10LibContextE(&vector, &context);
        IntrusivePtr owner{object, RecordDelete, &context};
        for (unsigned index = 0; index < 4; ++index)
            Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEE8pushBackERKS8_(&vector, &owner) == 0);
        for (unsigned size = 4; size < 129; ++size) {
            IntrusivePtr* before = nullptr;
            std::memcpy(&before, static_cast<const unsigned char*>(vector.object) + 0x38, sizeof(before));
            const auto oldAddress = reinterpret_cast<std::uintptr_t>(before);
            Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEE8pushBackERKS8_(&vector, &before[sourceIndex]) == 0);
            IntrusivePtr* after = nullptr;
            std::memcpy(&after, static_cast<const unsigned char*>(vector.object) + 0x38, sizeof(after));
            moves += reinterpret_cast<std::uintptr_t>(after) != oldAddress;
            Require(after[size].object == object && after[size].deleter == RecordDelete && after[size].libContext == &context);
            Require(ReadCount(object, 0) == static_cast<std::int32_t>(size + 2));
            Require(OnlyCountChanged(object, 0));
            ++cases;
        }
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEED1Ev(&vector);
        Require(ReadCount(object, 0) == 1 && deleterCalls == 0);
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEED1Ev(&owner);
        Require(ReadCount(object, 0) == 0 && deleterCalls == 1 && deletedObject == object);
    }
    std::printf("aliased intrusive vector: %u insertions, %u moved growths\n", cases, moves);
}

static void TestUserProfileVectorAssign() {
    int context = 0;
    alignas(8) unsigned char first[ObjectSize];
    alignas(8) unsigned char second[ObjectSize];
    std::memset(first, 0x5a, sizeof(first));
    std::memset(second, 0x5a, sizeof(second));
    WriteCount(first, VectorReferenceCountOffset, 1);
    WriteCount(second, VectorReferenceCountOffset, 7);
    deleterCalls = 0;
    deletedObject = nullptr;

    IntrusivePtr target{first, RecordDelete, &context};
    const IntrusivePtr source{second, RecordDelete, &context};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEaSERKSA_(&target, &source);
    Require(target.object == second && target.deleter == RecordDelete && target.libContext == &context);
    Require(ReadCount(first, VectorReferenceCountOffset) == 0 && ReadCount(second, VectorReferenceCountOffset) == 8);
    Require(deleterCalls == 1 && deletedObject == first);

    IntrusivePtr empty{nullptr, RecordDelete, &context};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEaSERKSA_(&empty, &source);
    Require(empty.object == second && ReadCount(second, VectorReferenceCountOffset) == 9);
    Require(OnlyCountChanged(second, VectorReferenceCountOffset));

    IntrusivePtr presenceTarget{nullptr, RecordDelete, &context};
    const IntrusivePtr presenceSource{second, RecordDelete, &context};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEaSERKSA_(&presenceTarget, &presenceSource);
    Require(presenceTarget.object == second && presenceTarget.deleter == RecordDelete && presenceTarget.libContext == &context);
    Require(ReadCount(second, VectorReferenceCountOffset) == 10);
    Require(deleterCalls == 1);
}

static void TestUserProfileModelAndTransaction() {
    Require(_ZN3sce2Np9CppWebApi11UserProfile2V112BasicProfileC1EPNS1_6Common10LibContextE() == 0);

    std::uint64_t transaction[0x20];
    std::memset(transaction, 0xa5, sizeof(transaction));
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(transaction) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() == 0);
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(transaction) == 0);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "alias-vector") == 0) { TestVectorAliasedGrowth(); return 0; }
    Require(sizeof(IntrusivePtr) == 0x18);

    const Construct constructors[] = {
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V219WebApiFilterRequestEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V132GetMultiDataStatusesResponseBodyEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEC1Ev,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEC1Ev,
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
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEE3getEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEE3getEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEdeEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEEdeEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V57ProductEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V132GetMultiDataStatusesResponseBodyEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V15OwnerEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V53SkuEEEEEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V57ProductEEEEEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEdeEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_17TitleCloudStorage2V110DataStatusEEEEEEdeEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEptEv,
        _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEptEv,
    };
    for (const auto access : accessors) TestAccess(access);

    const Copy copies[] = {
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V219WebApiFilterRequestEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V57ProductEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEC1ERS7_,
        _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEC1ERKS7_,
    };
    for (const auto copy : copies) TestCopy(copy);

    const AssignCase assignments[] = {
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEaSERS7_, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V113PlayerSessionEEEEEEaSERKSA_, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEaSERKSA_, VectorReferenceCountOffset},
    };
    for (const auto& testCase : assignments) TestAssign(testCase);

    const DestroyCase destructors[] = {
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEED1Ev, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V219WebApiFilterRequestEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEED1Ev, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V57ProductEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V59ContainerEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V132GetMultiDataStatusesResponseBodyEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V15OwnerEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V53SkuEEEEEED1Ev, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V57ProductEEEEEED1Ev, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEED1Ev, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_17TitleCloudStorage2V110DataStatusEEEEEED1Ev, VectorReferenceCountOffset},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEED1Ev, 0},
        {_ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEED1Ev, VectorReferenceCountOffset},
    };
    for (const auto& testCase : destructors) TestDestroy(testCase);

    TestSharedOwnership();
    TestVectorPushBackReleasesElements();
    TestAdoptedVectorBodyDestroyedThroughDeleter();
    TestUserProfileVectorAssign();
    TestUserProfileModelAndTransaction();
    return 0;
}
