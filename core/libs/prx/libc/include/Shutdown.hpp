#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SHUTDOWN_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SHUTDOWN_HPP

#include <stop_token>

class ProcessShutdown final {};

extern "C" void LibcRegisterShutdown_nid_postfix(void (*callback)());
extern "C" void LibcRunShutdown_nid_postfix();
extern "C" std::stop_token LibcShutdownToken_nid_postfix();
extern "C" void LibcRequestShutdown_nid_postfix();
extern "C" void LibcRequestExit_nid_postfix(int code);
extern "C" [[noreturn]] void LibcAwaitExit_nid_postfix();
extern "C" [[noreturn]] void LibcExit_nid_no_patch(int code);
extern "C" [[noreturn]] void LibcTerminate_nid_no_patch(int code);

#endif
