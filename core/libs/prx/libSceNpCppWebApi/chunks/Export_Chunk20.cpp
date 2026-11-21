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

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124GameSessionMemberForReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V111GameSessionEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V113PlayerSessionEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V117GameSessionPlayerEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V118GameSessionForReadEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V120PlayerSessionForReadEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEEE5emptyEv(const void* self) {
    return HostVectorSize(self, __func__) == 0;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V111GameSessionEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends8setorderENS5_5OrderE(void* self, int value) {
    (void)self;
    (void)value;
}

void APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends9setfilterENS5_6FilterE(void* self, int value) {
    (void)self;
    (void)value;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V112BasicProfile10getAvatarsEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x70, VectorReferenceCountOffset);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V114PersonalDetail18getProfilePicturesEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0xb0, VectorReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V16AvatarEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V16AvatarEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEEppEv(void* self) {
    auto* iterator = static_cast<CommonConstIterator*>(self);
    iterator->element = static_cast<std::uint8_t*>(iterator->element) + sizeof(IntrusivePtr);
    return iterator;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEixEm(void* self, std::uint64_t index) {
    return HostVectorElement(self, index, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEEixEm(void* self, std::uint64_t index) {
    return HostVectorElement(self, index, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile10getAboutMeEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x48;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile12aboutMeIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x68] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V112BasicProfile12avatarsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x88] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail20profilePicturesIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0xc8] != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V118GetFriendsResponse13getNextOffsetEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x50);
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V118GetFriendsResponse17getTotalItemCountEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x48);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V124GetPublicProfileResponse17getPersonalDetailEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x48, ReferenceCountOffset);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V124GetPublicProfileResponse19personalDetailIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x60] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V16Avatar6getUrlEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x18;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V16Avatar7getSizeEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V16Avatar8urlIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x38] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V16Avatar9sizeIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x14] != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V114PersonalDetailEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V16AvatarEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V16AvatarEEEEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V16AvatarEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V16AvatarEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEEdeEv(const void* self) {
    return static_cast<const CommonConstIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEEneERKS9_(const void* self, const void* other) {
    return static_cast<const CommonConstIterator*>(self)->element !=
           static_cast<const CommonConstIterator*>(other)->element;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEixEm(const void* self, std::uint64_t index) {
    return HostVectorElement(const_cast<void*>(self), index, __func__);
}

CommonConstIterator APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE3endEv(const void* self) {
    const VectorBody* body = HostVectorBody(self, 1, __func__);
    return CommonConstIterator(0, body != nullptr && body->elements != nullptr ? static_cast<std::uint8_t*>(body->elements) + body->size * sizeof(IntrusivePtr) : nullptr);
}

CommonConstIterator APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE5beginEv(const void* self) {
    const VectorBody* body = HostVectorBody(self, 1, __func__);
    return CommonConstIterator(0, body != nullptr ? body->elements : nullptr);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

}
