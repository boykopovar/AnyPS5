#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

template <typename T>
T Read(const std::uint8_t* object, const std::size_t offset) {
    T value;
    std::memcpy(&value, object + offset, sizeof(value));
    return value;
}

template <typename T>
void Write(std::uint8_t* object, const std::size_t offset, const T value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

constexpr auto CreateRemovedPlayer = _ZN3sce2Np9CppWebApi7Matches2V120RemovedPlayerFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RemovedPlayerEEE;
constexpr auto ReleaseRemovedPlayer = _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V113RemovedPlayerEED1Ev;
constexpr auto ConstructRemovedPlayers = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEEEC1EPNS2_10LibContextE;
constexpr auto PushRemovedPlayer = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEEE8pushBackERKS8_;
constexpr auto DestroyRemovedPlayers = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEEED1Ev;
constexpr auto CreateMember = _ZN3sce2Np9CppWebApi7Matches2V120RequestMemberFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RequestMemberEEE;
constexpr auto ReleaseMember = _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V113RequestMemberEED1Ev;
constexpr auto ConstructMembers = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEEC1EPNS2_10LibContextE;
constexpr auto PushMember = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEE8pushBackERKS8_;
constexpr auto DestroyMembers = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEED1Ev;
constexpr auto CreateTeam = _ZN3sce2Np9CppWebApi7Matches2V123RequestMatchTeamFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_16RequestMatchTeamEEE;
constexpr auto SetTeamMembers = _ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE;
constexpr auto ReleaseTeam = _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEED1Ev;

void (APS5_VABI* originalDeleter)(void*);
void* trackedObject;
void* trackedContext;
IntrusivePtr* observedOutput;
int deletionCount;

void APS5_VABI TrackDeletion(void* object) {
    Require(object == trackedObject);
    if (observedOutput != nullptr) {
        Require(observedOutput->object == object);
        Require(observedOutput->libContext == trackedContext);
    }
    ++deletionCount;
    originalDeleter(object);
}

void ObserveDeletion(IntrusivePtr* handle, bool observeOutput = false) {
    Require(handle->object != nullptr && handle->deleter != nullptr);
    originalDeleter = handle->deleter;
    trackedObject = handle->object;
    trackedContext = handle->libContext;
    observedOutput = observeOutput ? handle : nullptr;
    deletionCount = 0;
    handle->deleter = &TrackDeletion;
}

void TestOwnedFactoryOutput() {
    std::uint64_t firstContext = 1;
    std::uint64_t nextContext = 2;
    IntrusivePtr output{};
    Require(CreateRemovedPlayer(&firstContext, "first", &output) == 0);
    ObserveDeletion(&output, true);
    Require(CreateRemovedPlayer(&nextContext, "second", &output) == 0);
    Require(deletionCount == 1);
    Require(output.object != nullptr && output.object != trackedObject);
    Require(output.deleter != &TrackDeletion && output.libContext == &nextContext);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(output.object), 0) == 1);
    Require(std::strcmp(Read<const char*>(static_cast<std::uint8_t*>(output.object), 24), "second") == 0);
    ReleaseRemovedPlayer(&output);
    Require(output.object == nullptr);
}

void TestSharedFactoryOutput() {
    std::uint64_t context = 1;
    IntrusivePtr output{};
    IntrusivePtr owners{};
    Require(CreateRemovedPlayer(&context, "shared", &output) == 0);
    ObserveDeletion(&output);
    ConstructRemovedPlayers(&owners, &context);
    Require(PushRemovedPlayer(&owners, &output) == 0);
    Require(CreateRemovedPlayer(&context, "replacement", &output) == 0);
    Require(deletionCount == 0);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(trackedObject), 0) == 1);
    ReleaseRemovedPlayer(&output);
    DestroyRemovedPlayers(&owners);
    Require(deletionCount == 1);
}

void TestRejectedFactoryArguments() {
    std::uint64_t context = 1;
    IntrusivePtr output{};
    Require(CreateRemovedPlayer(&context, "kept", &output) == 0);
    ObserveDeletion(&output, true);
    const IntrusivePtr before = output;
    Require(CreateRemovedPlayer(nullptr, "unused", &output) == static_cast<int>(0x80553503));
    Require(CreateRemovedPlayer(&context, "unused", nullptr) == static_cast<int>(0x80553503));
    Require(output.object == before.object && output.deleter == before.deleter && output.libContext == before.libContext);
    Require(deletionCount == 0);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(output.object), 0) == 1);
    ReleaseRemovedPlayer(&output);
    Require(deletionCount == 1);
}

