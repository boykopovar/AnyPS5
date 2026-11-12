#include "prx/libSceNKWebKitRequirements/include/GuestPsl.hpp"
#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"

#include <libpsl.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

extern "C" {

const NKWebKit::GuestPslCtx* APS5_VABI psl_builtin_nid_postfix() {
    return reinterpret_cast<const NKWebKit::GuestPslCtx*>(psl_builtin());
}

int64_t APS5_VABI psl_builtin_file_time_nid_postfix() {
    return static_cast<int64_t>(psl_builtin_file_time());
}

const char* APS5_VABI psl_builtin_filename_nid_postfix() {
    return psl_builtin_filename();
}

int APS5_VABI psl_builtin_outdated_nid_postfix() {
    return psl_builtin_outdated();
}

const char* APS5_VABI psl_builtin_sha1sum_nid_postfix() {
    return psl_builtin_sha1sum();
}

int APS5_VABI psl_check_version_number_nid_postfix(int version) {
    return psl_check_version_number(version);
}

const char* APS5_VABI psl_dist_filename_nid_postfix() {
    return psl_dist_filename();
}

void APS5_VABI psl_free_nid_postfix(NKWebKit::GuestPslCtx* psl) {
    if (psl) {
        psl_free(const_cast<psl_ctx_t*>(reinterpret_cast<const psl_ctx_t*>(psl)));
    }
}

void APS5_VABI psl_free_string_nid_postfix(char* str) {
    if (str) {
        psl_free_string(str);
    }
}

const char* APS5_VABI psl_get_version_nid_postfix() {
    return psl_get_version();
}

int APS5_VABI psl_is_cookie_domain_acceptable_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* hostname, const char* cookie_domain) {
    return psl_is_cookie_domain_acceptable(reinterpret_cast<const psl_ctx_t*>(psl), hostname, cookie_domain);
}

int APS5_VABI psl_is_public_suffix_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* domain) {
    return psl_is_public_suffix(reinterpret_cast<const psl_ctx_t*>(psl), domain);
}

int APS5_VABI psl_is_public_suffix2_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* domain, int type) {
    return psl_is_public_suffix2(reinterpret_cast<const psl_ctx_t*>(psl), domain, type);
}

NKWebKit::GuestPslCtx* APS5_VABI psl_latest_nid_postfix(const char* fname) {
    return const_cast<NKWebKit::GuestPslCtx*>(reinterpret_cast<const NKWebKit::GuestPslCtx*>(psl_latest(fname)));
}

NKWebKit::GuestPslCtx* APS5_VABI psl_load_file_nid_postfix(const char* fname) {
    return const_cast<NKWebKit::GuestPslCtx*>(reinterpret_cast<const NKWebKit::GuestPslCtx*>(psl_load_file(fname)));
}

NKWebKit::GuestPslCtx* APS5_VABI psl_load_fp_nid_postfix(FileStream* fp) {
    if (!fp) return nullptr;
    const psl_ctx_t* psl = psl_load_fp(GetNativeStream(fp));
    fp->SyncStatus();
    return const_cast<NKWebKit::GuestPslCtx*>(reinterpret_cast<const NKWebKit::GuestPslCtx*>(psl));
}

const char* APS5_VABI psl_registrable_domain_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* domain) {
    return psl_registrable_domain(reinterpret_cast<const psl_ctx_t*>(psl), domain);
}

int APS5_VABI psl_str_to_utf8lower_nid_postfix(const char* str, const char* encoding, const char* locale, char** lower) {
    return psl_str_to_utf8lower(str, encoding, locale, lower);
}

int APS5_VABI psl_suffix_count_nid_postfix(const NKWebKit::GuestPslCtx* psl) {
    return psl_suffix_count(reinterpret_cast<const psl_ctx_t*>(psl));
}

int APS5_VABI psl_suffix_exception_count_nid_postfix(const NKWebKit::GuestPslCtx* psl) {
    return psl_suffix_exception_count(reinterpret_cast<const psl_ctx_t*>(psl));
}

int APS5_VABI psl_suffix_wildcard_count_nid_postfix(const NKWebKit::GuestPslCtx* psl) {
    return psl_suffix_wildcard_count(reinterpret_cast<const psl_ctx_t*>(psl));
}

const char* APS5_VABI psl_unregistrable_domain_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* domain) {
    return psl_unregistrable_domain(reinterpret_cast<const psl_ctx_t*>(psl), domain);
}

}
