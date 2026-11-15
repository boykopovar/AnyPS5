#include "prx/libSceNpCommon/include/Api.hpp"
#include <array>
#include <array>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void copyGuest(const void* source, void* destination, std::size_t size) {
    if (!source || !GuestCopyIn_nid_no_patch(destination, source, size))
        throw std::runtime_error("libSceNpCommon comparison operand is unreadable");
}

void copyGuestOffset(const void* source, std::size_t offset, void* destination, std::size_t size) {
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    if (!source || offset > UINTPTR_MAX - address)
        throw std::runtime_error("libSceNpCommon comparison operand address is invalid");
    copyGuest(reinterpret_cast<const void*>(address + offset), destination, size);
}

}

namespace {

template <typename TValue>
bool ReadNpObjectField(const void* object, std::size_t offset, TValue& value) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address == 0 || offset > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    try {
        return GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address + offset), sizeof(value));
    } catch (...) {
        return false;
    }
}

template <typename TValue>
bool WriteNpObjectField(void* object, std::size_t offset, const TValue& value) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address == 0 || offset > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    try {
        return GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(address + offset), &value, sizeof(value));
    } catch (...) {
        return false;
    }
}

}

extern "C" {

int APS5_VABI _ZN3sce2np10CancelableC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np8WorkItem6CancelEi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9WorkQueue11GetItemByIdEi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np14JsonDocBuilderC2ERKNS0_9JsonValueE(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn16_N3sce2np9HttpTransD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10MemoryFileD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10EventQueueC2EP16SceNpAllocatorEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np16StreamReadBufferD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZNK3sce2np9EventFlag6IsInitEv(const void* self) {
    std::int32_t initialized = 0;
    return ReadNpObjectField(self, 0x10, initialized) && initialized != 0;
}

int APS5_VABI _ZN3sce2np10CancelLockC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np8WorkItem10SetRunningEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient21CreateRequestForAsyncEPiiPKvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np18HttpConnectionPool13InvalidateAllEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient21DeleteRequestForAsyncEii(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np15CancelableScopeD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np8JsonFileD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9JsonArray12AddItemArrayEPPS1_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np14CalloutContextD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn8_N3sce2np10MemoryFile4ReadEPNS0_6HandleEPvmPm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _ZN3sce2np13NpTitleSecret5ClearEv(void* self) {
    const std::array<std::byte, 0x80> cleared{};
    WriteNpObjectField(self, 0x8, cleared);
}

int APS5_VABI _ZN3sce2np10EventQueue7EnqueueEPKvmj(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np12HttpTemplateD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9WorkQueueD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn16_N3sce2np9HttpTrans5WriteEPNS0_6HandleEPKvmPm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10EventQueue4ctorEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np20BufferedStreamReaderD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient7ConnectEPKvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient10DisconnectEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZN3sce2np3ipc10IpmiClient11IsConnectedEv(void* self) {
    std::int32_t state = 0;
    return ReadNpObjectField(self, 0x18, state) && state == 0;
}

int APS5_VABI _ZN3sce2np12HttpTemplate19SetAuthInfoCallbackEPFii15SceHttpAuthTypePKcPcS5_iPPhPmPiPvESA_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpXmlParse(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9WorkQueue15GetFinishedItemENS0_14WorkItemStatusE(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9LocalFile6RemoveEPKc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np14JsonDocBuilder12BuildBufSizeEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10MemoryFile8TruncateEl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9HttpTrans24GetResponseContentLengthEPNS0_6HandleEPbPm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient13CreateRequestEPiiPKvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np8SelectorD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10CancelableD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7HttpUriD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np15CancelableScopeD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10Cancelable7DestroyEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn8_N3sce2np9LocalFileD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce2np10JsonString6GetStrEPcm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10Cancelable10LockCancelEPKciS3_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np8WorkItem10SetPendingEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7Callout4StopEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np6ThreadD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(const void* left, const void* right) {
    std::array<std::uint8_t, 12> leftId{};
    std::array<std::uint8_t, 12> rightId{};
    copyGuest(left, leftId.data(), leftId.size());
    copyGuestOffset(right, 8, rightId.data(), rightId.size());
    return std::memcmp(leftId.data(), rightId.data(), leftId.size()) == 0;
}

bool APS5_VABI _ZNK3sce2np7RingBuf6IsFullEv(const void* self) {
    std::uint64_t readPosition = 0;
    std::uint64_t writePosition = 0;
    return ReadNpObjectField(self, 0x20, readPosition) && ReadNpObjectField(self, 0x28, writePosition) &&
           readPosition == writePosition;
}

bool APS5_VABI _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(const void* left, const void* right) {
    std::array<std::uint8_t, 128> leftSecret{};
    std::array<std::uint8_t, 128> rightSecret{};
    copyGuestOffset(left, 8, leftSecret.data(), leftSecret.size());
    copyGuest(right, rightSecret.data(), rightSecret.size());
    return std::memcmp(leftSecret.data(), rightSecret.data(), leftSecret.size()) != 0;
}

int APS5_VABI _ZN3sce2np9LocalFileD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np16StreamReadBufferD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient11TermServiceEi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np6HandleD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient17UnregisterServiceEi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9WorkQueue4ctorEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np6Thread4InitEPKcimm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZN3sce2np7Callout10IsTimedoutEv(void* self) {
    std::int32_t state = 0;
    return ReadNpObjectField(self, 0x10, state) && (state > 0 || state == -2);
}

int APS5_VABI _ZN3sce2np7Callout11CalloutFuncEPv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn8_N3sce2np6HandleD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np6Handle4InitEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn16_N3sce2np10MemoryFileD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient7ConnectEPKvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10MemoryFileD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9HttpTrans21GetResponseStatusCodeEPNS0_6HandleEPi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np14CalloutContextD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np14JsonDocBuilderC1ERKNS0_9JsonValueE(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9HttpTransD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np12HttpTemplateC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient15RegisterServiceEPKNS1_17ServiceClientInfoE(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np8WorkItemD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceNpAllocatorMalloc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZN3sce2np13RingBufMemory6IsInitEv(void* self) {
    std::uintptr_t buffer = 0;
    return ReadNpObjectField(self, 0x8, buffer) && buffer != 0;
}

int APS5_VABI _sceNpHeapMalloc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np12HttpTemplate7DestroyEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np15CancelableScope5BeginEPNS0_6HandleEPKciS5_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZNK3sce2np10EventQueue6IsInitEv(const void* self) {
    std::uint16_t initialized = 0;
    return ReadNpObjectField(self, 0x98, initialized) && initialized != 0;
}

int APS5_VABI _ZN3sce2np8JsonBool5ClearEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9LocalFile8TruncateEl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce2np10JsonNumber6GetNumEPcm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9LocalFileC1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np4PathD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9HttpTransC1EP16SceNpAllocatorEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np6Thread4dtorEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np12WorkerThreadD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9LocalFile5CloseEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce2np10JsonString9GetLengthEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10EventQueueD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _ZN3sce2np8JsonNull5ClearEv(void* self) {
    static_cast<void>(self);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient12AbortRequestEii(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZNK3sce2np5Mutex6IsInitEv(const void* self) {
    std::uint8_t initialized = 0;
    return ReadNpObjectField(self, 0x10, initialized) && initialized != 0;
}

}
