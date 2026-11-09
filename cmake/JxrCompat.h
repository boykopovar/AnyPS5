#pragma once

#include <wchar.h>
#include "strcodec.h"

#if defined(__ANSI__) && !defined(WIN32) && !defined(_BIG__ENDIAN_)
U32 _byteswap_ulong(U32 bits);
#endif
