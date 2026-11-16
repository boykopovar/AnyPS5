#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(const bool value) {
    if (!value) std::abort();
}

constexpr auto CreateMember = _ZN3sce2Np9CppWebApi14SessionManager2V130MemberWithMultiPlatformFactory6createEPNS1_6Common10LibContextERKmPKcPNS5_12IntrusivePtrINS3_23MemberWithMultiPlatformEEE;
constexpr auto SendMessage = _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi22sendGameSessionMessageEiRKNS4_33ParameterToSendGameSessionMessageERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE;
constexpr auto GetResponse = _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_;
constexpr auto ParameterConstruct = _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessageC1Ev;
constexpr auto ParameterInitialize = _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessage10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_50PostGameSessionsSessionIdSessionMessageRequestBodyEEE;
constexpr auto ParameterTerminate = _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessage9terminateEv;
constexpr auto PointerConstruct = _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEC1Ev;
constexpr auto PointerDestruct = _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEED1Ev;
constexpr auto VectorConstruct = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEEEC1EPNS2_10LibContextE;
constexpr auto VectorDestruct = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEEED1Ev;

}

int main() {
    Require(CreateMember() == static_cast<int>(0x80552907u));
    Require(SendMessage() == static_cast<int>(0x80552907u));
    Require(GetResponse() == static_cast<int>(0x80552907u));

    alignas(8) std::array<std::uint8_t, 0x40> parameter{};
    std::uint64_t context = 1;
    Require(ParameterConstruct(parameter.data()) == 0);
    Require(ParameterInitialize(parameter.data(), &context, "session") == 0);
    Require(ParameterTerminate(parameter.data()) == 0);

    IntrusivePtr pointer{};
    PointerConstruct(&pointer);
    PointerDestruct(&pointer);
    IntrusivePtr vector{};
    VectorConstruct(&vector, nullptr);
    VectorDestruct(&vector);
    return 0;
}
