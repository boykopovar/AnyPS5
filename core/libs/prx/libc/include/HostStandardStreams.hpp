#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTSTANDARDSTREAMS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTSTANDARDSTREAMS_HPP

#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif

#ifdef _WIN32
#undef stdout
#undef stderr
#ifdef __cplusplus
extern "C" {
#endif
FILE* HostStandardOutputFile_nid_no_patch(void);
FILE* HostStandardErrorFile_nid_no_patch(void);
#ifdef __cplusplus
}
#endif
#define stdout HostStandardOutputFile_nid_no_patch()
#define stderr HostStandardErrorFile_nid_no_patch()
#endif

#endif
