#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceHttp2GetAllTrailingHeaders();
int APS5_VABI sceHttp2SetProxyWithURL();
int APS5_VABI sceHttp2SetRequestServerName();
int APS5_VABI sceHttp2SetStripGZIPHeadersEnabled();
int APS5_VABI sceHttp2WebSocketClose();
int APS5_VABI sceHttp2WebSocketSendDataMessage();
int APS5_VABI sceHttp2WebSocketSendTextMessage();
int APS5_VABI sceHttp2WebSocketSetCloseTimeout();

}
