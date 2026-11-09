#ifndef GCC_TM_H
#define GCC_TM_H
#ifndef LIBC_GLIBC
# define LIBC_GLIBC 1
#endif
#ifndef LIBC_UCLIBC
# define LIBC_UCLIBC 2
#endif
#ifndef LIBC_BIONIC
# define LIBC_BIONIC 3
#endif
#ifndef LIBC_MUSL
# define LIBC_MUSL 4
#endif
#ifndef HEAP_TRAMPOLINES_INIT
# define HEAP_TRAMPOLINES_INIT 0
#endif
#ifdef IN_GCC
# include "options.h"
# include "insn-constants.h"
# include "config/vxworks-dummy.h"
# include "config/i386/biarch64.h"
# include "config/i386/i386.h"
# include "config/i386/unix.h"
# include "config/i386/bsd.h"
# include "config/i386/gas.h"
# include "config/i386/cygming.h"
# include "config/i386/mingw-pthread.h"
# include "config/mingw/mingw32.h"
# include "config/i386/mingw-w64.h"
# include "config/mingw/mingw-stdint.h"
# include "config/mingw/winnt.h"
# include "config/mingw/winnt-dll.h"
# include "config/initfini-array.h"
#endif
#if defined IN_GCC && !defined GENERATOR_FILE && !defined USED_FOR_TARGET
# include "insn-flags.h"
#endif
#if defined IN_GCC && !defined GENERATOR_FILE
# include "insn-modes.h"
#endif
# include "defaults.h"
#endif /* GCC_TM_H */
