#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <source_location>
#include <span>

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

struct CloudStringField {
    int (APS5_VABI* set)(void*, const char*);
    std::size_t offset;
    std::size_t flag;
};

struct CloudIntegerField {
    void (*set)(void*, std::uint32_t);
    std::size_t offset;
    std::size_t flag;
};

void CheckCloudFields(void* parameter, unsigned char* data, const std::uint64_t* context,
                      std::span<const CloudStringField> strings, std::span<const CloudIntegerField> integers) {
    for (const auto& field : strings) {
        auto* value = reinterpret_cast<CommonString*>(data + field.offset);
        Require(value->libContext == context);
        if (field.flag) Require(data[field.flag] == 0 && !value->buffer);
        char text[] = "status,range";
        Require(field.set(parameter, text) == 0);
        text[0] = 'X';
        Require(value->buffer && std::strcmp(value->buffer, "status,range") == 0 && value->bufferSize == sizeof(text));
        Require(field.set(parameter, value->buffer + 7) == 0);
        Require(std::strcmp(value->buffer, "range") == 0 && value->bufferSize == 6);
        Require(field.set(parameter, nullptr) == kInvalidArgument && std::strcmp(value->buffer, "range") == 0);
        Require(field.set(parameter, "") == 0 && value->buffer && *value->buffer == 0 && value->bufferSize == 1);
        if (field.flag) Require(data[field.flag] == 1);
    }
    for (const auto& field : integers) {
        for (const std::uint32_t input : {0xfedcba98u, 0u}) {
            field.set(parameter, input);
            std::uint32_t stored;
            std::memcpy(&stored, data + field.offset, sizeof(stored));
            Require(stored == input && (!field.flag || data[field.flag] == 1));
        }
    }
}

void TestCloudParameters() {
    const CloudStringField DownloadStringFields[] = {
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData10setifMatchEPKc, 0x80, 0xa0},
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData11setobjectIdEPKc, 0x30, 0x50},
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData12setaccountIdEPKc, 0x0, 0x0},
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData8setrangeEPKc, 0x58, 0x78},
    };

    const CloudIntegerField DownloadIntegerFields[] = {
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData17setnpServiceLabelEj(self, value); }, 0x24, 0x28},
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData9setslotIdEi(self, static_cast<int>(value)); }, 0x20, 0x0},
    };

    const CloudStringField StatusStringFields[] = {
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser10setslotIdsEPKc, 0x20, 0x0},
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser12setaccountIdEPKc, 0x0, 0x0},
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9setfieldsEPKc, 0x40, 0x60},
    };

    const CloudIntegerField StatusIntegerFields[] = {
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser11setsortModeENS5_8SortModeE(self, static_cast<int>(value)); }, 0x7c, 0x80},
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser17setnpServiceLabelEj(self, value); }, 0x84, 0x88},
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser7setsortENS5_4SortE(self, static_cast<int>(value)); }, 0x74, 0x78},
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser8setlimitEi(self, static_cast<int>(value)); }, 0x64, 0x68},
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9setoffsetEi(self, static_cast<int>(value)); }, 0x6c, 0x70},
    };

    const CloudStringField DeleteStringFields[] = {
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser10setslotIdsEPKc, 0x0, 0x0},
        {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser12setaccountIdEPKc, 0x20, 0x0},
    };

    const CloudIntegerField DeleteIntegerFields[] = {
        {[](void* self, std::uint32_t value) { _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser17setnpServiceLabelEj(self, value); }, 0x74, 0x78},
    };
    alignas(8) unsigned char params[64]{};
    std::uint64_t context = 0;
    _ZN3sce2Np9CppWebApi6Common10InitParamsC1Ev(params);
    _ZN3sce2Np9CppWebApi6Common10LibContextC1Ev(&context);
    Require(_ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(params, &context) == 0);
    for (int family = 0; family < 3; ++family) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        auto* p = storage.bytes;
        const SimpleFn destroy[] = {
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataD1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserD1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUserD1Ev,
        };
        SimpleFn construct[] = {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataC1Ev, _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserC1Ev, _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUserC1Ev};
        SimpleFn terminate[] = {_ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData9terminateEv, _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9terminateEv, _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser9terminateEv};
        construct[family](p);
        auto init = [&](const char* first, const char* second) {
            if (family == 0) return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData10initializeEPNS1_6Common10LibContextEPKci(p, &context, first, -7);
            if (family == 1) return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser10initializeEPNS1_6Common10LibContextEPKcSA_(p, &context, first, second);
            return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi32ParameterToDeleteMultiDataByUser10initializeEPNS1_6Common10LibContextEPKcSA_(p, &context, first, second);
        };
        auto strings = family == 0 ? std::span<const CloudStringField>(DownloadStringFields)
                                   : family == 1 ? std::span<const CloudStringField>(StatusStringFields)
                                                 : std::span<const CloudStringField>(DeleteStringFields);
        auto integers = family == 0 ? std::span<const CloudIntegerField>(DownloadIntegerFields)
                                    : family == 1 ? std::span<const CloudIntegerField>(StatusIntegerFields)
                                                  : std::span<const CloudIntegerField>(DeleteIntegerFields);
        for (const auto& f : strings) Require(f.set(p, nullptr) == kParameterNotInitialized);
        for (const auto& f : integers) f.set(p, 0);
        char first[] = "first", second[] = "second";
        Require(init(nullptr, second) == kInvalidArgument);
        if (family != 0) Require(init(first, nullptr) == kInvalidArgument);
        Require(init(first, second) == 0);
        Require(init(nullptr, nullptr) == kAlreadyInitialized);
        unsigned char* data;
        std::memcpy(&data, p + 8, sizeof(data));
        first[0] = second[0] = 'X';
        Require(std::strcmp(reinterpret_cast<const CommonString*>(data)->buffer, "first") == 0);
        if (family == 0) {
            int slot;
            std::memcpy(&slot, data + 0x20, sizeof(slot));
            Require(slot == -7);
        } else {
            Require(std::strcmp(reinterpret_cast<const CommonString*>(data + 0x20)->buffer, "second") == 0);
        }
        CheckCloudFields(p, data, &context, strings, integers);
        const auto live = storage;
        destroy[family](p);
        Require(std::memcmp(live.bytes, p, sizeof(live.bytes)) == 0);
        Require(terminate[family](p) == 0 && terminate[family](p) == kTerminatedUninitialized);
        const auto terminated = storage;
        destroy[family](p);
        Require(std::memcmp(terminated.bytes, p, sizeof(terminated.bytes)) == 0);
        construct[family](p);
        Require(init("", "") == 0 && terminate[family](p) == 0 && GuardIntact(storage));
    }
    Require(_ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(&context) == 0);
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
    TestCloudParameters();
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
