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

constexpr auto InvitationVectorBegin = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEE5beginEv;
constexpr auto InvitationVectorEnd = _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEE3endEv;
constexpr auto InvitationIteratorAdvance = _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEEppEv;
constexpr auto InvitationIteratorDereference = _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEEdeEv;
constexpr auto InvitationIteratorNotEqual = _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEEneERKS9_;
constexpr auto StringVectorReserve = _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE7reserveEi;
constexpr auto StringVectorPushBack = _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_;

void TestInvitationVectorIteration() {
    IntrusivePtr empty{};
    CommonIterator first{};
    CommonIterator last{};
    Require(InvitationVectorBegin(&first, &empty) == &first);
    Require(InvitationVectorEnd(&last, &empty) == &last);
    Require(first.element == nullptr);
    Require(last.element == nullptr);
    Require(!InvitationIteratorNotEqual(&first, &last));

    alignas(8) std::array<std::uint8_t, 0x18> element{};
    CommonIterator iterator{};
    iterator.element = element.data();
    Require(InvitationIteratorDereference(&iterator) == element.data());
    Require(InvitationIteratorAdvance(&iterator) == &iterator);
    Require(iterator.element == element.data() + sizeof(IntrusivePtr));
    Require(InvitationIteratorNotEqual(&iterator, &first));
    Require(!InvitationIteratorNotEqual(&iterator, &iterator));
}

void TestStringVectorReserve() {
    IntrusivePtr vector{};
    Require(StringVectorReserve(&vector, 8) == 0);
    Require(StringVectorReserve(&vector, 2) == 0);
    Require(StringVectorReserve(&vector, -1) == 0);
    Require(vector.object != nullptr);

    CommonString element{};
    Require(StringVectorPushBack(&vector, &element) == 0);
    Require(StringVectorPushBack(&vector, &element) == 0);
    Require(StringVectorReserve(&vector, 1) == 0);
    Require(StringVectorReserve(&vector, 4) == 0);
}

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

    TestInvitationVectorIteration();
    TestStringVectorReserve();
    return 0;
}