void TestFailedFactoryPayload() {
    std::uint64_t context = 1;
    IntrusivePtr output{};
    Require(CreateRemovedPlayer(&context, "previous", &output) == 0);
    ObserveDeletion(&output, true);
    Require(CreateRemovedPlayer(&context, nullptr, &output) == static_cast<int>(0x80553503));
    Require(deletionCount == 1);
    Require(output.object != nullptr && output.object != trackedObject);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(output.object), 0) == 1);
    Require(Read<const char*>(static_cast<std::uint8_t*>(output.object), 24) == nullptr);
    ReleaseRemovedPlayer(&output);
}

void TestFactoryReleasesMembers() {
    std::uint64_t context = 1;
    IntrusivePtr member{};
    IntrusivePtr members{};
    IntrusivePtr team{};
    Require(CreateMember(&context, "member", &member) == 0);
    ConstructMembers(&members, &context);
    Require(PushMember(&members, &member) == 0);
    ObserveDeletion(&members);
    Require(CreateTeam(&context, "first team", &team) == 0);
    Require(SetTeamMembers(team.object, &members) == 0);
    Require(CreateTeam(&context, "next team", &team) == 0);
    Require(deletionCount == 0);
    DestroyMembers(&members);
    Require(deletionCount == 1);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(member.object), 0) == 1);
    ReleaseMember(&member);
    ReleaseTeam(&team);
}

void RunFactoryOwnershipChecks() {
    TestOwnedFactoryOutput();
    TestSharedFactoryOutput();
    TestRejectedFactoryArguments();
    TestFailedFactoryPayload();
    TestFactoryReleasesMembers();
}

}

