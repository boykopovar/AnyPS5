/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#ifndef _INC_CORECRT_WCONIO
#define _INC_CORECRT_WCONIO

#include <corecrt.h>
#include <corecrt_stdio_config.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WEOF
#define WEOF (wint_t)(0xFFFF)
#endif

/**
 * Functions to read/write strings from/to console.
 */

_CRTIMP wchar_t *__cdecl _cgetws(wchar_t *_Buffer) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
_CRTIMP int __cdecl _cputws(const wchar_t *_String);

/**
 * Functions to read/write single characters from/to console.
 */

_CRTIMP wint_t __cdecl _getwch(void);
_CRTIMP wint_t __cdecl _getwche(void);
_CRTIMP wint_t __cdecl _putwch(wchar_t _WCh);
_CRTIMP wint_t __cdecl _ungetwch(wint_t _WCh);

#if __MSVCRT_VERSION__ >= 0x800
_CRTIMP wint_t __cdecl _getwch_nolock(void);
_CRTIMP wint_t __cdecl _getwche_nolock(void);
_CRTIMP wint_t __cdecl _putwch_nolock(wchar_t _WCh);
_CRTIMP wint_t __cdecl _ungetwch_nolock(wint_t _WCh);
#endif

/**
 * Formatted console I/O functions.
 */

#ifdef _UCRT
int __cdecl __conio_common_vcwprintf(unsigned __int64 _Options, const wchar_t *_Format, _locale_t _Locale, va_list _ArgList);
int __cdecl __conio_common_vcwprintf_p(unsigned __int64 _Options, const wchar_t *_Format, _locale_t _Locale, va_list _ArgList);
int __cdecl __conio_common_vcwprintf_s(unsigned __int64 _Options, const wchar_t *_Format, _locale_t _Locale, va_list _ArgList);
int __cdecl __conio_common_vcwscanf(unsigned __int64 _Options, const wchar_t *_Format, _locale_t _Locale, va_list _ArgList);

__mingw_ovr int __cdecl _vcwprintf(const wchar_t * __restrict__ _Format,va_list _ArgList)
{
  return __conio_common_vcwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, _Format, NULL, _ArgList);
}
__mingw_ovr int __cdecl _cwprintf(const wchar_t * __restrict__ _Format,...)
{
  __builtin_va_list _ArgList;
  int _Ret;
  __builtin_va_start(_ArgList, _Format);
  _Ret = _vcwprintf(_Format, _ArgList);
  __builtin_va_end(_ArgList);
  return _Ret;
}
__mingw_ovr __MINGW_ATTRIB_DEPRECATED_SEC_WARN
int __cdecl _cwscanf(const wchar_t * __restrict__ _Format,...)
{
  __builtin_va_list _ArgList;
  int _Ret;
  __builtin_va_start(_ArgList, _Format);
  _Ret = __conio_common_vcwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, _Format, NULL, _ArgList);
  __builtin_va_end(_ArgList);
  return _Ret;
}
__mingw_ovr __MINGW_ATTRIB_DEPRECATED_SEC_WARN
int __cdecl _cwscanf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...)
{
  __builtin_va_list _ArgList;
  int _Ret;
  __builtin_va_start(_ArgList, _Locale);
  _Ret = __conio_common_vcwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, _Format, _Locale, _ArgList);
  __builtin_va_end(_ArgList);
  return _Ret;
}
__mingw_ovr int __cdecl _vcwprintf_p(const wchar_t * __restrict__ _Format,va_list _ArgList)
{
  return __conio_common_vcwprintf_p(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, _Format, NULL, _ArgList);
}
__mingw_ovr int __cdecl _cwprintf_p(const wchar_t * __restrict__ _Format,...)
{
  __builtin_va_list _ArgList;
  int _Ret;
  __builtin_va_start(_ArgList, _Format);
  _Ret = _vcwprintf_p(_Format, _ArgList);
  __builtin_va_end(_ArgList);
  return _Ret;
}
__mingw_ovr int __cdecl _vcwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList)
{
  return __conio_common_vcwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, _Format, _Locale, _ArgList);
}
__mingw_ovr int __cdecl _cwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...)
{
  __builtin_va_list _ArgList;
  int _Ret;
  __builtin_va_start(_ArgList, _Locale);
  _Ret = _vcwprintf_l(_Format, _Locale, _ArgList);
  __builtin_va_end(_ArgList);
  return _Ret;
}
__mingw_ovr int __cdecl _vcwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList)
{
  return __conio_common_vcwprintf_p(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, _Format, _Locale, _ArgList);
}
__mingw_ovr int __cdecl _cwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...)
{
  __builtin_va_list _ArgList;
  int _Ret;
  __builtin_va_start(_ArgList, _Locale);
  _Ret = _vcwprintf_p_l(_Format, _Locale, _ArgList);
  __builtin_va_end(_ArgList);
  return _Ret;
}
#else /* !_UCRT */
_CRTIMP int __cdecl _cwprintf(const wchar_t * __restrict__ _Format,...);
_CRTIMP int __cdecl _cwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
_CRTIMP int __cdecl _cwprintf_p(const wchar_t * __restrict__ _Format,...);
_CRTIMP int __cdecl _cwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
_CRTIMP int __cdecl _vcwprintf(const wchar_t * __restrict__ _Format,va_list _ArgList);
_CRTIMP int __cdecl _vcwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
_CRTIMP int __cdecl _vcwprintf_p(const wchar_t * __restrict__ _Format,va_list _ArgList);
_CRTIMP int __cdecl _vcwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
_CRTIMP int __cdecl _cwscanf(const wchar_t * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
_CRTIMP int __cdecl _cwscanf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
#endif /* _UCRT */

#ifdef __cplusplus
}
#endif

#include <sec_api/wconio_s.h>

#endif /* _INC_CORECRT_WCONIO */
