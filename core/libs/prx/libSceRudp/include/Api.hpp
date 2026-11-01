#pragma once

#include "SceTypes.hpp"

using GuestRudpEventHandler = RudpEventHandler;

extern "C" {

int APS5_VABI sceRudpInit_nid_postfix(void* memPool, int memPoolSize);
int APS5_VABI sceRudpActivate(std::int32_t contextId, const void* address, std::uint32_t addressLength);
int APS5_VABI sceRudpGetStatus(void* status, std::size_t size);
int APS5_VABI sceRudpEnd(void);
int APS5_VABI sceRudpTerminate(int ctxId);
int APS5_VABI sceRudpEnableInternalIOThread(uint32_t stackSize, uint32_t priority);
int APS5_VABI sceRudpEnableInternalIOThread2(uint32_t stackSize, uint32_t priority, uint64_t affinity);
int APS5_VABI sceRudpSetEventHandler(GuestRudpEventHandler handler, void* arg);
int APS5_VABI sceRudpCreateContext(GuestRudpEventHandler handler, void* arg, int* ctxId);
int APS5_VABI sceRudpBind(int ctxId, int socketId, uint16_t vport, uint8_t muxMode);
int APS5_VABI sceRudpSetOption(int ctxId, int option, const void* value, uint32_t len);
int APS5_VABI sceRudpGetOption(int ctxId, int option, void* value, uint32_t len);
int APS5_VABI sceRudpInitiate(int ctxId, const void* to, uint32_t tolen, uint16_t vport);
int APS5_VABI sceRudpGetContextStatus(int ctxId, void* status, uint32_t size);
int APS5_VABI sceRudpGetRemoteInfo(int contextId, void* address, std::uint32_t* addressSize, std::uint16_t* virtualPort);
int APS5_VABI sceRudpGetNumberOfPacketsToRead(int ctxId);
int APS5_VABI sceRudpGetSizeWritable(int ctxId);
int APS5_VABI sceRudpGetSizeReadable(int ctxId);
int APS5_VABI sceRudpWrite(int ctxId, const void* data, uint32_t len, int flags);
int APS5_VABI sceRudpRead(int ctxId, void* data, uint32_t len, int flags, void* status);
int APS5_VABI sceRudpNetFlush(void);
int APS5_VABI sceRudpProcessEvents(std::uint64_t timeout);
int APS5_VABI sceRudpNetReceived(int socketId, const void* data, std::size_t dataSize, const void* sourceAddress, std::uint32_t sourceAddressSize);
int APS5_VABI sceRudpGetMaxSegmentSize(uint16_t* size);
int APS5_VABI sceRudpPollCreate(std::size_t size);
int APS5_VABI sceRudpPollControl(int pollId, int op, int ctxId, uint16_t events);
int APS5_VABI sceRudpPollWait(int pollId, RudpPollEvent* events, std::size_t eventLen, uint64_t timeout);
int APS5_VABI sceRudpPollDestroy(int pollId);
int APS5_VABI sceRudpAccept(int ctxId, GuestRudpEventHandler handler, void* handlerArg, int* newCtxId, void* address, std::uint32_t* addressLength, std::uint16_t* vport);
int APS5_VABI sceRudpFlush(int ctxId);
int APS5_VABI sceRudpGetLocalInfo();
int APS5_VABI sceRudpListen(int ctxId, int backlog);
int APS5_VABI sceRudpPollCancel(int pollId);
int APS5_VABI sceRudpSetMaxSegmentSize(uint16_t mss);
int APS5_VABI sceRudpGetContextBufferStatus(int ctxId, void* status, std::size_t size);

}
