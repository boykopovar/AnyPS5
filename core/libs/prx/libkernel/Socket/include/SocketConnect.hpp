#pragma once
#ifdef _WIN32
#include <winsock2.h>

extern "C" void RefuseClosedLoopbackAtOnce_nid_no_patch(SOCKET socket, const sockaddr* address);
#endif
