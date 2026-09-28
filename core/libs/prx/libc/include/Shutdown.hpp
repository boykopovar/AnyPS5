#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SHUTDOWN_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SHUTDOWN_HPP

extern "C" void LibcRegisterShutdown_nid_postfix(void (*callback)());
extern "C" void LibcRunShutdown_nid_postfix();
extern "C" [[noreturn]] void LibcExit_nid_no_patch(int code);

#endif
