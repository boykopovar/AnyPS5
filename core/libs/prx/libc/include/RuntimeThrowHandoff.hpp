#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_RUNTIMETHROWHANDOFF_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_RUNTIMETHROWHANDOFF_HPP

#ifdef _WIN32
#include <windows.h>

enum class RuntimeThrowHandoff { None, Redirected, InProgress };

RuntimeThrowHandoff LibcRuntimeThrowHandoff_nid_no_patch(EXCEPTION_POINTERS* info);
#endif

#endif
