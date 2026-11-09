/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#ifndef _INC_CONIO_S
#define _INC_CONIO_S

#include <conio.h>

#if defined(__LIBMSVCRT__)
/* When building mingw-w64, this should be blank.  */
#define _SECIMP
#else
#ifndef _SECIMP
#define _SECIMP __declspec(dllimport)
#endif /* _SECIMP */
#endif /* defined(__LIBMSVCRT__) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Functions to read/write strings from/to console.
 */

  _SECIMP errno_t __cdecl _cgets_s (char *_Buffer,size_t _Size,size_t *_SizeRead);
  __DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _cgets_s, char, _Buffer, size_t*, _SizeRead)

/**
 * Formatted console I/O functions.
 */

  _SECIMP int __cdecl _cprintf_s (const char *_Format,...);
  _SECIMP int __cdecl _cprintf_s_l (const char *_Format,_locale_t _Locale,...);
  _SECIMP int __cdecl _vcprintf_s (const char *_Format,va_list _ArgList);
  _SECIMP int __cdecl _vcprintf_s_l (const char *_Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _cscanf_s(const char *_Format,...);
  _CRTIMP int __cdecl _cscanf_s_l(const char *_Format,_locale_t _Locale,...);

#ifdef __cplusplus
}
#endif

#endif
