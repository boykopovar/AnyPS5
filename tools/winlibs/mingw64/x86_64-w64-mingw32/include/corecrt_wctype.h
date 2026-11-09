/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_CORECRT_WCTYPE
#define _INC_CORECRT_WCTYPE

#include <corecrt.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WEOF
#define WEOF (wint_t)(0xFFFF)
#endif

#ifndef _CTYPE_DISABLE_MACROS
_CRTIMP const unsigned short* __cdecl __pctype_func(void);
_CRTIMP const wctype_t * __cdecl __pwctype_func(void);

#ifndef __PCTYPE_FUNC
#define __PCTYPE_FUNC __pctype_func()
#endif

#ifndef _pctype
#define _pctype (__pctype_func())
#endif

#ifndef _pwctype
#define _pwctype (__pwctype_func())
#endif
#endif /* !_CTYPE_DISABLE_MACROS */

#if __MSVCRT_VERSION__ >= 0x0600
#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
extern _CRTIMP const unsigned short *_wctype;
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */
#endif

#define _UPPER 0x1
#define _LOWER 0x2
#define _DIGIT 0x4
#define _SPACE 0x8

#define _PUNCT 0x10
#define _CONTROL 0x20
#define _BLANK 0x40
#define _HEX 0x80

#define _LEADBYTE 0x8000
#define _ALPHA (0x0100|_UPPER|_LOWER)

/**
 * Standard C functions.
 */

_CRTIMP int __cdecl iswctype(wint_t _C,wctype_t _Type);

_CRTIMP int __cdecl iswalnum(wint_t _C);
_CRTIMP int __cdecl iswalpha(wint_t _C);
_CRTIMP int __cdecl iswblank(wint_t _C);
_CRTIMP int __cdecl iswcntrl(wint_t _C);
_CRTIMP int __cdecl iswdigit(wint_t _C);
_CRTIMP int __cdecl iswgraph(wint_t _C);
_CRTIMP int __cdecl iswlower(wint_t _C);
_CRTIMP int __cdecl iswprint(wint_t _C);
_CRTIMP int __cdecl iswpunct(wint_t _C);
_CRTIMP int __cdecl iswspace(wint_t _C);
_CRTIMP int __cdecl iswupper(wint_t _C);
_CRTIMP int __cdecl iswxdigit(wint_t _C);

_CRTIMP wint_t __cdecl towlower(wint_t _C);
_CRTIMP wint_t __cdecl towupper(wint_t _C);

#if !defined(_CTYPE_DISABLE_MACROS) && !defined(__cplusplus)
#define iswalnum(_c)  (iswctype(_c,_ALPHA|_DIGIT))
#define iswalpha(_c)  (iswctype(_c,_ALPHA))
#define iswblank(_c)  (iswctype(_c,_BLANK))
#define iswcntrl(_c)  (iswctype(_c,_CONTROL))
#define iswdigit(_c)  (iswctype(_c,_DIGIT))
#define iswgraph(_c)  (iswctype(_c,_PUNCT|_ALPHA|_DIGIT))
#define iswlower(_c)  (iswctype(_c,_LOWER))
#define iswprint(_c)  (iswctype(_c,_BLANK|_PUNCT|_ALPHA|_DIGIT))
#define iswpunct(_c)  (iswctype(_c,_PUNCT))
#define iswspace(_c)  (iswctype(_c,_SPACE))
#define iswupper(_c)  (iswctype(_c,_UPPER))
#define iswxdigit(_c) (iswctype(_c,_HEX))
#endif /* !_CTYPE_DISABLE_MACROS && !__cplusplus */

/**
 * Locale-specific versions of Standard C functions.
 *
 * They are available since msvcr80.dll.
 */

/* These are also available in msvcrt.dll since Windows Vista. */
#if __MSVCRT_VERSION__ >= 0x0800 || (__MSVCRT_VERSION__ == 0x0600 && _WIN32_WINNT >= 0x0600)
_CRTIMP int __cdecl _iswctype_l(wint_t _C,wctype_t _Type,_locale_t _Locale);

