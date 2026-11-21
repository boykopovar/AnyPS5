extern "C" {
#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN = static_cast<int>(0x80552907);
static constexpr std::size_t ReferenceCountOffset = 0;
static constexpr std::size_t VectorReferenceCountOffset = 0x30;
static constexpr int TransactionNotStarted = static_cast<int>(0x80553508);


static void ConstructParameter(void* self);
static int InitializeParameter(void* self, const std::uint64_t* context, bool argumentsPresent);
static int InitializeParameterWithBody(void* self, const std::uint64_t* context, bool textsPresent, const IntrusivePtr* body);
static int TerminateParameter(void* self);

static constexpr int ParameterNotInitialized = static_cast<int>(0x80553502);
static constexpr int ParameterInvalidArgument = static_cast<int>(0x80553503);
static constexpr int ParameterAlreadyInitialized = static_cast<int>(0x8055350f);
static constexpr int ParameterTerminatedUninitialized = static_cast<int>(0x80553510);
static constexpr int ParameterBodyMissing = static_cast<int>(0x80553512);

struct ParameterObject {
    void* base;
    const void* implementation;
};

static constexpr unsigned char ParameterImplementation = 0;

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEppEv(CommonIterator* self) {
    self->element = static_cast<CommonString*>(self->element) + 1;
    return self;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_6StringEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8copyFromERKS5_(void* self, const void* source) {
    return HostVectorCopyFromStrings(static_cast<IntrusivePtr*>(self), static_cast<const IntrusivePtr*>(source), __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEneERKS5_(const CommonIterator* self, const CommonIterator* other) {
    return self->element != other->element;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_15DefaultResponseENS2_12IntrusivePtrINS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(CommonIterator* iterator, const IntrusivePtr* self) {
    return static_cast<CommonIterator*>(HostVectorEnd(iterator, self, 2, sizeof(CommonString), __func__));
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringeqEPKc(const CommonString* self, const char* other) {
    return other != nullptr && std::strcmp(self->buffer != nullptr ? self->buffer : "", other) == 0;
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_15DefaultResponseENS2_12IntrusivePtrINS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_15DefaultResponseENS2_12IntrusivePtrINS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(CommonIterator* iterator, const IntrusivePtr* self) {
    return static_cast<CommonIterator*>(HostVectorBegin(iterator, self, 2, __func__));
}


void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_6StringEEdeEv(const CommonIterator* self) {
    return static_cast<CommonString*>(self->element);
}


bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile19personalDetailIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x40) != 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi28ParameterToGetPublicProfiles9terminateEv() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V112BasicProfileC1EPNS1_6Common10LibContextE(void* self, void* context) {
    auto* object = static_cast<std::uint8_t*>(self);
    std::memset(object + 0x10, 0, 0x38);
    InitHostString(object + 0x48, context);
    object[0x68] = 0;
    std::memset(object + 0x70, 0, 0x40);
    object[0xb8] = 0;
    object[0xc0] = 0;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi28ParameterToGetPublicProfiles10initializeEPNS1_6Common10LibContextEPKc() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile25isOfficiallyVerifiedIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xaa) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V113BasicPresence14accountIdIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x18) != 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V113BasicPresence12setAccountIdERKm(void* self, const std::uint64_t* accountId) {
    auto* object = static_cast<std::uint8_t*>(self);
    object[0x18] = 1;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V113BasicPresenceC1EPNS1_6Common10LibContextE(void* self, void* context) {
    auto* object = static_cast<std::uint8_t*>(self);
    object[0x18] = 0;
    std::memset(object + 0x1f, 0, 4);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V114PersonalDetailEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V113BasicPresence12setInContextERKb(void* self, const bool* inContext) {
    auto* object = static_cast<std::uint8_t*>(self);
    object[0x21] = *inContext ? 1 : 0;
    object[0x22] = 1;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends9setoffsetEi() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V112BasicProfile11setOnlineIdERK13SceNpOnlineId(void* self, const void* onlineId) {
    auto* object = static_cast<std::uint8_t*>(self);
    std::memcpy(object + 0x10, onlineId, 0x10);
    object[0x24] = 1;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile17getPersonalDetailEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x28, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1ERKSB_(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile11getOnlineIdEv(void* out, const void* self) {
    std::memcpy(out, static_cast<const std::uint8_t*>(self) + 0x10, 0x10);
    return out;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEaSERKSA_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, 0x30, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail14getDisplayNameEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x88;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile13onlineIdIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x24) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V113BasicPresence14inContextIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x22) != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V114PersonalDetailEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEC1ERKS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V112BasicProfileD1Ev(void* self) {
    auto* object = static_cast<std::uint8_t*>(self);
    ReleaseIntrusivePtr(reinterpret_cast<IntrusivePtr*>(object + 0x90), StringVectorReferenceCountOffset, __func__);
    ReleaseIntrusivePtr(reinterpret_cast<IntrusivePtr*>(object + 0x70), VectorReferenceCountOffset, __func__);
    DestroyHostString(object + 0x48);
    ReleaseIntrusivePtr(reinterpret_cast<IntrusivePtr*>(object + 0x28), ReferenceCountOffset, __func__);
    return self;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEC1Ev(IntrusivePtr* self) {
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile23getIsOfficiallyVerifiedEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xa9) != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi17getPublicProfilesEiRKNS4_28ParameterToGetPublicProfilesERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_25GetPublicProfilesResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEaSERKSA_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, 0x30, __func__);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V125GetPublicProfilesResponse11getProfilesEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends8setlimitEi() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V112BasicProfile23setIsOfficiallyVerifiedERKb(void* self, const bool* verified) {
    auto* object = static_cast<std::uint8_t*>(self);
    object[0xa9] = *verified ? 1 : 0;
    object[0xaa] = 1;
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V113BasicPresence17onlineStatusIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x20) != 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi28ParameterToGetPublicProfilesD1Ev() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi28ParameterToGetPublicProfilesC1Ev() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEaSERKS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, 0x00, __func__);
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V113BasicPresenceD1Ev(void* self) {
    return self;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsers8setlimitEi() {
    return 0;
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEptEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsers9setoffsetEi() {
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V113BasicPresence15setOnlineStatusERKNS3_12OnlineStatusE(void* self, const std::uint32_t* status) {
    auto* object = static_cast<std::uint8_t*>(self);
    object[0x20] = 1;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEEaSERKSB_(void* self, const void* source) {
    auto* dst = static_cast<std::uint64_t*>(self);
    const auto* src = static_cast<const std::uint64_t*>(source);
    dst[0] = src[0];
    dst[1] = src[1];
    return self;
}

}
