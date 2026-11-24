// Derived from src/psl.c (libpsl ba4c49a205408845834b1648ad23c15ee09f722b): https://github.com/rockdaboot/libpsl/blob/ba4c49a205408845834b1648ad23c15ee09f722b/src/psl.c
/*
 * Copyright(c) 2014-2024 Tim Ruehsen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "prx/libSceNKWebKitRequirements/include/GuestPsl.hpp"
#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <libpsl.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

constexpr int GuestInet = 2;
constexpr int GuestInet6 = 28;

void SyncFileStream(FileStream* stream) {
    stream->SyncStatus();
}

bool IsIpAddress(const char* hostname) {
    std::array<unsigned char, 16> address{};
    return __inet_pton_nid_postfix(GuestInet, hostname, address.data()) == 1
        || __inet_pton_nid_postfix(GuestInet6, hostname, address.data()) == 1;
}

} // namespace

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

int APS5_VABI psl_is_cookie_domain_acceptable_nid_postfix(const NKWebKit::GuestPslCtx* guest, const char* hostname, const char* cookieDomain) {
    const auto* psl = reinterpret_cast<const psl_ctx_t*>(guest);
    if (!psl || !hostname || !cookieDomain) return 0;
    while (*cookieDomain == '.') ++cookieDomain;
    if (!std::strcmp(hostname, cookieDomain)) return 1;
    if (IsIpAddress(hostname)) return 0;
    const std::size_t cookieLength = std::strlen(cookieDomain);
    const std::size_t hostLength = std::strlen(hostname);
    if (cookieLength >= hostLength) return 0;
    const char* tail = hostname + hostLength - cookieLength;
    if (std::strcmp(tail, cookieDomain) || tail[-1] != '.') return 0;
    const char* unregistrable = psl_unregistrable_domain(psl, hostname);
    return !unregistrable || cookieLength > std::strlen(unregistrable) ? 1 : 0;
}

int APS5_VABI psl_is_public_suffix_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* domain) {
    return psl_is_public_suffix(reinterpret_cast<const psl_ctx_t*>(psl), domain);
}

int APS5_VABI psl_is_public_suffix2_nid_postfix(const NKWebKit::GuestPslCtx* psl, const char* domain, int type) {
    return psl_is_public_suffix2(reinterpret_cast<const psl_ctx_t*>(psl), domain, type);
}

NKWebKit::GuestPslCtx* APS5_VABI psl_latest_nid_postfix(const char* fname) {
    struct Source {
        const char* filename;
        std::int64_t modified;
    };
    const std::int64_t builtinTime = psl_builtin_file_time();
    std::array<Source, 3> sources{{{fname, builtinTime}, {psl_dist_filename(), builtinTime}, {psl_builtin_filename(), builtinTime}}};
    for (auto& source : sources) {
        if (!source.filename || !*source.filename) continue;
        FileStat status{};
        if (stat_nid_postfix(source.filename, &status) == 0) source.modified = status.st_mtim.tv_sec;
    }
    for (std::size_t attempt = 0; attempt < sources.size(); ++attempt) {
        auto source = std::max_element(sources.begin(), sources.end(), [](const Source& left, const Source& right) {
            return left.modified < right.modified;
        });
        if (source->modified <= builtinTime) break;
        auto* loaded = psl_load_file_nid_postfix(source->filename);
        source->modified = builtinTime;
        if (loaded) return loaded;
    }
    return const_cast<NKWebKit::GuestPslCtx*>(reinterpret_cast<const NKWebKit::GuestPslCtx*>(psl_builtin()));
}

NKWebKit::GuestPslCtx* APS5_VABI psl_load_file_nid_postfix(const char* fname) {
    if (!fname) return nullptr;
    char firstChar = '\0';
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&firstChar, fname, sizeof(firstChar));
    errno = savedErrno;
    if (!readable || firstChar == '\0') return nullptr;
    std::unique_ptr<FileStream, decltype(&fclose_nid_postfix)> stream(fopen_nid_postfix(fname, "rb"), fclose_nid_postfix);
    return stream ? psl_load_fp_nid_postfix(stream.get()) : nullptr;
}

NKWebKit::GuestPslCtx* APS5_VABI psl_load_fp_nid_postfix(FileStream* fp) {
    if (!fp) return nullptr;
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(fp, sizeof(*fp));
    errno = savedErrno;
    if (!writable) return nullptr;
    const psl_ctx_t* psl = psl_load_fp(GetNativeStream(fp));
    SyncFileStream(fp);
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
