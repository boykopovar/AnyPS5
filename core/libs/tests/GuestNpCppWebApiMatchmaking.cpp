#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <source_location>

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

constexpr int kNotSignedIn = static_cast<int>(0x80552907);
constexpr int kParameterNotInitialized = static_cast<int>(0x80553502);
constexpr int kInvalidArgument = static_cast<int>(0x80553503);
constexpr int kAlreadyInitialized = static_cast<int>(0x8055350f);
constexpr int kNotInitialized = static_cast<int>(0x80553510);
constexpr int kBodyMissing = static_cast<int>(0x80553512);
constexpr std::size_t kParameterSize = 0x10;
constexpr std::size_t kGuardSize = 0x10;
constexpr unsigned char kFill = 0xA5;

struct Storage {
    alignas(8) unsigned char bytes[kParameterSize + kGuardSize];
};

bool GuardIntact(const Storage& storage) {
    for (std::size_t index = kParameterSize; index < sizeof(storage.bytes); ++index) {
        if (storage.bytes[index] != kFill) return false;
    }
    return true;
}

using SimpleFn = int (APS5_VABI*)(void*);
using TextFn = int (APS5_VABI*)(void*, const char*);
using StringInitFn = int (APS5_VABI*)(void*, const std::uint64_t*, const char*);
using BodyInitFn = int (APS5_VABI*)(void*, const std::uint64_t*, const IntrusivePtr*);

const SimpleFn kSignedOut[] = {
    _ZN3sce2Np9CppWebApi11Matchmaking2V115LocationFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_8LocationEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V116AttributeFactory6createEPNS1_6Common10LibContextEPKcNS3_13AttributeTypeES9_PNS5_12IntrusivePtrINS3_9AttributeEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V128PlayerForTicketCreateFactory6createEPNS1_6Common10LibContextERKmNS3_8PlatformEPNS5_12IntrusivePtrINS3_21PlayerForTicketCreateEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V130SubmitTicketRequestBodyFactory6createEPNS1_6Common10LibContextEPKcRKNS5_6VectorINS5_12IntrusivePtrINS3_21PlayerForTicketCreateEEEEEPNSB_INS3_23SubmitTicketRequestBodyEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api12cancelTicketEiRKNS4_23ParameterToCancelTicketERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api12submitTicketEiRKNS4_23ParameterToSubmitTicketERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_24SubmitTicketResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api8getOfferEiRKNS4_19ParameterToGetOfferERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_20GetOfferResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api9getTicketEiRKNS4_20ParameterToGetTicketERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_21GetTicketResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE,
};
const SimpleFn kSetters[] = {
    _ZN3sce2Np9CppWebApi11Matchmaking2V121PlayerForTicketCreate10setNatTypeERKi,
    _ZN3sce2Np9CppWebApi11Matchmaking2V121PlayerForTicketCreate11setTeamNameEPKc,
    _ZN3sce2Np9CppWebApi11Matchmaking2V121PlayerForTicketCreate19setPlayerAttributesERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_9AttributeEEEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V123SubmitTicketRequestBody11setLocationERKNS1_6Common12IntrusivePtrINS3_8LocationEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V123SubmitTicketRequestBody19setTicketAttributesERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_9AttributeEEEEE,
    _ZN3sce2Np9CppWebApi11Matchmaking2V18Location16setGameSessionIdEPKc,
};
const SimpleFn kConstructors[] = {
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOfferC1Ev,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicketC1Ev,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicketC1Ev,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicketC1Ev,
};
const SimpleFn kDestructors[] = {
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOfferD1Ev,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicketD1Ev,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicketD1Ev,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicketD1Ev,
};
const SimpleFn kTerminators[] = {
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOffer9terminateEv,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicket9terminateEv,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicket9terminateEv,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicket9terminateEv,
};
const StringInitFn kStringInitializers[] = {
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api19ParameterToGetOffer10initializeEPNS1_6Common10LibContextEPKc,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicket10initializeEPNS1_6Common10LibContextEPKc,
    _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToCancelTicket10initializeEPNS1_6Common10LibContextEPKc,
};
const BodyInitFn kBodyInitializer = _ZN3sce2Np9CppWebApi11Matchmaking2V13Api23ParameterToSubmitTicket10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_23SubmitTicketRequestBodyEEE;
const TextFn kSetView = _ZN3sce2Np9CppWebApi11Matchmaking2V13Api20ParameterToGetTicket7setviewEPKc;

}

int main() {
    for (const auto fn : kSignedOut) Require(fn(nullptr) == kNotSignedIn);
    for (const auto fn : kSetters) Require(fn(nullptr) == 0);

    for (const auto fn : kConstructors) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        Require(fn(storage.bytes) == 0);
        Require(GuardIntact(storage));
    }
    for (const auto fn : kDestructors) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        Require(fn(storage.bytes) == 0);
        Require(GuardIntact(storage));
    }
    for (const auto fn : kTerminators) {
        Storage storage;
        std::memset(storage.bytes, 0, sizeof(storage.bytes));
        Require(fn(storage.bytes) == kNotInitialized);
    }
    for (std::size_t index = 0; index < sizeof(kStringInitializers) / sizeof(kStringInitializers[0]); ++index) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        std::memset(storage.bytes, 0, kParameterSize);
        std::uint64_t context = 1;
        Require(kStringInitializers[index](storage.bytes, &context, nullptr) == kInvalidArgument);
        Require(kStringInitializers[index](storage.bytes, &context, "id") == 0);
        Require(kStringInitializers[index](storage.bytes, &context, "id") == kAlreadyInitialized);
        Require(kTerminators[index](storage.bytes) == 0);
        Require(kTerminators[index](storage.bytes) == kNotInitialized);
        Require(GuardIntact(storage));
    }
    {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        std::memset(storage.bytes, 0, kParameterSize);
        std::uint64_t context = 1;
        IntrusivePtr empty{};
        Require(kBodyInitializer(storage.bytes, &context, &empty) == kBodyMissing);
        IntrusivePtr body{};
        body.object = storage.bytes;
        Require(kBodyInitializer(storage.bytes, &context, &body) == 0);
        Require(kBodyInitializer(storage.bytes, &context, &body) == kAlreadyInitialized);
        Require(kTerminators[3](storage.bytes) == 0);
        Require(kTerminators[3](storage.bytes) == kNotInitialized);
        Require(GuardIntact(storage));
    }
    {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        std::memset(storage.bytes, 0, kParameterSize);
        std::uint64_t context = 1;
        Require(kSetView(storage.bytes, "view") == kParameterNotInitialized);
        Require(kStringInitializers[1](storage.bytes, &context, "id") == 0);
        Require(kSetView(storage.bytes, "view") == 0);
        Require(kSetView(storage.bytes, nullptr) == kInvalidArgument);
        Require(kTerminators[1](storage.bytes) == 0);
        Require(kSetView(storage.bytes, "view") == kParameterNotInitialized);
        Require(GuardIntact(storage));
    }
    return 0;
}
