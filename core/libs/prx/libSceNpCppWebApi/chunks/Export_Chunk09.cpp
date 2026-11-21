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

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}


void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE5emptyEv(const void* self) {
    return HostVectorSize(self, __func__) == 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V143PutPlayerSessionsSessionIdLeaderRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V139PatchPlayerSessionsSessionIdRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V139PatchPlayerSessionsSessionIdRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V113PlayerSessionEEEEEEaSERKSA_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, 0x30, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V12ToEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V113PlayerSessionEEEEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V149PostPlayerSessionsSessionIdInvitationsRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEEEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V130RequestPlayerSessionInvitationEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V160PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V12ToEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorImE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V152PostPlayerSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEEixEm(IntrusivePtr* self, unsigned long index) {
    return HostVectorElement(self, index, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V143PutPlayerSessionsSessionIdLeaderRequestBodyEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V130PostPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V139PatchPlayerSessionsSessionIdRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V139PatchPlayerSessionsSessionIdRequestBodyEEC1Ev(IntrusivePtr* self) {
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V133ResponsePlayerSessionMemberPlayerEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V130RequestPlayerSessionInvitationEEC1Ev(IntrusivePtr* self) {
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V133ResponsePlayerSessionMemberPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V130RequestPlayerSessionInvitationEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorImEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118LeaderWithOnlineIdEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V130PostPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorImEneERKS4_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V120PlayerSessionForReadEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V130RequestPlayerSessionInvitationEEEE8pushBackERKS8_(IntrusivePtr* self, const void* element) {
    return VectorPushBackIntrusivePtr(self, static_cast<const IntrusivePtr*>(element));
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V152PostPlayerSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V149PostPlayerSessionsSessionIdInvitationsRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V130RequestPlayerSessionInvitationEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V152PostPlayerSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5getIdEv() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEE8copyFromERKS9_(void* self, const void* source) {
    return HostVectorCopyFrom(static_cast<IntrusivePtr*>(self), static_cast<const IntrusivePtr*>(source), __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V113PlayerSessionEEEEEEC1Ev(IntrusivePtr* self) {
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku10labelIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x80) != 0;
}

struct MatchesParameter {
    void* base;
    void* implementation;
};

static constexpr int MatchesOutOfMemory = static_cast<int>(0x80553504);

static CommonString* MatchesStringAt(void* implementation, const std::size_t offset) {
    return reinterpret_cast<CommonString*>(static_cast<std::uint8_t*>(implementation) + offset);
}

static void ConstructMatchesString(CommonString* self, void* libContext) {
    self->referenceCount = 0;
    self->buffer = nullptr;
    self->bufferSize = 0;
    self->libContext = libContext;
}

static void DestroyMatchesString(CommonString* self) {
    std::free(self->buffer);
    self->buffer = nullptr;
    self->bufferSize = 0;
}

static int AssignMatchesString(CommonString* self, const char* text) {
    if (text == nullptr) return ParameterInvalidArgument;
    const std::size_t length = std::strlen(text);
    char* const oldBuffer = self->buffer;
    char* const buffer = static_cast<char*>(std::malloc(length + 1));
    if (buffer == nullptr) {
        self->buffer = oldBuffer;
        return MatchesOutOfMemory;
    }
    std::memcpy(buffer, text, length + 1);
    self->buffer = buffer;
    self->bufferSize = length + 1;
    std::free(oldBuffer);
    return 0;
}

static int InitializeMatchesParameter(void* self, const std::uint64_t* context, const bool hasText, const char* text, const std::size_t implementationSize, const std::size_t bodyOffset, const std::size_t contextOffset, const IntrusivePtr* body) {
    if (context == nullptr) return ParameterInvalidArgument;
    if (*context == 0) return ParameterNotInitialized;
    auto* parameter = static_cast<MatchesParameter*>(self);
    if (parameter->implementation != nullptr) return ParameterAlreadyInitialized;
    void* implementation = std::calloc(1, implementationSize);
    if (implementation == nullptr) return MatchesOutOfMemory;
    void* libContext = const_cast<std::uint64_t*>(context);
    if (hasText) ConstructMatchesString(MatchesStringAt(implementation, 0), libContext);
    if (hasText) {
        const int result = AssignMatchesString(MatchesStringAt(implementation, 0), text);
        if (result < 0) {
            DestroyMatchesString(MatchesStringAt(implementation, 0));
            std::free(implementation);
            return result;
        }
    }
    if (body == nullptr || body->object == nullptr) {
        if (hasText) DestroyMatchesString(MatchesStringAt(implementation, 0));
        std::free(implementation);
        return ParameterBodyMissing;
    }
    auto* destination = reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(implementation) + bodyOffset);
    ReferenceCount(destination->object, ReferenceCountOffset).fetch_add(1);
    parameter->implementation = implementation;
    return 0;
}

static int TerminateMatchesParameter(void* self, const bool hasText, const std::size_t bodyOffset) {
    auto* parameter = static_cast<MatchesParameter*>(self);
    if (parameter->implementation == nullptr) return ParameterTerminatedUninitialized;
    void* implementation = parameter->implementation;
    if (hasText) DestroyMatchesString(MatchesStringAt(implementation, 0));
    ReleaseIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(implementation) + bodyOffset), ReferenceCountOffset, __func__);
    std::free(implementation);
    parameter->implementation = nullptr;
    return 0;
}

static int InitializeGetMatchDetailParameter(void* self, const std::uint64_t* context, const char* text) {
    if (context == nullptr) return ParameterInvalidArgument;
    if (*context == 0) return ParameterNotInitialized;
    auto* parameter = static_cast<MatchesParameter*>(self);
    if (parameter->implementation != nullptr) return ParameterAlreadyInitialized;
    void* implementation = std::calloc(1, 0x38);
    if (implementation == nullptr) return MatchesOutOfMemory;
    void* libContext = const_cast<std::uint64_t*>(context);
    ConstructMatchesString(MatchesStringAt(implementation, 0), libContext);
    const int result = AssignMatchesString(MatchesStringAt(implementation, 0), text);
    if (result < 0) {
        DestroyMatchesString(MatchesStringAt(implementation, 0));
        std::free(implementation);
        return result;
    }
    parameter->implementation = implementation;
    return 0;
}

static int TerminateGetMatchDetailParameter(void* self) {
    auto* parameter = static_cast<MatchesParameter*>(self);
    if (parameter->implementation == nullptr) return ParameterTerminatedUninitialized;
    DestroyMatchesString(MatchesStringAt(parameter->implementation, 0));
    std::free(parameter->implementation);
    parameter->implementation = nullptr;
    return 0;
}

static void APS5_VABI DestroyMatchesModel(void* object) {
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelString16(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelStrings16And48(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    DestroyMatchesString(MatchesStringAt(object, 48));
    std::free(object);
}

static void APS5_VABI DestroyRequestMatchPlayer(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    DestroyMatchesString(MatchesStringAt(object, 56));
    DestroyMatchesString(MatchesStringAt(object, 96));
    std::free(object);
}

static void APS5_VABI DestroyAddedPlayer(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    DestroyMatchesString(MatchesStringAt(object, 56));
    DestroyMatchesString(MatchesStringAt(object, 96));
    DestroyMatchesString(MatchesStringAt(object, 136));
    std::free(object);
}

static void ReleaseMatchesMember(void* object, const std::size_t offset, const std::size_t referenceCountOffset) {
    ReleaseIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(object) + offset), referenceCountOffset, __func__);
}

static void APS5_VABI DestroyCreateMatchRequest(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 24));
    DestroyMatchesString(MatchesStringAt(object, 56));
    ReleaseMatchesMember(object, 152, ReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyRequestMatchTeam(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    DestroyMatchesString(MatchesStringAt(object, 48));
    ReleaseMatchesMember(object, 88, VectorReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyRequestTeamStatistic(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    ReleaseMatchesMember(object, 48, VectorReferenceCountOffset);
    ReleaseMatchesMember(object, 80, VectorReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelVectors16And48(void* object) {
    ReleaseMatchesMember(object, 16, VectorReferenceCountOffset);
    ReleaseMatchesMember(object, 48, VectorReferenceCountOffset);
    std::free(object);
}

}
