#include "prx/libSceIpmi/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>

namespace {

void Store32(std::byte* object, std::size_t offset, std::uint32_t value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

void Store64(std::byte* object, std::size_t offset, std::uint64_t value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

void InitInvocationConfig(std::byte* config) {
    Store32(config, 0x00, 1);
    Store64(config, 0x08, 0xf00);
    Store64(config, 0x10, 0xf00);
}

void InitMsgQueueConfig(std::byte* config) {
    Store32(config, 0x00, 0);
    std::memset(config + 0x08, 0, 0x100);
}

void InitClientConfig(std::byte* config) {
    config[0x00] = std::byte{0};
    config[0x19] = std::byte{0};
    Store32(config, 0x1c, 0);
    InitInvocationConfig(config + 0x20);
    InitInvocationConfig(config + 0x38);
    InitMsgQueueConfig(config + 0x50);
}

void InitServerConfig(std::byte* config) {
    Store64(config, 0x00, 0xf00);
    Store64(config, 0x08, 0xf00);
    Store64(config, 0x10, 0);
    std::memset(config + 0x18, 0, 2);
    std::memset(config + 0x32, 0, 2);
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
    const auto* values = static_cast<const std::uint64_t*>(self);
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
