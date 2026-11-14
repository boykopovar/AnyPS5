#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceHttp2SetProxyWithURL();
int APS5_VABI sceHttp2SetRequestServerName();
int APS5_VABI sceHttp2SetStripGZIPHeadersEnabled();

}