int main(int argc, char** argv) {
    RunFactoryOwnershipChecks();
    if (argc == 2 && std::strcmp(argv[1], "factory-ownership") == 0) return 0;
    alignas(8) std::array<std::uint8_t, 0x200> request{};
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer12setAccountIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 96 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 96 + 0x08), "value") == 0);
    Require(request[128] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer13setPlayerNameEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 56 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 56 + 0x08), "value") == 0);
    Require(request[88] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer9setTeamIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 136 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 136 + 0x08), "value") == 0);
    Require(request[168] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam11setTeamNameEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 48 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 48 + 0x08), "value") == 0);
    Require(request[80] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest9setZoneIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 56 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 56 + 0x08), "value") == 0);
    Require(request[88] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer12setAccountIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 96 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 96 + 0x08), "value") == 0);
    Require(request[128] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer13setPlayerNameEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 56 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 56 + 0x08), "value") == 0);
    Require(request[88] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest9setZoneIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 24 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 24 + 0x08), "value") == 0);
    Require(request[56] == 1);
    std::memset(request.data(), 0, request.size());
    const int intValue = 0x1234;
    Require(_ZN3sce2Np9CppWebApi7Matches2V113RemovedPlayer9setReasonERKNS3_11LeaveReasonE(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 48) == intValue && request[52] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116JoinMatchRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V117LeaveMatchRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setExpirationTimeERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 92) == intValue && request[96] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCooperativeResultERKNS3_24RequestCooperativeResultE(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 20) == intValue && request[24] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setExpirationTimeERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 60) == intValue && request[64] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchStatusRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    const double doubleValue = 1.5;
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults8setScoreERKd(request.data(), &doubleValue) == 0);
    Require(Read<double>(request.data(), 56) == doubleValue && request[64] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestPlayerResults8setScoreERKd(request.data(), &doubleValue) == 0);
    Require(Read<double>(request.data(), 56) == doubleValue && request[64] == 1);
    alignas(8) std::array<std::uint8_t, 0x40> objectBody{};
    Write<std::int32_t>(objectBody.data(), 0x00, 1);
    const IntrusivePtr objectValue{objectBody.data(), nullptr, nullptr};
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 152) == objectBody.data() && request[176] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCompetitiveResultERKNS1_6Common12IntrusivePtrINS3_24RequestCompetitiveResultEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 32) == objectBody.data() && request[56] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest18setMatchStatisticsERKNS1_6Common12IntrusivePtrINS3_22RequestMatchStatisticsEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 48) == objectBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 112) == objectBody.data() && request[136] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    alignas(8) std::array<std::uint8_t, 0x40> vectorBody{};
    Write<std::int32_t>(vectorBody.data(), 0x30, 1);
    const IntrusivePtr vectorValue{vectorBody.data(), nullptr, nullptr};
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 88) == vectorBody.data() && request[112] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults20setTeamMemberResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 72) == vectorBody.data() && request[96] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10setPlayersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestMatchPlayerEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 16) == vectorBody.data() && request[40] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster8setTeamsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_16RequestMatchTeamEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic23setTeamMemberStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_26RequestTeamMemberStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 80) == vectorBody.data() && request[104] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic8setStatsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics17setTeamStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestTeamStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics19setPlayerStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22RequestPlayerStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 16) == vectorBody.data() && request[40] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult14setTeamResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestTeamResultsEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult16setPlayerResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestPlayerResultsEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 16) == vectorBody.data() && request[40] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::uint64_t vectorContext = 9;
    alignas(8) IntrusivePtr emptyMembers{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEEC1EPNS2_10LibContextE(&emptyMembers, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE(request.data(), &emptyMembers) == 0);
    Require(Read<void*>(request.data(), 88) == nullptr && request[112] == 1);
    alignas(8) IntrusivePtr emptyTeamMemberResults{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V123RequestTeamMemberResultEEEEC1EPNS2_10LibContextE(&emptyTeamMemberResults, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults20setTeamMemberResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEEEE(request.data(), &emptyTeamMemberResults) == 0);
    Require(Read<void*>(request.data(), 72) == nullptr && request[96] == 1);
    alignas(8) IntrusivePtr emptyPlayers{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEEC1EPNS2_10LibContextE(&emptyPlayers, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10setPlayersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestMatchPlayerEEEEE(request.data(), &emptyPlayers) == 0);
    Require(Read<void*>(request.data(), 16) == nullptr && request[40] == 1);
    alignas(8) IntrusivePtr emptyTeams{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEEC1EPNS2_10LibContextE(&emptyTeams, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster8setTeamsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_16RequestMatchTeamEEEEE(request.data(), &emptyTeams) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyTeamMemberStatistics{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V126RequestTeamMemberStatisticEEEEC1EPNS2_10LibContextE(&emptyTeamMemberStatistics, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic23setTeamMemberStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_26RequestTeamMemberStatisticEEEEE(request.data(), &emptyTeamMemberStatistics) == 0);
    Require(Read<void*>(request.data(), 80) == nullptr && request[104] == 1);
    alignas(8) IntrusivePtr emptyStats{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEEC1EPNS2_10LibContextE(&emptyStats, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic8setStatsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEE(request.data(), &emptyStats) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyTeamStatistics{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEEC1EPNS2_10LibContextE(&emptyTeamStatistics, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics17setTeamStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestTeamStatisticEEEEE(request.data(), &emptyTeamStatistics) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyPlayerStatistics{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEEC1EPNS2_10LibContextE(&emptyPlayerStatistics, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics19setPlayerStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22RequestPlayerStatisticEEEEE(request.data(), &emptyPlayerStatistics) == 0);
    Require(Read<void*>(request.data(), 16) == nullptr && request[40] == 1);
    alignas(8) IntrusivePtr emptyTeamResults{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEEEC1EPNS2_10LibContextE(&emptyTeamResults, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult14setTeamResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestTeamResultsEEEEE(request.data(), &emptyTeamResults) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyPlayerResults{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEEEC1EPNS2_10LibContextE(&emptyPlayerResults, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult16setPlayerResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestPlayerResultsEEEEE(request.data(), &emptyPlayerResults) == 0);
    Require(Read<void*>(request.data(), 16) == nullptr && request[40] == 1);
    std::memset(request.data(), 0, request.size());
    Write<std::uint8_t>(request.data(), 0x48, 1);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10teamsIsSetEv(request.data()));
    Write<std::uint8_t>(request.data(), 0x28, 1);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster12playersIsSetEv(request.data()));
    Require(_ZNK3sce2Np9CppWebApi7Matches2V119CreateMatchResponse10getMatchIdEv(request.data()) == request.data() + 0x30);
    alignas(8) std::array<std::uint8_t, 0x10> joinParameter{};
    alignas(8) std::array<std::uint8_t, 0x40> joinBodyObject{};
    Write<std::int32_t>(joinBodyObject.data(), 0x00, 1);
    const IntrusivePtr joinBody{joinBodyObject.data(), nullptr, nullptr};
    std::uint64_t context = 1;
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(joinParameter.data(), &context, "room", &joinBody) == 0);
    Require(Read<void*>(joinParameter.data(), 0x08) != nullptr);
    Require(Read<std::int32_t>(joinBodyObject.data(), 0x00) == 2);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(joinParameter.data(), &context, "room", &joinBody) == static_cast<int>(0x8055350f));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch9terminateEv(joinParameter.data()) == 0);
    Require(Read<std::int32_t>(joinBodyObject.data(), 0x00) == 1);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch9terminateEv(joinParameter.data()) == static_cast<int>(0x80553510));

    alignas(8) std::array<std::uint8_t, 0x10> emptyParameter{};
    const IntrusivePtr emptyBody{nullptr, nullptr, nullptr};
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), &context, "room", &emptyBody) == static_cast<int>(0x80553512));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), nullptr, "room", &joinBody) == static_cast<int>(0x80553503));
    std::uint64_t zero = 0;
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), &zero, "room", &joinBody) == static_cast<int>(0x80553502));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), &context, nullptr, &joinBody) == static_cast<int>(0x80553503));

    alignas(8) std::array<std::uint8_t, 0x10> createParameter{};
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_18CreateMatchRequestEEE(createParameter.data(), &context, &joinBody) == 0);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch9terminateEv(createParameter.data()) == 0);

    IntrusivePtr created{};
    Require(_ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE(nullptr, "room", &created) == static_cast<int>(0x80553503));
    Require(created.object == nullptr);
    Require(_ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE(&context, "room", nullptr) == static_cast<int>(0x80553503));
    Require(_ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE(&context, nullptr, &created) == static_cast<int>(0x80553503));
    Require(_ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE(&context, "room", &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(created.object), 0) == 1);
    Require(Read<const char*>(static_cast<std::uint8_t*>(created.object), 24 + 0x08) != nullptr && std::strcmp(Read<const char*>(static_cast<std::uint8_t*>(created.object), 24 + 0x08), "room") == 0);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V120RemovedPlayerFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RemovedPlayerEEE(&context, "player", &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(created.object != nullptr && created.libContext == &context);
    Require(Read<const char*>(static_cast<std::uint8_t*>(created.object), 16 + 0x08) != nullptr && std::strcmp(Read<const char*>(static_cast<std::uint8_t*>(created.object), 16 + 0x08), "player") == 0);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V118AddedPlayerFactory6createEPNS1_6Common10LibContextEPKcNS3_10PlayerTypeEPNS5_12IntrusivePtrINS3_11AddedPlayerEEE(&context, "player", 7, &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(created.object != nullptr);
    Require(Read<const char*>(static_cast<std::uint8_t*>(created.object), 16 + 0x08) != nullptr && std::strcmp(Read<const char*>(static_cast<std::uint8_t*>(created.object), 16 + 0x08), "player") == 0);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(created.object), 0x30) == 7);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V126RequestMatchResultsFactory6createEPNS1_6Common10LibContextENS3_14ResultsVersionEPNS5_12IntrusivePtrINS3_19RequestMatchResultsEEE(&context, 3, &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(created.object != nullptr);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(created.object), 0x10) == 3);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V131UpdateMatchStatusRequestFactory6createEPNS1_6Common10LibContextENS3_12UpdateStatusEPNS5_12IntrusivePtrINS3_24UpdateMatchStatusRequestEEE(&context, 2, &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(Read<std::int32_t>(static_cast<std::uint8_t*>(created.object), 0x18) == 2);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V126RequestInGameRosterFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_19RequestInGameRosterEEE(&context, &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestMemberFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RequestMemberEEE(&context, "member", &created) == 0);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V127RequestTeamStatisticFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_20RequestTeamStatisticEEE(&context, "member", &created) == 0);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V129RequestMatchStatisticsFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_22RequestMatchStatisticsEEE(&context, &created) == 0);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V131RequestCompetitiveResultFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_24RequestCompetitiveResultEEE(&context, &created) == 0);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    created.deleter(created.object);
    created = {};

    Require(_ZN3sce2Np9CppWebApi7Matches2V131UpdateMatchDetailRequestFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_24UpdateMatchDetailRequestEEE(&context, &created) == 0);
    Require(created.object != nullptr && created.deleter != nullptr && created.libContext == &context);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    created.deleter(created.object);
    created = {};

    alignas(8) std::array<std::uint8_t, 0x50> membersBody{};
    Write<std::uint64_t>(membersBody.data(), 0x00, 0x434e507056656342ULL);
    Write<std::int32_t>(membersBody.data(), 0x30, 1);
    IntrusivePtr membersVector{};
    membersVector.object = membersBody.data();
    membersVector.libContext = &context;
    Require(_ZN3sce2Np9CppWebApi7Matches2V123RequestMatchTeamFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_16RequestMatchTeamEEE(&context, "team", &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE(created.object, &membersVector) == 0);
    Require(Read<std::int32_t>(membersBody.data(), 0x30) == 2);
    created.deleter(created.object);
    Require(Read<std::int32_t>(membersBody.data(), 0x30) == 1);
    created = {};

    alignas(8) std::array<std::uint8_t, 0x50> rosterObject{};
    Write<std::int32_t>(rosterObject.data(), 0, 1);
    IntrusivePtr rosterHandle{};
    rosterHandle.object = rosterObject.data();
    rosterHandle.libContext = &context;
    Require(_ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE(&context, "room", &created) == 0);
    Require(Read<const void*>(static_cast<const std::uint8_t*>(created.object), 0x08) == &context);
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(created.object, &rosterHandle) == 0);
    Require(Read<std::int32_t>(rosterObject.data(), 0) == 2);
    created.deleter(created.object);
    Require(Read<std::int32_t>(rosterObject.data(), 0) == 1);
    created = {};

    return 0;
}
