#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <cstring>

static constexpr int SCE_AMPR_ERROR_BUFFER_FULL = 0x8002001C;

static int Append(Apr::CommandBufferObject* buffer, const void* command, uint32_t bytes) {
    if (!buffer->base || bytes > buffer->size - buffer->offset) return SCE_AMPR_ERROR_BUFFER_FULL;
    std::memcpy(buffer->base + buffer->offset, command, bytes);
    buffer->offset += bytes;
    ++buffer->numCommands;
    return 0;
}

template<class TCommand>
static int AppendCommand(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, TCommand command) {
    if (!buffer) return static_cast<int>(0x80020016);
    command.header = {opcode, sizeof(command)};
    return Append(buffer, &command, sizeof(command));
}

extern "C" {

int APS5_VABI sceAmprCommandBufferWriteAddressOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint64_t value) {
    if (!address) return static_cast<int>(0x80020016);
    return AppendCommand(buffer, Apr::Opcode::WriteAddress, Apr::WriteAddressCommand{{}, reinterpret_cast<std::uint64_t>(address), value, 0, 0});
}

int APS5_VABI sceAmprCommandBufferWriteCounterOnCompletion(Apr::CommandBufferObject* buffer, std::uint32_t counter, std::uint32_t value) {
    return AppendCommand(buffer, Apr::Opcode::WriteCounter, Apr::WriteCounterCommand{{}, counter, value});
}

int APS5_VABI sceAmprCommandBufferWaitOnAddress(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t compare) {
    return AppendCommand(buffer, Apr::Opcode::WaitOnAddress, Apr::WaitCommand{{}, reinterpret_cast<std::uint64_t>(address), reference, mask, 0, compare});
}

int APS5_VABI sceAmprCommandBufferWaitOnCounter(Apr::CommandBufferObject* buffer, std::uint32_t counter, std::uint32_t reference, std::uint32_t compare) {
    return AppendCommand(buffer, Apr::Opcode::WaitOnCounter, Apr::WaitCommand{{}, 0, reference, ~0ull, counter, compare});
}

