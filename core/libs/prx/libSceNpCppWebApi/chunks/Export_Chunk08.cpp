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

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V219WebApiFilterRequestEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V219WebApiFilterRequestEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V219WebApiFilterRequestEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEC1Ev(IntrusivePtr* self) {
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118LeaderWithOnlineIdEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V160PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V143PutPlayerSessionsSessionIdLeaderRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorImED2Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody17setNpServiceLabelERKj(void* self, const std::uint32_t* label) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody15setWaitsForDataERKb(void* self, const bool* value) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody17setNpServiceLabelERKj(void* self, const std::uint32_t* label) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9setfieldsEPKc(void* self, const char* fields) {
    auto* parameter = static_cast<ParameterObject*>(self);
    if (parameter->implementation == nullptr) return ParameterNotInitialized;
    void* implementation = const_cast<void*>(parameter->implementation);
    const int result = AssignCommonString(CommonStringAt(implementation, 0x08), fields);
    if (result < 0) return result;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer14setkeepHtmlTagEb(void* self, bool value) {
    void* implementation = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(self) + 0x08);
    if (implementation == nullptr) return;
}

void APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer20setuseCurrencySymbolEb(void* self, bool value) {
    void* implementation = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(self) + 0x08);
    if (implementation == nullptr) return;
}

IntrusivePtr APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V59Container7getSkusEv(void* self) {
    IntrusivePtr result = *reinterpret_cast<const IntrusivePtr*>(static_cast<const std::uint8_t*>(self) + 0x208);
    if (result.object != nullptr) ReferenceCount(result.object, VectorReferenceCountOffset).fetch_add(1);
    return result;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody12entriesIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x48) != 0;
}

std::int32_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody18getTotalEntryCountEv(const void* self) {
    std::int32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x58, sizeof(value));
    return value;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody17needsTmpRankIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x42) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku10getEndDateEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x88;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku12endDateIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xa8) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container13childrenIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x240) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDescriptionEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x158;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDisplayNameEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x88;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getReleaseDateEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0xa8;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container16releaseDateIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xc8) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container5getIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x38;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container7getTypeEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x1e8;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container8getLabelEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x58;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container9skusIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x220) != 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V149PostPlayerSessionsSessionIdInvitationsRequestBodyEEC1Ev(IntrusivePtr* self) {
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEE8pushBackERKS8_(IntrusivePtr* self, const void* element) {
    return VectorPushBackIntrusivePtr(self, static_cast<const IntrusivePtr*>(element));
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V149PostPlayerSessionsSessionIdInvitationsRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEaSERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V12ToEEC1Ev(IntrusivePtr* self) {
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V160PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V120PlayerSessionForReadEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V161PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBase14getHeaderValueEPKcRNS2_12IntrusivePtrINS2_6StringEEE(const CommonResponseHeaderBase* self, const char* name, IntrusivePtr* value) {
    (void)name;
    (void)value;
    if (self->headers == nullptr) return ParameterBodyMissing;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorImE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 3, sizeof(std::uint64_t), __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorImE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 3, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorImEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(std::uint64_t));
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V113BasicPresence12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V113BasicPresence12getInContextEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x21) != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V113BasicPresence15getOnlineStatusEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x1c);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi15ProfanityFilter2V223FilterProfanityResponse10getMessageEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x38;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponse13getRestrictedEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x30) != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_18ResponseHeaderBaseEEptEv(const IntrusivePtr* self) {
    return self->object;
}

static_assert(sizeof(CommonResponseHeaderBase) == 0x10 && offsetof(CommonResponseHeaderBase, headers) == 0x08);

void APS5_VABI _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseC1Ev(CommonResponseHeaderBase* self) {
    self->referenceCount = 0;
    self->headers = nullptr;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseC2Ev(CommonResponseHeaderBase* self) {
    _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseC1Ev(self);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseD1Ev(CommonResponseHeaderBase* self) {
    (void)self;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseD2Ev(CommonResponseHeaderBase* self) {
    (void)self;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6StringEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11UserProfile2V113BasicPresenceEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorImEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSA_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

}
