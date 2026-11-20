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

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api12submitTicketEiRKNS4_23ParameterToSubmitTicketERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_24SubmitTicketResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOffer10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* id) {
    return InitializeParameter(self, context, id != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOffer9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOfferC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOfferD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicket10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* id) {
    return InitializeParameter(self, context, id != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicket7setviewEPKc(void* self, const char* view) {
    return SetParameterText(self, view);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicket9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicketC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicketD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicket10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* id) {
    return InitializeParameter(self, context, id != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicket9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicketC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicketD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicket10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_23SubmitTicketRequestBodyEEE(void* self, const std::uint64_t* context, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, true, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicket9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicketC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicketD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api8getOfferEiRKNS4_19ParameterToGetOfferERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_20GetOfferResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api9getTicketEiRKNS4_20ParameterToGetTicketERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_21GetTicketResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V18Location16setGameSessionIdEPKc(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x19);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getPlatformEv(void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x30, sizeof(value));
    return static_cast<int>(value);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getTeamNameEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x38);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead12getAccountIdEv(const void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x10, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13onlineIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x2d, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13platformIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x34, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13teamNameIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x58, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead14accountIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x18, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead21playerAttributesIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x80, sizeof(value));
    return value;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x19);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getPlatformEv(void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x30, sizeof(value));
    return static_cast<int>(value);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getTeamNameEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x38);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getTicketIdEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x60);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead12getAccountIdEv(const void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x10, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13onlineIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x2d, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13platformIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x34, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13teamNameIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x58, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13ticketIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x80, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead14accountIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x18, sizeof(value));
    return value;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody10getOfferIdEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x30);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody11getLocationEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0xa8, ReferenceCountOffset);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody11statusIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xa0, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody12offerIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x50, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody12playersIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x98, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody13locationIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xc0, sizeof(value));
    return value;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody14getRulesetNameEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x58);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody16rulesetNameIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x78, sizeof(value));
    return value;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getCreatedDateTimeEv(void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xe8, sizeof(value));
    return value;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getUpdatedDateTimeEv(void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xf8, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20createdDateTimeIsSetEv(void* self) {
    return static_cast<const std::uint8_t*>(self)[0xf0] != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20updatedDateTimeIsSetEv(void* self) {
    return static_cast<const std::uint8_t*>(self)[0x100] != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody9getStatusEv(void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x9c, sizeof(value));
    return static_cast<int>(value);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody11getTicketIdEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x30);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody11statusIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xc0, sizeof(value));
    return value;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody12getSubmitterEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0xf0, ReferenceCountOffset);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody12playersIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xb8, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody13ticketIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x50, sizeof(value));
    return value;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody14getRulesetNameEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x58);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody14submitterIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x108, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody16rulesetNameIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x78, sizeof(value));
    return value;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody18getCreatedDateTimeEv(void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x130, sizeof(value));
    return value;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody18getUpdatedDateTimeEv(void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x140, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody20createdDateTimeIsSetEv(void* self) {
    return static_cast<const std::uint8_t*>(self)[0x138] != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody20updatedDateTimeIsSetEv(void* self) {
    return static_cast<const std::uint8_t*>(self)[0x148] != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody21ticketAttributesIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x98, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody9getStatusEv(void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0xbc, sizeof(value));
    return static_cast<int>(value);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V124SubmitTicketResponseBody11getTicketIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x30;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V124SubmitTicketResponseBody13ticketIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x50, sizeof(value));
    return value;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V18Location16getGameSessionIdEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x10);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V18Location18gameSessionIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x30, sizeof(value));
    return value;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Attribute7getNameEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x10);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Attribute7getTypeEv(void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x30, sizeof(value));
    return static_cast<int>(value);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Attribute8getValueEv(void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x38);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter11getPlatformEv(void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x1c, sizeof(value));
    return static_cast<int>(value);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter12getAccountIdEv(const void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x10, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter13platformIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x20, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter14accountIdIsSetEv(void* self) {
    std::uint8_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x18, sizeof(value));
    return value;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData10initializeEPNS1_6Common10LibContextEPKci(void* self, const std::uint64_t* context, const char* text, int value) {
    (void)value;
    return InitializeParameter(self, context, text != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V129SetDataInfoRequestBodyFactory6createEPNS1_6Common10LibContextEPvmPNS5_12IntrusivePtrINS3_22SetDataInfoRequestBodyEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi10uploadDataEiRKNS4_21ParameterToUploadDataERNS1_6Common19UpStreamTransactionINS8_12IntrusivePtrINS3_22UploadDataResponseBodyEEENSA_INS4_25UploadDataResponseHeadersEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi10uploadDataEiRKNS4_42ParameterWithBinaryRequestBodyToUploadDataERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_22UploadDataResponseBodyEEENSA_INS4_25UploadDataResponseHeadersEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi11setDataInfoEiRKNS4_22ParameterToSetDataInfoERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS4_26SetDataInfoResponseHeadersEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

}
