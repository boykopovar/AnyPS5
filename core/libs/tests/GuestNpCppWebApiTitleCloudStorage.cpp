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
constexpr int kTerminatedUninitialized = static_cast<int>(0x80553510);
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

void APS5_VABI NoopDelete(void*) {}

using SimpleFn = int (APS5_VABI*)(void*);
using TextFn = int (APS5_VABI*)(void*, const char*);
using DataFn = int (APS5_VABI*)(void*, const void*, std::size_t);

const SimpleFn kSignedOut[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V129SetDataInfoRequestBodyFactory6createEPNS1_6Common10LibContextEPvmPNS5_12IntrusivePtrINS3_22SetDataInfoRequestBodyEEE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi10uploadDataEiRKNS4_21ParameterToUploadDataERNS1_6Common19UpStreamTransactionINS8_12IntrusivePtrINS3_22UploadDataResponseBodyEEENSA_INS4_25UploadDataResponseHeadersEEEEE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi10uploadDataEiRKNS4_42ParameterWithBinaryRequestBodyToUploadDataERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_22UploadDataResponseBodyEEENSA_INS4_25UploadDataResponseHeadersEEEEE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi11setDataInfoEiRKNS4_22ParameterToSetDataInfoERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS4_26SetDataInfoResponseHeadersEEEEE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi12downloadDataEiRKNS4_23ParameterToDownloadDataERNS1_6Common11TransactionINS8_12IntrusivePtrINS8_6BinaryEEENSA_INS4_27DownloadDataResponseHeadersEEEEE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21deleteMultiDataByUserEiRKNS4_32ParameterToDeleteMultiDataByUserERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
};
const SimpleFn kNumericSetters[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData21setxPsnNpServiceLabelEj,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData22setxPsnAtomicOperationENS5_19XPsnAtomicOperationE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo22setxPsnAtomicOperationENS5_19XPsnAtomicOperationE,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData17setnpServiceLabelEj,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser17setnpServiceLabelEj,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser17setnpServiceLabelEj,
};
const TextFn kTextSetters[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo24setxPsnAtomicOperationIdEPKc,
};
const DataFn kDataSetters[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadData14setRequestBodyEPKhm,
};
const SimpleFn kConstructors[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadDataC1Ev,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfoC1Ev,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUserC1Ev,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadDataC1Ev,
};
const SimpleFn kDestructors[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadDataD1Ev,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfoD1Ev,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUserD1Ev,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadDataD1Ev,
};
const SimpleFn kTerminators[] = {
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData9terminateEv,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo9terminateEv,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser9terminateEv,
    _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadData9terminateEv,
};

bool Constructed(SimpleFn constructor) {
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));
    if (constructor(storage.bytes) != 0) return false;
    if (!GuardIntact(storage)) return false;
    for (std::size_t index = 0; index < kParameterSize; ++index) {
        if (storage.bytes[index] != 0) return false;
    }
    return true;
}

void TestInitializers() {
    const std::uint64_t uninitialized = 0;
    std::uint64_t context = 1;

    {
        Storage parameter;
        std::memset(parameter.bytes, kFill, sizeof(parameter.bytes));
        Require(kConstructors[0](parameter.bytes) == 0 && GuardIntact(parameter));
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, nullptr, "label", 0) == kInvalidArgument);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, &uninitialized, "label", 0) == kParameterNotInitialized);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, &context, nullptr, 0) == kInvalidArgument);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, &context, "label", 0) == 0);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi21ParameterToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, &context, "label", 0) == kAlreadyInitialized);
        Require(kTerminators[0](parameter.bytes) == 0);
        Require(kTerminators[0](parameter.bytes) == kTerminatedUninitialized);
        Require(kDestructors[0](parameter.bytes) == 0 && GuardIntact(parameter));
    }
    {
        Storage parameter;
        std::memset(parameter.bytes, kFill, sizeof(parameter.bytes));
        Require(kConstructors[1](parameter.bytes) == 0 && GuardIntact(parameter));
        IntrusivePtr missing{};
        IntrusivePtr body{parameter.bytes, NoopDelete, nullptr};
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo10initializeEPNS1_6Common10LibContextEPKciNS6_12IntrusivePtrINS3_22SetDataInfoRequestBodyEEE(parameter.bytes, &context, "label", 0, &missing) == kBodyMissing);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo10initializeEPNS1_6Common10LibContextEPKciNS6_12IntrusivePtrINS3_22SetDataInfoRequestBodyEEE(parameter.bytes, &context, "label", 0, &body) == 0);
        Require(kTerminators[1](parameter.bytes) == 0);
        Require(kDestructors[1](parameter.bytes) == 0 && GuardIntact(parameter));
    }
    {
        Storage parameter;
        std::memset(parameter.bytes, kFill, sizeof(parameter.bytes));
        Require(kConstructors[2](parameter.bytes) == 0 && GuardIntact(parameter));
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser10initializeEPNS1_6Common10LibContextEPKcSA_(parameter.bytes, &context, "slot", nullptr) == kInvalidArgument);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser10initializeEPNS1_6Common10LibContextEPKcSA_(parameter.bytes, &context, "slot", "me") == 0);
        Require(kTerminators[2](parameter.bytes) == 0);
        Require(kDestructors[2](parameter.bytes) == 0 && GuardIntact(parameter));
    }
    {
        Storage parameter;
        std::memset(parameter.bytes, kFill, sizeof(parameter.bytes));
        Require(kConstructors[3](parameter.bytes) == 0 && GuardIntact(parameter));
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, &context, nullptr, 0) == kInvalidArgument);
        Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadData10initializeEPNS1_6Common10LibContextEPKci(parameter.bytes, &context, "slot", 0) == 0);
        Require(kTerminators[3](parameter.bytes) == 0);
        Require(kDestructors[3](parameter.bytes) == 0 && GuardIntact(parameter));
    }
}

