/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_CONIO
#define _INC_CONIO

#include <crtdefs.h>
#include <corecrt_stdio_config.h>
#include <corecrt_wconio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Functions to read/write strings from/to console.
 */

  _CRTIMP char *__cdecl _cgets(char *_Buffer) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _cputs(const char *_Str);

#ifndef	NO_OLDNAMES
  char *__cdecl cgets(char *_Buffer) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl cputs(const char *_Str) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif

/**
 * Functions to read/write single characters from/to console.
 */

  _CRTIMP int __cdecl _kbhit(void);
  _CRTIMP int __cdecl _getch(void);
  _CRTIMP int __cdecl _getche(void);
  _CRTIMP int __cdecl _putch(int _Ch);
  _CRTIMP int __cdecl _ungetch(int _Ch);

#ifndef	NO_OLDNAMES
  int __cdecl kbhit(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl getch(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl getche(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl putch(int _Ch) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl ungetch(int _Ch) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif

#if __MSVCRT_VERSION__ >= 0x800
  _CRTIMP int __cdecl _getch_nolock(void);
  _CRTIMP int __cdecl _getche_nolock(void);
  _CRTIMP int __cdecl _putch_nolock(int _Ch);
  _CRTIMP int __cdecl _ungetch_nolock(int _Ch);
#endif

/**
 * Formatted console I/O functions.
 */

#ifdef _UCRT
  int __cdecl __conio_common_vcprintf(unsigned __int64 _Options, const char *_Format, _locale_t _Locale, va_list _ArgList);
  int __cdecl __conio_common_vcprintf_p(unsigned __int64 _Options, const char *_Format, _locale_t _Locale, va_list _ArgList);
  int __cdecl __conio_common_vcprintf_s(unsigned __int64 _Options, const char *_Format, _locale_t _Locale, va_list _ArgList);
  int __cdecl __conio_common_vcscanf(unsigned __int64 _Options, const char *_Format, _locale_t _Locale, va_list _ArgList);

  __mingw_ovr int __cdecl _vcprintf(const char * __restrict__ _Format,va_list _ArgList)
  {
    return __conio_common_vcprintf(0, _Format, NULL, _ArgList);
  }
  __mingw_ovr int __cdecl _cprintf(const char * __restrict__ _Format,...)
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Format);
    _Ret = _vcprintf(_Format, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
  __mingw_ovr __MINGW_ATTRIB_DEPRECATED_SEC_WARN
  int __cdecl _cscanf(const char * __restrict__ _Format,...)
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Format);
    _Ret = __conio_common_vcscanf(0, _Format, NULL, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
  __mingw_ovr __MINGW_ATTRIB_DEPRECATED_SEC_WARN
  int __cdecl _cscanf_l(const char * __restrict__ _Format,_locale_t _Locale,...)
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Locale);
    _Ret = __conio_common_vcscanf(0, _Format, _Locale, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
  __mingw_ovr int __cdecl _vcprintf_p(const char * __restrict__ _Format,va_list _ArgList)
  {
    return __conio_common_vcprintf_p(0, _Format, NULL, _ArgList);
  }
  __mingw_ovr int __cdecl _cprintf_p(const char * __restrict__ _Format,...)
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Format);
    _Ret = _vcprintf_p(_Format, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
  __mingw_ovr int __cdecl _vcprintf_l(const char * __restrict__ _Format,_locale_t _Locale,va_list _ArgList)
  {
    return __conio_common_vcprintf(0, _Format, _Locale, _ArgList);
  }
  __mingw_ovr int __cdecl _cprintf_l(const char * __restrict__ _Format,_locale_t _Locale,...)
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Locale);
    _Ret = _vcprintf_l(_Format, _Locale, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
  __mingw_ovr int __cdecl _vcprintf_p_l(const char * __restrict__ _Format,_locale_t _Locale,va_list _ArgList)
  {
    return __conio_common_vcprintf_p(0, _Format, _Locale, _ArgList);
  }
  __mingw_ovr int __cdecl _cprintf_p_l(const char * __restrict__ _Format,_locale_t _Locale,...)
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Locale);
    _Ret = _vcprintf_p_l(_Format, _Locale, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
#ifndef	NO_OLDNAMES
  __mingw_ovr int __cdecl cprintf(const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_MSVC2005
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Format);
    _Ret = _vcprintf(_Format, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
  __mingw_ovr int __cdecl cscanf(const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_MSVC2005
  {
    __builtin_va_list _ArgList;
    int _Ret;
    __builtin_va_start(_ArgList, _Format);
    _Ret = __conio_common_vcscanf(0, _Format, NULL, _ArgList);
    __builtin_va_end(_ArgList);
    return _Ret;
  }
#endif /* NO_OLDNAMES */
#else /* !_UCRT */
  _CRTIMP int __cdecl _cprintf(const char * __restrict__ _Format,...);
  _CRTIMP int __cdecl _cprintf_l(const char * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _cprintf_p(const char * __restrict__ _Format,...);
  _CRTIMP int __cdecl _cprintf_p_l(const char * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _vcprintf(const char * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _vcprintf_l(const char * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _vcprintf_p(const char * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _vcprintf_p_l(const char * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _cscanf(const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _cscanf_l(const char * __restrict__ _Format,_locale_t _Locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
#ifndef	NO_OLDNAMES
  int __cdecl cprintf(const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl cscanf(const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif /* NO_OLDNAMES */
#endif /* _UCRT */

/**
 * Port I/O functions.
 *
 * These functions were removed from CRT in msvcr80.dll.
 * They are still available in i386 system versions of msvcrt.dll.
 */

#if (__MSVCRT_VERSION__ >= 0x0400 && __MSVCRT_VERSION__ < 0x0800) && defined(_M_IX86)
  int __cdecl _inp(unsigned short);
  unsigned short __cdecl _inpw(unsigned short);
  unsigned long __cdecl _inpd(unsigned short);
  int __cdecl _outp(unsigned short,int);
  unsigned short __cdecl _outpw(unsigned short,unsigned short);
  unsigned long __cdecl _outpd(unsigned short,unsigned long);
#ifndef	NO_OLDNAMES
  int __cdecl inp(unsigned short) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  unsigned short __cdecl inpw(unsigned short) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl outp(unsigned short,int) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  unsigned short __cdecl outpw(unsigned short,unsigned short) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif /* NO_OLDNAMES */
#endif

#ifdef __cplusplus
}
#endif

#include <sec_api/conio_s.h>

#endif
