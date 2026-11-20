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

// Offline PSN: request entry points (API calls, transaction start/readData, factories) fail with
// SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN, the constant shadPS4/fpPS4 document for this code; parameter
// bookkeeping succeeds; the audited accessors read the measured offsets and the rest stay
// unimplemented because a failed request never produces a response.

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

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption() {
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V16AvatarEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V112BasicProfileEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V16AvatarEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE(void* self, const std::uint64_t* context) {
    (void)self;
    if (context == nullptr) return ParameterInvalidArgument;
    if (*context == 0) return ParameterNotInitialized;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(const void* self, IntrusivePtr* response) {
    (void)response;
    if (!TransactionStarted(self)) return TransactionNotStarted;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEEC1ERKS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V125FriendJoinedPlayerSessionEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V125FriendJoinedPlayerSessionEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V125FriendJoinedPlayerSessionEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V125FriendJoinedPlayerSessionEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, FriendVectorReferenceCount, __func__);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V125FriendJoinedPlayerSessionEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, FriendVectorReferenceCount, __func__);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V125FriendJoinedPlayerSessionEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V125FriendJoinedPlayerSessionEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

const char* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V16Friend12getAccountIdEv(const void* self) {
    const char* value = nullptr;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x10, sizeof(value));
    return value;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V16Friend14accountIdIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x18] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V16Friend19playerSessionsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x48] != 0;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V125FriendJoinedPlayerSession12getSessionIdEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V125FriendJoinedPlayerSession14sessionIdIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x30] != 0;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE4sizeEv(const void* self) {
    return FriendVectorSize(self);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE5emptyEv(const void* self) {
    return FriendVectorReadPointer(self, FriendVectorBegin) == FriendVectorReadPointer(self, FriendVectorEnd);
}

CommonConstIterator APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE5beginEv(const void* self) {
    return FriendVectorIterator(self, FriendVectorBegin);
}

CommonConstIterator APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE3endEv(const void* self) {
    return FriendVectorIterator(self, FriendVectorEnd);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEixEm(const void* self, const std::uint64_t index) {
    return FriendVectorElement(const_cast<void*>(self), index);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V125FriendJoinedPlayerSessionEEEEixEm(void* self, const std::uint64_t index) {
    return FriendVectorElement(self, index);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEED1Ev(void* self) {
    (void)self;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEppEv(void* self) {
    auto* iterator = static_cast<CommonConstIterator*>(self);
    iterator->element = static_cast<std::uint8_t*>(iterator->element) + FriendVectorElementSize;
    return iterator;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEdeEv(const void* self) {
    return static_cast<const CommonConstIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEneERKS9_(const void* self, const void* other) {
    return static_cast<const CommonConstIterator*>(self)->element !=
           static_cast<const CommonConstIterator*>(other)->element;
}
void APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEEC1Ev(void* self) {
    std::memset(self, 0, 0x10);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEEC1Ev(void* self) {
    std::memset(self, 0, 0x10);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE5startEPNS2_10LibContextE(void* self, void* context) {
    (void)self;
    (void)context;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE5startEPNS2_10LibContextE(void* self, void* context) {
    (void)self;
    (void)context;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE6finishEv(void* self) {
    (void)self;
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common19UpStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE6finishEv(void* self) {
    (void)self;
    return TransactionNotStarted;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE11hasResponseEv(const void* self) {

    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V18LocationEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V18LocationEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V19SubmitterEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V113PlayerForReadEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V118PlayerForOfferReadEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V19AttributeEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEEppEv(void* self) {
    auto* iterator = static_cast<CommonConstIterator*>(self);
    iterator->element = static_cast<std::uint8_t*>(iterator->element) + sizeof(IntrusivePtr);
    return iterator;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEE3endEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, true, __func__);
    return iterator;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEE5beginEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, false, __func__);
    return iterator;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEE3endEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, true, __func__);
    return iterator;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEE5beginEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, false, __func__);
    return iterator;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(const void* self, void* response) {
    (void)self;
    (void)response;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv(const void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1ERKSB_(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V132GetMultiDataStatusesResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv(const void* self) {

    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(const void* self, void* response) {
    (void)self;
    (void)response;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv(const void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE11hasResponseEv(const void* self) {

    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(const void* self, void* response) {
    (void)self;
    (void)response;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv(const void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_15DefaultResponseENS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEEE11hasResponseEv(const void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi25UploadDataResponseHeadersEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEptEv(const IntrusivePtr* self) {
    return self->object;
}

}
