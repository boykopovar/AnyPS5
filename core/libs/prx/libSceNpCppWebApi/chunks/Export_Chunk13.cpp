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

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}




int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi15getGameSessionsEiRKNS4_26ParameterToGetGameSessionsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_27GetGameSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi16leaveGameSessionEiRKNS4_27ParameterToLeaveGameSessionERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi18createGameSessionsEiRKNS4_29ParameterToCreateGameSessionsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_28PostGameSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi23joinGameSessionAsPlayerEiRKNS4_34ParameterToJoinGameSessionAsPlayerERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_50PostGameSessionsSessionIdMemberPlayersResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi24setGameSessionPropertiesEiRKNS4_35ParameterToSetGameSessionPropertiesERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi26ParameterToGetGameSessions10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* a1) {
    return InitializeParameter(self, context, a1 != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi26ParameterToGetGameSessions9setfieldsENS1_6Common12IntrusivePtrINS6_6VectorINS6_6StringEEEEE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi26ParameterToGetGameSessions9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi26ParameterToGetGameSessionsC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi26ParameterToGetGameSessionsD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi27ParameterToLeaveGameSession10initializeEPNS1_6Common10LibContextEPKcSA_(void* self, const std::uint64_t* context, ...) {
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi27ParameterToLeaveGameSession9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi27ParameterToLeaveGameSessionC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi27ParameterToLeaveGameSessionD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi29ParameterToCreateGameSessions10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_27PostGameSessionsRequestBodyEEE(void* self, const std::uint64_t* context, ...) {
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi29ParameterToCreateGameSessions9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi29ParameterToCreateGameSessionsC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi29ParameterToCreateGameSessionsD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi34ParameterToJoinGameSessionAsPlayer10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_49PostGameSessionsSessionIdMemberPlayersRequestBodyEEE(void* self, const std::uint64_t* context, ...) {
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi34ParameterToJoinGameSessionAsPlayer9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi34ParameterToJoinGameSessionAsPlayerC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi34ParameterToJoinGameSessionAsPlayerD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi35ParameterToSetGameSessionProperties10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_37PatchGameSessionsSessionIdRequestBodyEEE(void* self, const std::uint64_t* context, ...) {
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi35ParameterToSetGameSessionProperties9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi35ParameterToSetGameSessionPropertiesC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi35ParameterToSetGameSessionPropertiesD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V118RequestGameSession14setCustomData1EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V118RequestGameSession14setCustomData2EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V118RequestGameSession16setMaxSpectatorsERKi() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V118RequestGameSession19setUsePlayerSessionERKb() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession16setSwapSupportedERKb() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession19setLeaderPrivilegesERKNS1_6Common6VectorINS5_6StringEEE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V124RequestGameSessionPlayer12setJoinStateERKNS3_16InitialJoinStateE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V124RequestGameSessionPlayer15setPushContextsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22GameSessionPushContextEEEEE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V125RequestGameSessionFactory6createEPNS1_6Common10LibContextEiNS5_12IntrusivePtrINS3_30RequestGameSessionMemberPlayerEEERKNS5_6VectorINS5_6StringEEEPNS8_INS3_18RequestGameSessionEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V127GetGameSessionsResponseBody15getGameSessionsEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V128PostGameSessionsResponseBody15getGameSessionsEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V128RequestJoinGameSessionPlayer12setJoinStateERKNS3_16InitialJoinStateE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V128RequestJoinGameSessionPlayer15setPushContextsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22GameSessionPushContextEEEEE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V129GameSessionPushContextFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_22GameSessionPushContextEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V131RequestGameSessionPlayerFactory6createEPNS1_6Common10LibContextEPKcS9_PNS5_12IntrusivePtrINS3_24RequestGameSessionPlayerEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V134PostGameSessionsRequestBodyFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_18RequestGameSessionEEEEEPNS9_INS3_27PostGameSessionsRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V135RequestJoinGameSessionPlayerFactory6createEPNS1_6Common10LibContextEPKcS9_PNS5_12IntrusivePtrINS3_28RequestJoinGameSessionPlayerEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V137PatchGameSessionsSessionIdRequestBody14setCustomData1EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V137PatchGameSessionsSessionIdRequestBody14setCustomData2EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V137PatchGameSessionsSessionIdRequestBody16setMaxSpectatorsERKi() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V137RequestGameSessionMemberPlayerFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_24RequestGameSessionPlayerEEEEEPNS9_INS3_30RequestGameSessionMemberPlayerEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody14setCustomData1EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody14setCustomData2EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody16setMaxSpectatorsERKi() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V144PatchGameSessionsSessionIdRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_37PatchGameSessionsSessionIdRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V156PostGameSessionsSessionIdMemberPlayersRequestBodyFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_28RequestJoinGameSessionPlayerEEEEEPNS9_INS3_49PostGameSessionsSessionIdMemberPlayersRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetPublicProfilesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSD_ESD_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V130PostPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V152PostPlayerSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V119CreateMatchResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V119CreateMatchResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V119CreateMatchResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V119CreateMatchResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv(const void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V119CreateMatchResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(const void* self, void* response) {
    (void)self;
    (void)response;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(const void* self, void* response) {
    (void)self;
    (void)response;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6VectorINS4_INS1_13InGameCatalog2V59ContainerEEEEEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliSB_PvEPFvliSF_ESF_() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_15DefaultResponseENS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_15DefaultResponseENS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_15DefaultResponseENS2_12IntrusivePtrINS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS4_PvEPFvliS9_ES9_() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121PlayerForTicketCreateEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121PlayerForTicketCreateEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V123SubmitTicketRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V123SubmitTicketRequestBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V123SubmitTicketRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V19AttributeEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V19AttributeEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V114RepresentativeEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118MatchmakingForReadEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118RequestGameSessionEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118RequestGameSessionEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124PlayerSessionPushContextEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124RequestGameSessionPlayerEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124RequestGameSessionPlayerEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127PostGameSessionsRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127PostGameSessionsRequestBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127PostGameSessionsRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

}
