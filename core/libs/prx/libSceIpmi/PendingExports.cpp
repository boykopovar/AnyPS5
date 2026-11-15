#include "prx/libSceIpmi/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

bool ValidSpan(const void* address, std::size_t bytes) {
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    return start != 0 && (bytes == 0 || bytes - 1 <= std::numeric_limits<std::uintptr_t>::max() - start);
}

void EnsureWritable(std::byte* config, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool writable = ValidSpan(config, bytes) && GuestWritable_nid_no_patch(config, bytes);
    errno = savedErrno;
    if (!writable) throw std::runtime_error(message);
}

void CopyOutChecked(std::byte* object, std::size_t offset, const void* source, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(object + offset, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(message);
}

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool copied = ValidSpan(source, bytes) && GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(message);
}

void Store8(std::byte* object, std::size_t offset, std::uint8_t value) {
    CopyOutChecked(object, offset, &value, sizeof(value), "IPMI::Config: unwritable config");
}

void Store32(std::byte* object, std::size_t offset, std::uint32_t value) {
    CopyOutChecked(object, offset, &value, sizeof(value), "IPMI::Config: unwritable config");
}

void Store64(std::byte* object, std::size_t offset, std::uint64_t value) {
    CopyOutChecked(object, offset, &value, sizeof(value), "IPMI::Config: unwritable config");
}

void WriteZeros(std::byte* object, std::size_t offset, std::size_t bytes) {
    static constexpr std::array<std::byte, 0x100> zeros{};
    if (bytes > zeros.size()) throw std::runtime_error("IPMI::Config: zero fill overflow");
    CopyOutChecked(object, offset, zeros.data(), bytes, "IPMI::Config: unwritable config");
}

void WriteInvocationFields(std::byte* config) {
    Store32(config, 0x00, 1);
    Store64(config, 0x08, 0xf00);
    Store64(config, 0x10, 0xf00);
}

void WriteMsgQueueFields(std::byte* config) {
    Store32(config, 0x00, 0);
    WriteZeros(config, 0x08, 0x100);
}

void InitInvocationConfig(std::byte* config) {
    EnsureWritable(config, 0x18, "IPMI::InvocationConfig: unwritable config");
    WriteInvocationFields(config);
}

void InitMsgQueueConfig(std::byte* config) {
    EnsureWritable(config, 0x108, "IPMI::MsgQueueConfig: unwritable config");
    WriteMsgQueueFields(config);
}

void InitClientConfig(std::byte* config) {
    EnsureWritable(config, 0x158, "IPMI::Client::Config: unwritable config");
    Store8(config, 0x00, 0);
    Store8(config, 0x19, 0);
    Store32(config, 0x1c, 0);
    WriteInvocationFields(config + 0x20);
    WriteInvocationFields(config + 0x38);
    WriteMsgQueueFields(config + 0x50);
}

void InitServerConfig(std::byte* config) {
    EnsureWritable(config, 0x34, "IPMI::Server::Config: unwritable config");
    Store64(config, 0x00, 0xf00);
    Store64(config, 0x08, 0xf00);
    Store64(config, 0x10, 0);
    WriteZeros(config, 0x18, 2);
    WriteZeros(config, 0x32, 2);
}

}

extern "C" {

int APS5_VABI _ZN4IPMI4impl11SessionImplC1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
std::size_t APS5_VABI _ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(std::size_t messageSize, std::uint32_t count) {
    return (messageSize + 0x10) * count;
}
void APS5_VABI _ZN4IPMI6Server12EventHandler21onAsyncMethodDispatchEPNS_7SessionEjjPvmS4_m(void* self) {
    (void)self;
}
void APS5_VABI _ZN4IPMI6Client6Config26SyncMethodInvocationConfigC1Ev(void* self) {
    InitInvocationConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Server12EventHandler20onSyncMethodDispatchEPNS_7SessionEjPvmmS4_m(void* self) {
    (void)self;
}
void APS5_VABI _ZN4IPMI7SessionD1Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl9tryGetMsgEjPvPmm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6Server12EventHandler20onSyncMethodDispatchEPNS_7SessionEjPKNS_8DataInfoEjPNS_10BufferInfoEj(void* self) {
    (void)self;
}
void APS5_VABI _ZN4IPMI6Client6Config14MsgQueueConfigC1Ev(void* self) {
    InitMsgQueueConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6ServerD2Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI6ServerD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
std::size_t APS5_VABI _ZN4IPMI6Client6Config24estimateClientMemorySizeEv(void* self) {
    (void)self;
    return 0x1000;
}
int APS5_VABI _ZN4IPMI6Server6createEPPS0_PKNS0_6ConfigEPvS6_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6Client6ConfigC2Ev(void* self) {
    InitClientConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Server12EventHandler15onSessionKilledEPNS_7SessionE(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI4impl11SessionImplD1Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl27respondToAsyncMethodRequestEjiPKvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6Client6Config27AsyncMethodInvocationConfigC2Ev(void* self) {
    InitInvocationConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Server12EventHandler21onAsyncMethodDispatchEPNS_7SessionEjjPKNS_8DataInfoEj(void* self) {
    (void)self;
}
void APS5_VABI _ZN4IPMI6ClientD2Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl7sendMsgEjPKvmPj(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI6ClientC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI7SessionD2Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZNK4IPMI4impl11SessionImpl16isPeerPrivilegedEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI7SessionC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI4impl11SessionImplD2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6ClientD1Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI6Server12EventHandlerD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
std::size_t APS5_VABI _ZN4IPMI7Session6Config25estimateSessionMemorySizeEv(void* self) {
    (void)self;
    return 0;
}
std::size_t APS5_VABI _ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(const void* self) {
    std::uint64_t values[2]{};
    CopyInChecked(values, self, sizeof(values), "IPMI::Server::Config: unreadable config");
    const std::uint64_t fromFirst = values[0] + 0x100;
    const std::uint64_t fromSecond = values[1] + 0x100;
    return fromFirst >= fromSecond ? fromFirst : fromSecond;
}
int APS5_VABI _ZN4IPMI4impl11SessionImplD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl7destroyEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6Client6Config27AsyncMethodInvocationConfigC1Ev(void* self) {
    InitInvocationConfig(static_cast<std::byte*>(self));
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl6getMsgEjPvPmmPj(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl27respondToAsyncMethodRequestEjjiPKNS_8DataInfoEj(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6Server12EventHandlerD2Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI6ClientD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI6ServerC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI4impl11SessionImplC2Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI _ZN4IPMI7SessionD0Ev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6ServerD1Ev(void* self) {
    (void)self;
}
void APS5_VABI _ZN4IPMI6Server6ConfigC1Ev(void* self) {
    InitServerConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Server12EventHandlerD1Ev(void* self) {
    (void)self;
}
int APS5_VABI _ZN4IPMI4impl11SessionImpl9getServerEv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
void APS5_VABI _ZN4IPMI6Client6ConfigC1Ev(void* self) {
    InitClientConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Client6Config26SyncMethodInvocationConfigC2Ev(void* self) {
    InitInvocationConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Client6Config14MsgQueueConfigC2Ev(void* self) {
    InitMsgQueueConfig(static_cast<std::byte*>(self));
}
void APS5_VABI _ZN4IPMI6Server6ConfigC2Ev(void* self) {
    InitServerConfig(static_cast<std::byte*>(self));
}

}
