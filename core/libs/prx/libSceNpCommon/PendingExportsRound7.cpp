#include "prx/libSceNpCommon/include/Api.hpp"
#include <array>
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

extern "C" {

int APS5_VABI _ZN3sce2np10CancelableC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilXmlEscape(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np4CondD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceNpAllocatorExRealloc(void) {
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

int APS5_VABI _ZN3sce2np7RingBuf4dtorEv(void) {
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

int APS5_VABI _ZN3sce2np4Cond4ctorEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np16StreamReadBufferD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce2np9EventFlag6IsInitEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np10CancelLockC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np13NpTitleSecretC2ERK16SceNpTitleSecret(void) {
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

int APS5_VABI _ZN3sce2np9Semaphore4WaitEj(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2npeqERKNS0_4TimeES3_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np13RingBufMemory6ExpandEm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn8_N3sce2np10MemoryFile4ReadEPNS0_6HandleEPvmPm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7RingBuf15CheckoutForReadEPm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9EventFlag4OpenEPKc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np13NpTitleSecret5ClearEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI _ZN3sce2np4Cond4InitEPKcPNS0_5MutexE(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient10DisconnectEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient11IsConnectedEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np12HttpTemplate19SetAuthInfoCallbackEPFii15SceHttpAuthTypePKcPcS5_iPPhPmPiPvESA_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilParseJid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9Semaphore6CreateEiiPKc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2npleERKNS0_4TimeERK10SceRtcTick(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilXmlUnescape(void) {
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

int APS5_VABI _ZN3sce2np9EventFlag4PollEmjPm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9EventFlagD0Ev(void) {
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

int APS5_VABI _ZN3sce2np7RingBuf14CheckinForReadEm(void) {
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

int APS5_VABI _ZN3sce2np9EventFlag3SetEm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce2np7RingBuf6IsFullEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI _ZNK3sce2np4Time18ConvertToPosixTimeEPl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7Callout10IsTimedoutEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI _sceNpAllocatorExFree(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient7ConnectEPKvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7RingBufC1Ev(void) {
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

int APS5_VABI _ZN3sce2np13NpTitleSecretC2ERKS1_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np14CalloutContextD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np5Mutex7DestroyEv(void) {
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

int APS5_VABI _ZN3sce2np9EventFlag6CreateEPKcj(void) {
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

int APS5_VABI _ZN3sce2np9SemaphoreD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9EventFlagD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceNpAllocatorMalloc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2npneERK10SceRtcTickRKNS0_4TimeE(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np13RingBufMemory6IsInitEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7RingBufD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI _ZNK3sce2np10EventQueue6IsInitEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI _ZN3sce2np13RingBufMemory7DestroyEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9LocalFileC1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np7RingBufD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np4PathD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np4CondD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np9EventFlag7DestroyEv(void) {
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

int APS5_VABI _ZN3sce2np4Time8AddHoursEl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np12WorkerThreadD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np13RingBufMemoryD0Ev(void) {
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

int APS5_VABI _ZN3sce2np8JsonNull5ClearEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np4Time12GetUserClockEPS1_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient12AbortRequestEii(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce2np5Mutex6IsInitEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
