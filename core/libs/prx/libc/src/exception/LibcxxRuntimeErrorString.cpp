#include "prx/libc/include/Api.hpp"
#include "../LibcxxStringArgument.hpp"

extern "C" void APS5_VABI _ZNSt13runtime_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
    LibcException::ExceptionObject* self, const void* message) {
    _ZNSt13runtime_errorC1EPKc_nid_postfix(self, CopyLibcxxStringArgument(message).c_str());
}

extern "C" void APS5_VABI _ZNSt13runtime_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
    LibcException::ExceptionObject* self, const void* message) {
    _ZNSt13runtime_errorC2EPKc_nid_postfix(self, CopyLibcxxStringArgument(message).c_str());
}
