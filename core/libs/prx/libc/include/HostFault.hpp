#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTFAULT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTFAULT_HPP

#ifdef _WIN32
using LibcHostFaultHandler = bool (*)(void* exceptionPointers) noexcept;

extern "C" void LibcSetHostFaultHandler_nid_no_patch(LibcHostFaultHandler handler) noexcept;
#endif

#endif
