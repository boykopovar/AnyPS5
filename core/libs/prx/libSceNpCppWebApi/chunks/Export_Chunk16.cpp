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

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V117LeaveMatchRequestEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V118CreateMatchRequestEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V119CreateMatchResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V119RequestInGameRosterEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V119RequestMatchResultsEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V120ReportResultsRequestEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V122RequestMatchStatisticsEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V124RequestCompetitiveResultEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V124UpdateMatchDetailRequestEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V124UpdateMatchStatusRequestEEptEv(const IntrusivePtr* self) {
    return self->object;
}


void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V111GameSessionEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V118GameSessionForReadEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V118GameSessionForReadEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V120PlayerSessionForReadEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V130PostPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V152PostPlayerSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE18getResponseHeadersERSB_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_15DefaultResponseENS2_12IntrusivePtrINS2_18ResponseHeaderBaseEEEE18getResponseHeadersERS7_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}


std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}


static constexpr int StringInvalidArgument = static_cast<int>(0x80553503);
static constexpr int StringOutOfMemory = static_cast<int>(0x80553504);

static void ResetString(CommonString* self) {
    std::free(self->buffer);
    self->buffer = nullptr;
    self->bufferSize = 0;
}

static int AssignString(CommonString* self, const char* text) {
    if (text == nullptr) return StringInvalidArgument;
    const std::size_t length = std::strlen(text) + 1;
    char* buffer = static_cast<char*>(std::malloc(length));
    if (buffer == nullptr) return StringOutOfMemory;
    std::memcpy(buffer, text, length);
    std::free(self->buffer);
    self->buffer = buffer;
    self->bufferSize = length;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(CommonString* self, void* libContext) {
    self->referenceCount = 0;
    self->buffer = nullptr;
    self->bufferSize = 0;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringC1Ev(CommonString* self) {
    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(self, nullptr);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringD1Ev(CommonString* self) {
    ResetString(self);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringD2Ev(CommonString* self) {
    ResetString(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6String8copyFromEPKc(CommonString* self, const char* text) {
    return AssignString(self, text);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6String8copyFromERKS3_(CommonString* self, const CommonString* source) {
    const bool hadContext = self->libContext != nullptr;
    if (!hadContext) self->libContext = source->libContext;
    if (source->bufferSize == 0) {
        ResetString(self);
        return 0;
    }
    const int result = AssignString(self, source->buffer != nullptr ? source->buffer : "");
    if (!hadContext && result != 0) self->libContext = nullptr;
    return result;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6String5clearEv(CommonString* self) {
    ResetString(self);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String4sizeEv(const CommonString* self) {
    return self->buffer != nullptr ? static_cast<std::uint64_t>(std::strlen(self->buffer)) : 0;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String6lengthEv(const CommonString* self) {
    return self->buffer != nullptr ? static_cast<std::uint64_t>(std::strlen(self->buffer)) : 0;
}

bool APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringeqERKS3_(const CommonString* self, const CommonString* other) {
    const char* left = self->buffer != nullptr ? self->buffer : "";
    const char* right = other->buffer != nullptr ? other->buffer : "";
    return std::strncmp(left, right, std::strlen(left) + 1) == 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6String10setContextEPNS2_10LibContextE(CommonString* self, void* libContext) {
    self->libContext = libContext;
}

std::int32_t APS5_VABI _ZN3sce2Np9CppWebApi6Common6String21intrusive_ptr_add_refEPS3_(CommonString* self) {
    return std::atomic_ref<std::int32_t>(self->referenceCount).fetch_add(1) + 1;
}

std::int32_t APS5_VABI _ZN3sce2Np9CppWebApi6Common6String21intrusive_ptr_sub_refEPS3_(CommonString* self) {
    return std::atomic_ref<std::int32_t>(self->referenceCount).fetch_sub(1);
}

const char* APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String5c_strEv(const CommonString* self) {
    return self->buffer != nullptr ? self->buffer : "";
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6String6appendEPKc(CommonString* self, const char* text) {
    if (text == nullptr) return StringInvalidArgument;
    if (*text == '\0') return 0;
    const std::size_t oldLength = self->buffer != nullptr && self->bufferSize != 0
        ? static_cast<std::size_t>(self->bufferSize - 1) : 0;
    const std::size_t textLength = std::strlen(text);
    const std::size_t newSize = oldLength + textLength + 1;
    char* buffer = static_cast<char*>(std::malloc(newSize));
    if (buffer == nullptr) return StringOutOfMemory;
    if (oldLength != 0) std::memcpy(buffer, self->buffer, oldLength);
    std::memcpy(buffer + oldLength, text, textLength + 1);
    std::free(self->buffer);
    self->buffer = buffer;
    self->bufferSize = newSize;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6String6appendERKS3_(CommonString* self, const CommonString* source) {
    return _ZN3sce2Np9CppWebApi6Common6String6appendEPKc(self, source->buffer != nullptr ? source->buffer : "");
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEE8pushBackERKS8_(IntrusivePtr* self, const void* element) {
    return VectorPushBackIntrusivePtr(self, static_cast<const IntrusivePtr*>(element));
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, StringVectorReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEC1EPS6_PFvS8_EPNS2_10LibContextE(IntrusivePtr* self, void* body, void (APS5_VABI* deleter)(void*), void* libContext) {
    self->object = body;
    self->deleter = deleter;
    self->libContext = libContext;
    if (body) ReferenceCount(body, StringVectorReferenceCountOffset).fetch_add(1);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V126RequestPlayerSessionPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, StringVectorReferenceCountOffset, __func__);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ScalarVectorReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEEaSERS6_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, ScalarVectorReferenceCountOffset, __func__);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String5emptyEv(const CommonString* self) {
    return self->buffer == nullptr || self->buffer[0] == '\0';
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi16getPublicProfileEiRKNS4_27ParameterToGetPublicProfileERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_24GetPublicProfileResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi27ParameterToGetPublicProfile10initializeEPNS1_6Common10LibContextEPKc() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi27ParameterToGetPublicProfile9terminateEv() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi27ParameterToGetPublicProfileC1Ev() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V115BasicProfileApi27ParameterToGetPublicProfileD1Ev() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}


static_assert(sizeof(CommonConstIterator) == 0x10 && offsetof(CommonConstIterator, element) == 0x8);
static constexpr std::size_t FriendVectorBegin = 0x18;
static constexpr std::size_t FriendVectorEnd = 0x28;
static constexpr std::size_t FriendVectorElementSize = 0x18;
static constexpr std::size_t FriendVectorReferenceCount = 0x30;
static constexpr std::size_t FriendPlayerSessions = 0x30;

static const void* FriendVectorReadPointer(const void* self, const std::size_t offset) {
    const void* value = nullptr;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + offset, sizeof(value));
    return value;
}

static std::uint64_t FriendVectorSize(const void* self) {
    const auto begin = reinterpret_cast<std::uintptr_t>(FriendVectorReadPointer(self, FriendVectorBegin));
    const auto end = reinterpret_cast<std::uintptr_t>(FriendVectorReadPointer(self, FriendVectorEnd));
    return static_cast<std::uint64_t>((end - begin) / FriendVectorElementSize);
}

static void* FriendVectorElement(void* self, const std::uint64_t index) {
    return static_cast<std::uint8_t*>(const_cast<void*>(FriendVectorReadPointer(self, FriendVectorBegin))) +
           index * FriendVectorElementSize;
}

static CommonConstIterator FriendVectorIterator(const void* self, const std::size_t offset) {
    return CommonConstIterator(0, const_cast<void*>(FriendVectorReadPointer(self, offset)));
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessionsC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessionsD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions10initializeEPNS1_6Common10LibContextE(void* self, const std::uint64_t* context) {
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions9terminateEv(void* self) {
    return TerminateParameter(self);
}

static bool TransactionStarted(const void* self) {
    if (self == nullptr) return false;
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    std::memcpy(&first, self, sizeof(first));
    std::memcpy(&second, static_cast<const std::uint8_t*>(self) + sizeof(first), sizeof(second));
    return first != 0 && second != 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi24getFriendsPlayerSessionsEiRKNS4_35ParameterToGetFriendsPlayerSessionsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_36GetFriendsPlayerSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE(int self, const void* parameter, void* transaction) {
    (void)self;
    (void)parameter;
    if (!TransactionStarted(transaction)) return TransactionNotStarted;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

IntrusivePtr APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V136GetFriendsPlayerSessionsResponseBody10getFriendsEv(const void* self) {
    IntrusivePtr result;
    CopyIntrusivePtr(&result, reinterpret_cast<const IntrusivePtr*>(
        static_cast<const std::uint8_t*>(self) + FriendPlayerSessions), FriendVectorReferenceCount);
    return result;
}

IntrusivePtr APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V16Friend17getPlayerSessionsEv(const void* self) {
    IntrusivePtr result;
    CopyIntrusivePtr(&result, reinterpret_cast<const IntrusivePtr*>(
        static_cast<const std::uint8_t*>(self) + FriendPlayerSessions), FriendVectorReferenceCount);
    return result;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    std::memset(self, 0, 0x10);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V114PersonalDetailEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V16AvatarEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetPublicProfileResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

}
