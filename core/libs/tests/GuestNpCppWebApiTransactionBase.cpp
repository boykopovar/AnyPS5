#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int TransactionNotStarted = static_cast<int>(0x80553508);
constexpr int TransactionInvalidArgument = static_cast<int>(0x80553503);
constexpr int TransactionCallbackAlreadySet = static_cast<int>(0x80553507);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceNpCppWebApi transaction base: %s\n", message);
    std::abort();
}

template <typename T>
T Load(const void* base, const std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(value));
    return value;
}

template <typename T>
void Store(void* base, const std::size_t offset, const T& value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

}

int main() {
    alignas(8) unsigned char transaction[0x10];
    alignas(8) unsigned char body[0x100];
    std::memset(transaction, 0, sizeof(transaction));
    std::memset(body, 0, sizeof(body));

    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS9_PvEPFvliSD_ESD_(
                transaction, nullptr, nullptr, nullptr) == TransactionNotStarted,
            "setOnFinishedCallback rejects an unstarted transaction");

    std::uint64_t context = 0;
    Store<void*>(transaction, 0, &context);
    Store<void*>(transaction, 8, body);
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS9_PvEPFvliSD_ESD_(
                transaction, nullptr, reinterpret_cast<void*>(2), reinterpret_cast<void*>(3)) == TransactionInvalidArgument,
            "setOnFinishedCallback rejects a null handler");
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS9_PvEPFvliSD_ESD_(
                transaction, reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), reinterpret_cast<void*>(3)) == 0,
            "setOnFinishedCallback stores the callbacks");
    Require(Load<void*>(body, 0x98) == reinterpret_cast<void*>(1) && Load<void*>(body, 0xa0) == reinterpret_cast<void*>(2) &&
                Load<void*>(body, 0xa8) == reinterpret_cast<void*>(3),
            "setOnFinishedCallback stores the callback fields");
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS9_PvEPFvliSD_ESD_(
                transaction, reinterpret_cast<void*>(4), reinterpret_cast<void*>(5), reinterpret_cast<void*>(6)) ==
                TransactionCallbackAlreadySet,
            "setOnFinishedCallback rejects a second registration");

    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption(
                transaction, reinterpret_cast<void*>(0x1234)) == 0,
            "setResponseInformationOption stores the option");
    Require(Load<void*>(body, 0x78) == reinterpret_cast<void*>(0x1234), "setResponseInformationOption stores the option field");
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE28getResponseInformationOptionEv(
                transaction) == reinterpret_cast<void*>(0x1234),
            "getResponseInformationOption reads the option field");

    std::uint64_t headers = 0;
    Require(_ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSB_(
                transaction, &headers) == TransactionNotStarted,
            "getResponseHeaders reports an unstarted transaction off-line");

    Store<void*>(transaction, 8, nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE18getResponseHeadersERSB_(
                transaction, &headers) == TransactionNotStarted,
            "getResponseHeaders rejects a transaction without a body");
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE28setResponseInformationOptionEP37SceNpWebApi2ResponseInformationOption(
                transaction, reinterpret_cast<void*>(0x1234)) == TransactionNotStarted,
            "setResponseInformationOption rejects a transaction without a body");
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE28getResponseInformationOptionEv(
                transaction) == nullptr,
            "getResponseInformationOption returns null without a body");

    alignas(8) unsigned char playerSession[0x200];
    std::memset(playerSession, 0, sizeof(playerSession));
    const std::int32_t expirationTime = 0x01020304;
    _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession17setExpirationTimeERKi(playerSession, &expirationTime);
    Require(Load<std::int32_t>(playerSession, 0x11c) == expirationTime && Load<std::uint8_t>(playerSession, 0x120) == 1,
            "setExpirationTime stores the value and its flag");
    const bool nonPsnSupported = true;
    _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession18setNonPsnSupportedERKb(playerSession, &nonPsnSupported);
    Require(Load<std::uint8_t>(playerSession, 0x121) == 1 && Load<std::uint8_t>(playerSession, 0x122) == 1,
            "setNonPsnSupported stores the value and its flag");

    std::puts("libSceNpCppWebApi transaction base checks passed");
    return 0;
}