int APS5_VABI sceAmprCommandBufferWriteKernelEventQueueOnCompletion(Apr::CommandBufferObject* buffer, std::uint64_t equeue, std::uint64_t ident, std::uint64_t data, std::uint64_t userData) {
    return AppendCommand(buffer, Apr::Opcode::WriteKernelEventQueue, Apr::WriteKernelEventQueueCommand{{}, equeue, ident, data, userData});
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromTimeCounterOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address) {
    return AppendCommand(buffer, Apr::Opcode::WriteAddressFromTimeCounter, Apr::WriteAddressFromCounterCommand{{}, reinterpret_cast<std::uint64_t>(address), 0, 0});
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint32_t counter) {
    return AppendCommand(buffer, Apr::Opcode::WriteAddressFromCounter, Apr::WriteAddressFromCounterCommand{{}, reinterpret_cast<std::uint64_t>(address), counter, 0});
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint32_t counter0, std::uint32_t counter1) {
    return AppendCommand(buffer, Apr::Opcode::WriteAddressFromCounterPair, Apr::WriteAddressFromCounterCommand{{}, reinterpret_cast<std::uint64_t>(address), counter0, counter1});
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressOnCompletion() { return sizeof(Apr::WriteAddressCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteCounterOnCompletion() { return sizeof(Apr::WriteCounterCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnAddress() { return sizeof(Apr::WaitCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnCounter() { return sizeof(Apr::WaitCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteKernelEventQueueOnCompletion() { return sizeof(Apr::WriteKernelEventQueueCommand); }

int APS5_VABI sceAmprAprCommandBufferConstructor(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState) {
    buffer->type = Apr::BufferType::Apr;
    *gatherState = 0;
    *scatterState = 0;
    return 0;
}

int APS5_VABI sceAmprAprCommandBufferDestructor(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState) {
    (void)buffer;
    (void)gatherState;
    (void)scatterState;
    return 0;
}

int APS5_VABI sceAmprAprCommandBufferMapBegin() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprAprCommandBufferMapDirectBegin() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprAprCommandBufferMapEnd() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprAprCommandBufferReadFile(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState, uint32_t fileId, void* destination, uint64_t size, uint64_t offset) {
    (void)gatherState;
    (void)scatterState;
    Apr::ReadFileCommand command{};
    command.header = {Apr::Opcode::ReadFile, sizeof(command)};
    command.fileId = fileId;
    command.destination = reinterpret_cast<uint64_t>(destination);
    command.size = size;
    command.offset = offset;
    return Append(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAprCommandBufferReadFileGather() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprAprCommandBufferReadFileGatherScatter() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprAprCommandBufferReadFileScatter() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprAprCommandBufferResetGatherScatterState() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferClearBuffer(Apr::CommandBufferObject* buffer) {
    buffer->base = nullptr;
    buffer->size = 0;
    buffer->offset = 0;
    buffer->numCommands = 0;
    return 0;
}

int APS5_VABI sceAmprCommandBufferConstructMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferConstructNop() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferConstructor(Apr::CommandBufferObject* buffer) {
    *buffer = {nullptr, 0, 0, 0, Apr::BufferType::Generic};
    return 0;
}

int APS5_VABI sceAmprCommandBufferDestructor(Apr::CommandBufferObject* buffer) {
    (void)buffer;
    return 0;
}

void* APS5_VABI sceAmprCommandBufferGetBufferBaseAddress(const Apr::CommandBufferObject* buffer) {
    return buffer->base;
}

uint32_t APS5_VABI sceAmprCommandBufferGetCurrentOffset(const Apr::CommandBufferObject* buffer) {
    return buffer->offset;
}

uint32_t APS5_VABI sceAmprCommandBufferGetNumCommands(const Apr::CommandBufferObject* buffer) {
    return buffer->numCommands;
}

uint32_t APS5_VABI sceAmprCommandBufferGetSize(const Apr::CommandBufferObject* buffer) {
    return buffer->size;
}

uint32_t APS5_VABI sceAmprCommandBufferGetType(const Apr::CommandBufferObject* buffer) {
    return static_cast<uint32_t>(buffer->type);
}

int APS5_VABI sceAmprCommandBufferNop() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferNopWithData() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferPopMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferPushMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferPushMarkerWithColor() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferReset(Apr::CommandBufferObject* buffer) {
    buffer->offset = 0;
    buffer->numCommands = 0;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetBuffer(Apr::CommandBufferObject* buffer, void* memory, uint32_t size) {
    buffer->base = static_cast<uint8_t*>(memory);
    buffer->size = size;
    buffer->offset = 0;
    buffer->numCommands = 0;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferSetMarkerWithColor() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWaitOnAddress_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWaitOnCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterPair_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromTimeCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWriteAddress_04_00(Apr::CommandBufferObject* buffer, uint64_t* address, uint64_t value, uint32_t flags) {
    Apr::WriteAddressCommand command{};
    command.header = {Apr::Opcode::WriteAddress, sizeof(command)};
    command.address = reinterpret_cast<uint64_t>(address);
    command.value = value;
    command.flags = flags;
    return Append(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprCommandBufferWriteCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprCommandBufferWriteKernelEventQueue_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeMapBegin() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeMapDirectBegin() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeMapEnd() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeNop() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeNopWithData() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizePopMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizePushMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizePushMarkerWithColor() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAmprMeasureCommandSizeReadFile(void) {
    return sizeof(Apr::ReadFileCommand);
}

int APS5_VABI sceAmprMeasureCommandSizeReadFileGather() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeReadFileGatherScatter() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeReadFileScatter() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeResetGatherScatterState() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeSetMarker() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeSetMarkerWithColor() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeWaitOnAddress_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeWaitOnCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromTimeCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAmprMeasureCommandSizeWriteAddress_04_00(void) {
    return sizeof(Apr::WriteAddressCommand);
}

int APS5_VABI sceAmprMeasureCommandSizeWriteCounter_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAmprMeasureCommandSizeWriteKernelEventQueue_04_00() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


APS5_EXPORT("0hlfW1O4Aa4", sceAmprUnknown00);
int APS5_VABI sceAmprUnknown00(void) {
    NotImplemented_nid_no_patch("0hlfW1O4Aa4");
    return 0;
}

APS5_EXPORT("802pFCwC9w0", sceAmprUnknown01);
int APS5_VABI sceAmprUnknown01(void) {
    NotImplemented_nid_no_patch("802pFCwC9w0");
    return 0;
}

APS5_EXPORT("LTov9gMEqCU", sceAmprUnknown02);
int APS5_VABI sceAmprUnknown02(void) {
    NotImplemented_nid_no_patch("LTov9gMEqCU");
    return 0;
}

APS5_EXPORT("MU25eqxSDTw", sceAmprUnknown03);
int APS5_VABI sceAmprUnknown03(void) {
    NotImplemented_nid_no_patch("MU25eqxSDTw");
    return 0;
}

APS5_EXPORT("W0j6vCxh9Pc", sceAmprUnknown04);
int APS5_VABI sceAmprUnknown04(void) {
    NotImplemented_nid_no_patch("W0j6vCxh9Pc");
    return 0;
}

APS5_EXPORT("YvmY5Jf0VYU", sceAmprUnknown05);
int APS5_VABI sceAmprUnknown05(void) {
    NotImplemented_nid_no_patch("YvmY5Jf0VYU");
    return 0;
}

APS5_EXPORT("a-z7wxuYO2E", sceAmprUnknown06);
int APS5_VABI sceAmprUnknown06(void) {
    NotImplemented_nid_no_patch("a-z7wxuYO2E");
    return 0;
}

APS5_EXPORT("dGYo9mE8K2A", sceAmprUnknown07);
int APS5_VABI sceAmprUnknown07(void) {
    NotImplemented_nid_no_patch("dGYo9mE8K2A");
    return 0;
}

APS5_EXPORT("gjLRZgfb3i0", sceAmprUnknown08);
int APS5_VABI sceAmprUnknown08(void) {
    NotImplemented_nid_no_patch("gjLRZgfb3i0");
    return 0;
}

APS5_EXPORT("np6xXcXEnXE", sceAmprUnknown09);
int APS5_VABI sceAmprUnknown09(void) {
    NotImplemented_nid_no_patch("np6xXcXEnXE");
    return 0;
}

APS5_EXPORT("xZqiZvmcp9k", sceAmprUnknown10);
int APS5_VABI sceAmprUnknown10(void) {
    NotImplemented_nid_no_patch("xZqiZvmcp9k");
    return 0;
}

APS5_EXPORT("-mLzBSk-VGs", sceAmprUnknown11);
int APS5_VABI sceAmprUnknown11(void) {
    NotImplemented_nid_no_patch("-mLzBSk-VGs");
    return 0;
}

APS5_EXPORT("-vXEQdRADLI", sceAmprUnknown12);
int APS5_VABI sceAmprUnknown12(void) {
    NotImplemented_nid_no_patch("-vXEQdRADLI");
    return 0;
}

APS5_EXPORT("0uuqgRz9qfo", sceAmprUnknown13);
int APS5_VABI sceAmprUnknown13(void) {
    NotImplemented_nid_no_patch("0uuqgRz9qfo");
    return 0;
}

APS5_EXPORT("1HYEoANqZ1w", sceAmprUnknown14);
int APS5_VABI sceAmprUnknown14(void) {
    NotImplemented_nid_no_patch("1HYEoANqZ1w");
    return 0;
}

APS5_EXPORT("2B+V3qCqz4s", sceAmprUnknown15);
int APS5_VABI sceAmprUnknown15(void) {
    NotImplemented_nid_no_patch("2B+V3qCqz4s");
    return 0;
}

APS5_EXPORT("2UFh+YKfuzk", sceAmprUnknown16);
int APS5_VABI sceAmprUnknown16(void) {
    NotImplemented_nid_no_patch("2UFh+YKfuzk");
    return 0;
}

APS5_EXPORT("6f5f-qx4ucA", sceAmprUnknown17);
int APS5_VABI sceAmprUnknown17(void) {
    NotImplemented_nid_no_patch("6f5f-qx4ucA");
    return 0;
}

APS5_EXPORT("CSEjkTYt5dw", sceAmprUnknown18);
int APS5_VABI sceAmprUnknown18(void) {
    NotImplemented_nid_no_patch("CSEjkTYt5dw");
    return 0;
}

APS5_EXPORT("ENLfKJEZTjE", sceAmprUnknown19);
int APS5_VABI sceAmprUnknown19(void) {
    NotImplemented_nid_no_patch("ENLfKJEZTjE");
    return 0;
}

APS5_EXPORT("Jo9ON-AX9eU", sceAmprUnknown20);
int APS5_VABI sceAmprUnknown20(void) {
    NotImplemented_nid_no_patch("Jo9ON-AX9eU");
    return 0;
}

APS5_EXPORT("KfcTPbeaOqg", sceAmprUnknown21);
int APS5_VABI sceAmprUnknown21(void) {
    NotImplemented_nid_no_patch("KfcTPbeaOqg");
    return 0;
}

APS5_EXPORT("ODGONXcSmz4", sceAmprUnknown22);
int APS5_VABI sceAmprUnknown22(void) {
    NotImplemented_nid_no_patch("ODGONXcSmz4");
    return 0;
}

APS5_EXPORT("S5WbPO54nD0", sceAmprUnknown23);
int APS5_VABI sceAmprUnknown23(void) {
    NotImplemented_nid_no_patch("S5WbPO54nD0");
    return 0;
}

APS5_EXPORT("VUzjXknPPBs", sceAmprUnknown24);
int APS5_VABI sceAmprUnknown24(void) {
    NotImplemented_nid_no_patch("VUzjXknPPBs");
    return 0;
}

APS5_EXPORT("a54t8+k7KpY", sceAmprUnknown25);
int APS5_VABI sceAmprUnknown25(void) {
    NotImplemented_nid_no_patch("a54t8+k7KpY");
    return 0;
}

APS5_EXPORT("aeHxLWwq0gQ", sceAmprUnknown26);
int APS5_VABI sceAmprUnknown26(void) {
    NotImplemented_nid_no_patch("aeHxLWwq0gQ");
    return 0;
}

APS5_EXPORT("apHKv46QaCw", sceAmprUnknown27);
int APS5_VABI sceAmprUnknown27(void) {
    NotImplemented_nid_no_patch("apHKv46QaCw");
    return 0;
}

APS5_EXPORT("eMnBe5mZFLw", sceAmprUnknown28);
int APS5_VABI sceAmprUnknown28(void) {
    NotImplemented_nid_no_patch("eMnBe5mZFLw");
    return 0;
}

APS5_EXPORT("fRWufXAccuI", sceAmprUnknown29);
int APS5_VABI sceAmprUnknown29(void) {
    NotImplemented_nid_no_patch("fRWufXAccuI");
    return 0;
}

APS5_EXPORT("jAO1IJKMhE4", sceAmprUnknown30);
int APS5_VABI sceAmprUnknown30(void) {
    NotImplemented_nid_no_patch("jAO1IJKMhE4");
    return 0;
}

APS5_EXPORT("jfRI3snge3o", sceAmprUnknown31);
int APS5_VABI sceAmprUnknown31(void) {
    NotImplemented_nid_no_patch("jfRI3snge3o");
    return 0;
}

APS5_EXPORT("qPe7-h5Jnuc", sceAmprUnknown32);
int APS5_VABI sceAmprUnknown32(void) {
    NotImplemented_nid_no_patch("qPe7-h5Jnuc");
    return 0;
}

APS5_EXPORT("qlWiRfOJx1A", sceAmprUnknown33);
int APS5_VABI sceAmprUnknown33(void) {
    NotImplemented_nid_no_patch("qlWiRfOJx1A");
    return 0;
}

APS5_EXPORT("sZLrjx-yEx4", sceAmprUnknown34);
int APS5_VABI sceAmprUnknown34(void) {
    NotImplemented_nid_no_patch("sZLrjx-yEx4");
    return 0;
}

APS5_EXPORT("usKbuvy2hQg", sceAmprUnknown35);
int APS5_VABI sceAmprUnknown35(void) {
    NotImplemented_nid_no_patch("usKbuvy2hQg");
    return 0;
}
}
