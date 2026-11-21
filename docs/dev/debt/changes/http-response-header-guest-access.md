# libSceHttp

## change [sceHttpGetCookie]

- [sceHttpGetCookie](../../../core/libs/prx/libSceHttp/src/Cookies.cpp), [URI functions](../../../core/libs/prx/libSceHttp/src/Uri.cpp) and [Aps5HttpClient_Read](../../../core/libs/prx/libSceHttp/src/Client.cpp) still read or write guest buffers in place, so an unmapped buffer crashes the host instead of returning `ERROR_INVALID_VALUE`. `sceHttpGetCookie` needs a stored cookie, which the tests cannot create offline. The Uri and Client functions overlap open pull requests.

## add Functional

- [sceHttpParseResponseHeader guest access](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - analyzes host copies of the field name (at most 4095 bytes) and the header, but returns a value pointer into the original guest header. Returning parser `INVALID_RESPONSE` (`0x80432060`) for unreadable header bytes or parser `INVALID_VALUE` (`0x804321FE`) for an unreadable field name or unwritable output is a host-safety fallback; console behavior on unmapped memory is unmeasured. Copies preserve caller `errno`. Output writes keep their order, so the value pointer can already be written when the length write fails. Header staging uses host storage proportional to `headerLen`; native allocation behavior is not modeled.