void TestSetters() {
    for (const auto setter : kNumericSetters) Require(setter(nullptr) == 0);
    Storage parameter;
    std::memset(parameter.bytes, kFill, sizeof(parameter.bytes));
    Require(kConstructors[1](parameter.bytes) == 0);
    for (const auto setter : kTextSetters) Require(setter(parameter.bytes, "id") == kParameterNotInitialized);
    std::uint64_t context = 1;
    IntrusivePtr missing{};
    Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo10initializeEPNS1_6Common10LibContextEPKciNS6_12IntrusivePtrINS3_22SetDataInfoRequestBodyEEE(parameter.bytes, &context, "label", 0, &missing) == kBodyMissing);
    IntrusivePtr body{parameter.bytes, NoopDelete, nullptr};
    Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi22ParameterToSetDataInfo10initializeEPNS1_6Common10LibContextEPKciNS6_12IntrusivePtrINS3_22SetDataInfoRequestBodyEEE(parameter.bytes, &context, "label", 0, &body) == 0);
    Require(kTextSetters[0](parameter.bytes, nullptr) == kInvalidArgument);
    Require(kTextSetters[0](parameter.bytes, "id") == 0);
    Require(kTerminators[1](parameter.bytes) == 0);

    Storage binary;
    std::memset(binary.bytes, kFill, sizeof(binary.bytes));
    Require(kConstructors[3](binary.bytes) == 0);
    for (const auto setter : kDataSetters) Require(setter(binary.bytes, nullptr, 0) == kParameterNotInitialized);
    Require(_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi42ParameterWithBinaryRequestBodyToUploadData10initializeEPNS1_6Common10LibContextEPKci(binary.bytes, &context, "slot", 0) == 0);
    Require(kDataSetters[0](binary.bytes, nullptr, 1) == kInvalidArgument);
    const unsigned char data[] = {1, 2, 3};
    Require(kDataSetters[0](binary.bytes, data, sizeof(data)) == 0);
    Require(kTerminators[3](binary.bytes) == 0);
}

}


constexpr int kTransactionNotStarted = static_cast<int>(0x80553508);

void TestBookkeeping() {
    alignas(8) unsigned char object[8]{};
    IntrusivePtr header{};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEC1Ev(&header);
    Require(header.object == nullptr && header.deleter == nullptr && header.libContext == nullptr);
    header.object = object;
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEE3getEv(&header) == object);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEptEv(&header) == object);
    header.object = nullptr;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEED1Ev(&header);

    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEC1Ev(&header);
    Require(header.object == nullptr && header.deleter == nullptr && header.libContext == nullptr);
    header.object = object;
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEE3getEv(&header) == object);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEptEv(&header) == object);
    header.object = nullptr;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEED1Ev(&header);

    header.object = object;
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi25UploadDataResponseHeadersEE3getEv(&header) == object);
    header.object = nullptr;

    alignas(8) unsigned char transaction[0x10];
    std::memset(transaction, kFill, sizeof(transaction));
    _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEEC1Ev(transaction);
    for (unsigned char byte : transaction) Require(byte == 0);
    _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEED1Ev(transaction);

    std::memset(transaction, kFill, sizeof(transaction));
    _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEEC1Ev(transaction);
    for (unsigned char byte : transaction) Require(byte == 0);
    _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEED1Ev(transaction);

    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE11hasResponseEv(nullptr) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V132GetMultiDataStatusesResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv(nullptr) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE11hasResponseEv(nullptr) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_15DefaultResponseENS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi26SetDataInfoResponseHeadersEEEE11hasResponseEv(nullptr) == 0);

    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE5startEPNS2_10LibContextE(nullptr, nullptr) == kNotSignedIn);
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE5startEPNS2_10LibContextE(nullptr, nullptr) == kNotSignedIn);
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS2_6BinaryEEENS4_INS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE6finishEv(nullptr) == kTransactionNotStarted);
    Require(_ZN3sce2Np9CppWebApi6Common19UpStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V122UploadDataResponseBodyEEENS4_INS6_7DataApi25UploadDataResponseHeadersEEEE6finishEv(nullptr) == kTransactionNotStarted);
}

int main() {
    TestBookkeeping();
    for (const auto fn : kSignedOut) Require(fn(nullptr) == kNotSignedIn);
    for (const auto constructor : kConstructors) Require(Constructed(constructor));
    for (const auto destructor : kDestructors) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        Require(destructor(storage.bytes) == 0);
        Require(GuardIntact(storage));
    }
    for (const auto terminator : kTerminators) {
        Storage storage;
        std::memset(storage.bytes, 0, sizeof(storage.bytes));
        Require(terminator(storage.bytes) == kTerminatedUninitialized);
    }
    TestInitializers();
    TestSetters();
    alignas(8) IntrusivePtr emptyVector{};
    CommonIterator iterator{};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEEE5beginEv(&iterator, &emptyVector) == &iterator);
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEEE3endEv(&iterator, &emptyVector) == &iterator);
    Require(iterator.element == nullptr);
    return 0;
}
