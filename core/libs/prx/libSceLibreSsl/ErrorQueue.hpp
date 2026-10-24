#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_ERRORQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_ERRORQUEUE_HPP

#include <cstddef>
#include <cstdint>
#include "include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
std::uint64_t LibreSslPeekLastError_nid_no_patch();
void LibreSslLoadErrorStrings_nid_no_patch();

void APS5_VABI ERR_clear_error_nid_postfix();
char* APS5_VABI ERR_error_string_nid_postfix(std::uint64_t error, char* buffer);
void APS5_VABI ERR_error_string_n_nid_postfix(std::uint64_t error, char* buffer, std::size_t length);
void APS5_VABI ERR_free_strings_nid_postfix();
std::uint64_t APS5_VABI ERR_get_error_nid_postfix();
std::uint64_t APS5_VABI ERR_peek_error_nid_postfix();
void APS5_VABI ERR_remove_thread_state_nid_postfix(const void* id);
}

#endif