_CRTIMP int __cdecl _iswalnum_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswalpha_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswblank_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswcntrl_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswdigit_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswgraph_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswlower_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswprint_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswpunct_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswspace_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswupper_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswxdigit_l(wint_t _C,_locale_t _Locale);

_CRTIMP wint_t __cdecl _towlower_l(wint_t _C,_locale_t _Locale);
_CRTIMP wint_t __cdecl _towupper_l(wint_t _C,_locale_t _Locale);

#if !defined(_CTYPE_DISABLE_MACROS) && !defined(__cplusplus)
#define _iswalnum_l(_c,_p)  (_iswctype_l(_c,_ALPHA|_DIGIT,_p))
#define _iswalpha_l(_c,_p)  (_iswctype_l(_c,_ALPHA,_p))
#define _iswblank_l(_c,_p)  (((_c) == '\t') || _iswctype_l(_c,_BLANK,_p))
#define _iswcntrl_l(_c,_p)  (_iswctype_l(_c,_CONTROL,_p))
#define _iswdigit_l(_c,_p)  (_iswctype_l(_c,_DIGIT,_p))
#define _iswgraph_l(_c,_p)  (_iswctype_l(_c,_PUNCT|_ALPHA|_DIGIT,_p))
#define _iswlower_l(_c,_p)  (_iswctype_l(_c,_LOWER,_p))
#define _iswprint_l(_c,_p)  (_iswctype_l(_c,_BLANK|_PUNCT|_ALPHA|_DIGIT,_p))
#define _iswpunct_l(_c,_p)  (_iswctype_l(_c,_PUNCT,_p))
#define _iswspace_l(_c,_p)  (_iswctype_l(_c,_SPACE,_p))
#define _iswupper_l(_c,_p)  (_iswctype_l(_c,_UPPER,_p))
#define _iswxdigit_l(_c,_p) (_iswctype_l(_c,_HEX,_p))
#endif /* !_CTYPE_DISABLE_MACROS && !__cplusplus */
#endif

/**
 * Microsoft-specific functions.
 */

_CRTIMP int __cdecl __iswcsym(wint_t _C);
_CRTIMP int __cdecl __iswcsymf(wint_t _C);
_CRTIMP int __cdecl iswascii(wint_t _C);

/* These are only available for Desktop applications. */
#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP int __cdecl is_wctype(wint_t _C,wctype_t _Type);
_CRTIMP int __cdecl isleadbyte(int _C);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#if !defined(_CTYPE_DISABLE_MACROS) && !defined(__cplusplus)
#define __iswcsym(_c)  (iswalnum(_c) || ((_c)=='_'))
#define __iswcsymf(_c) (iswalpha(_c) || ((_c)=='_'))
#define iswascii(_c)   ((unsigned)(_c) < 0x80)
#endif /* !_CTYPE_DISABLE_MACROS && !__cplusplus */

/**
 * Locale-specific versions of Microsoft functions.
 *
 * They are available since msvcr80.dll.
 */

/* These are only available since msvcr80.dll, never in msvcrt.dll. */
#if __MSVCRT_VERSION__ >= 0x0800
_CRTIMP int __cdecl _iswcsym_l(wint_t _C,_locale_t _Locale);
_CRTIMP int __cdecl _iswcsymf_l(wint_t _C,_locale_t _Locale);

#if !defined(_CTYPE_DISABLE_MACROS) && !defined(__cplusplus)
#define _iswcsym_l(_c,_p)  (_iswalnum_l(_c,_p) || ((_c)=='_'))
#define _iswcsymf_l(_c,_p) (_iswalpha_l(_c,_p) || ((_c)=='_'))
#endif /* !_CTYPE_DISABLE_MACROS && !__cplusplus */
#endif

/* These are also available in msvcrt.dll since Windows Vista. */
#if __MSVCRT_VERSION__ >= 0x0800 || (__MSVCRT_VERSION__ == 0x0600 && _WIN32_WINNT >= 0x0600)
/* These are only available for Desktop applications. */
#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP int __cdecl _isleadbyte_l(int _C,_locale_t _Locale);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */
#endif

#ifdef __cplusplus
}
#endif

#endif
