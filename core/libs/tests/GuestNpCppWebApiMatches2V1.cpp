#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
constexpr int kTransactionNotStarted = static_cast<int>(0x80553508);
constexpr int kAlreadyInitialized = static_cast<int>(0x8055350f);
constexpr int kNotInitialized = static_cast<int>(0x80553510);

void RequireResponseAccessors() {
    alignas(8) unsigned char model[0x160];
    for (std::size_t index = 0; index < sizeof(model); ++index) model[index] = static_cast<unsigned char>(index);
    const void* self = model;
    Require(_ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse10getMatchIdEv(self) == model + 0x30);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse13getActivityIdEv(self) == model + 0x60);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse13getCustomDataEv(self) == model + 0x138);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse15customDataIsSetEv(self));
    model[0x158] = 0;
    Require(!_ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse15customDataIsSetEv(self));
    model[0x158] = 0xff;
    Require(_ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse15customDataIsSetEv(self));
}

void RequireParameterLifecycle() {
    alignas(8) unsigned char parameter[0x10];
    std::memset(parameter, 0xa5, sizeof(parameter));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetailC1Ev(parameter) == 0);
    Require(*reinterpret_cast<void**>(parameter) == nullptr);
    Require(*reinterpret_cast<void**>(parameter + 8) == nullptr);
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail17setnpServiceLabelEi(parameter, 5);
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail7setviewENS5_4ViewE(parameter, 2);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail9terminateEv(parameter) == kNotInitialized);
    std::uint64_t context = 1;
    std::uint64_t zeroContext = 0;
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail10initializeEPNS1_6Common10LibContextEPKc(parameter, nullptr, "id") == kInvalidArgument);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail10initializeEPNS1_6Common10LibContextEPKc(parameter, &zeroContext, "id") == kParameterNotInitialized);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail10initializeEPNS1_6Common10LibContextEPKc(parameter, &context, nullptr) == kInvalidArgument);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail10initializeEPNS1_6Common10LibContextEPKc(parameter, &context, "id") == 0);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail10initializeEPNS1_6Common10LibContextEPKc(parameter, &context, "id") == kAlreadyInitialized);
    auto* implementation = static_cast<unsigned char*>(*reinterpret_cast<void**>(parameter + 8));
    Require(implementation != nullptr);
    Require(*reinterpret_cast<void**>(implementation + 0x30) == &context);
    auto* stored = reinterpret_cast<CommonString*>(implementation);
    Require(stored->buffer != nullptr && std::strcmp(stored->buffer, "id") == 0);
    Require(stored->libContext == &context);
    Require(*reinterpret_cast<std::int32_t*>(implementation + 0x20) == 0);
    Require(implementation[0x24] == 0);
    Require(*reinterpret_cast<std::int32_t*>(implementation + 0x28) == 0);
    Require(implementation[0x2c] == 0);
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail17setnpServiceLabelEi(parameter, 7);
    Require(*reinterpret_cast<std::int32_t*>(implementation + 0x20) == 7);
    Require(implementation[0x24] == 1);
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail7setviewENS5_4ViewE(parameter, 3);
    Require(*reinterpret_cast<std::int32_t*>(implementation + 0x28) == 3);
    Require(implementation[0x2c] == 1);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail9terminateEv(parameter) == 0);
    Require(*reinterpret_cast<void**>(parameter + 8) == nullptr);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail9terminateEv(parameter) == kNotInitialized);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetailD1Ev(parameter) == 0);
}

void RequireSetCustomData() {
    alignas(8) unsigned char request[0xa0]{};
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest13setCustomDataEPKc(request, "payload") == 0);
    Require(request[0x90] == 1);
    auto* created = reinterpret_cast<CommonString*>(request + 0x70);
    Require(created->buffer != nullptr && std::strcmp(created->buffer, "payload") == 0);
    Require(created->bufferSize == 8);
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest13setCustomDataEPKc(request, nullptr) == kInvalidArgument);
    Require(request[0x90] == 1);
    std::free(created->buffer);
    created->buffer = nullptr;

    alignas(8) unsigned char update[0x70]{};
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest13setCustomDataEPKc(update, "detail") == 0);
    Require(update[0x68] == 1);
    auto* updated = reinterpret_cast<CommonString*>(update + 0x48);
    Require(updated->buffer != nullptr && std::strcmp(updated->buffer, "detail") == 0);
    std::free(updated->buffer);
}

int gDestroyed = 0;
void APS5_VABI CountDeleter(void* object) {
    ++gDestroyed;
    *reinterpret_cast<std::int32_t*>(object) = 0;
}

void RequireIntrusivePtrOps() {
    IntrusivePtr handle{};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEC1Ev(&handle);
    Require(handle.object == nullptr && handle.deleter == nullptr && handle.libContext == nullptr);
    alignas(8) unsigned char object[8]{};
    *reinterpret_cast<std::int32_t*>(object) = 1;
    handle.object = object;
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEptEv(&handle) == object);
    handle.object = nullptr;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEED1Ev(&handle);

    alignas(8) unsigned char oldObject[8]{};
    alignas(8) unsigned char newObject[8]{};
    *reinterpret_cast<std::int32_t*>(oldObject) = 1;
    *reinterpret_cast<std::int32_t*>(newObject) = 1;
    IntrusivePtr slot{};
    slot.object = oldObject;
    slot.deleter = CountDeleter;
    slot.libContext = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xfeed));
    gDestroyed = 0;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V124UpdateMatchDetailRequestEE5resetEPS6_(&slot, newObject);
    Require(gDestroyed == 1);
    Require(*reinterpret_cast<std::int32_t*>(oldObject) == 0);
    Require(*reinterpret_cast<std::int32_t*>(newObject) == 2);
    Require(slot.object == newObject);
    Require(slot.deleter == nullptr);
    Require(slot.libContext == reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xfeed)));
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V124UpdateMatchStatusRequestEE5resetEPS6_(&slot, newObject);
    Require(*reinterpret_cast<std::int32_t*>(newObject) == 2);
    Require(gDestroyed == 1);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V124UpdateMatchDetailRequestEE5resetEPS6_(&slot, nullptr);
    Require(*reinterpret_cast<std::int32_t*>(newObject) == 1);
    Require(slot.object == nullptr && slot.deleter == nullptr);
    Require(gDestroyed == 1);
}

void RequireTransactionStubs() {
    alignas(8) unsigned char transaction[0x10];
    std::memset(transaction, 0xa5, sizeof(transaction));
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(transaction) == 0);
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(transaction) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_(transaction, nullptr) == kNotSignedIn);
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() == kNotSignedIn);
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_7Matches2V122GetMatchDetailResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() == kTransactionNotStarted);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi14getMatchDetailEiRKNS4_25ParameterToGetMatchDetailERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_22GetMatchDetailResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE(nullptr) == kNotSignedIn);
}

}  // namespace

int main() {
    RequireResponseAccessors();
    RequireParameterLifecycle();
    RequireSetCustomData();
    RequireIntrusivePtrOps();
    RequireTransactionStubs();
    return 0;
}
