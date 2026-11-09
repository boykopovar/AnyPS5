/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#ifndef _INC_SWPRINTF_INL
#define _INC_SWPRINTF_INL

#ifdef __cplusplus

extern "C++" {

/* __attribute__((__format__ (gnu_wprintf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
int vswprintf (wchar_t *__stream, const wchar_t *__format, __builtin_va_list __local_argv) __MINGW_ASM_CALL(_vswprintf);

/* __attribute__((__format__ (gnu_wprintf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
int swprintf (wchar_t *__stream, const wchar_t *__format, ...) __MINGW_ASM_CALL(_swprintf);

}

#elif defined(_CRT_NON_CONFORMING_SWPRINTFS)

#define swprintf _swprintf
#define vswprintf _vswprintf

#endif /* __cplusplus */

#endif /* _INC_SWPRINTF_INL */
