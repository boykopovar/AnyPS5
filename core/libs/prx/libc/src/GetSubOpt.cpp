/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace {

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

bool CopyOutPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

bool WritablePreservingErrno(void* destination, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    errno = saved;
    return ok;
}

std::string CopyGuestCString(const char* source, const char* func) {
    std::string result;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    std::size_t offset = 0;
    for (;;) {
        if (offset > std::numeric_limits<std::uintptr_t>::max() - base) NotImplemented_nid_no_patch(func);
        const auto current = base + offset;
        const std::size_t toPageEnd = 4096 - (current % 4096);
        const std::size_t chunk = toPageEnd < 256 ? toPageEnd : 256;
        char buffer[256];
        if (!CopyInPreservingErrno(buffer, reinterpret_cast<const void*>(current), chunk)) {
            NotImplemented_nid_no_patch(func);
        }
        if (const auto* end = static_cast<const char*>(std::memchr(buffer, '\0', chunk))) {
            result.append(buffer, static_cast<std::size_t>(end - buffer));
            return result;
        }
        result.append(buffer, chunk);
        offset += chunk;
    }
}

}

extern "C" {

char* suboptarg_nid_postfix = nullptr;

int APS5_VABI getsubopt_nid_postfix(char** optionp, char* const* tokens, char** valuep) {
    if (valuep && !WritablePreservingErrno(valuep, sizeof(*valuep))) NotImplemented_nid_no_patch(__func__);
    if (!optionp) {
        suboptarg_nid_postfix = nullptr;
        if (valuep) {
            char* nullValue = nullptr;
            if (!CopyOutPreservingErrno(valuep, &nullValue, sizeof(nullValue))) NotImplemented_nid_no_patch(__func__);
        }
        return -1;
    }

    char* option = nullptr;
    if (!CopyInPreservingErrno(&option, optionp, sizeof(option))) NotImplemented_nid_no_patch(__func__);
    if (!option) {
        suboptarg_nid_postfix = nullptr;
        if (valuep) {
            char* nullValue = nullptr;
            if (!CopyOutPreservingErrno(valuep, &nullValue, sizeof(nullValue))) NotImplemented_nid_no_patch(__func__);
        }
        return -1;
    }

    if (!WritablePreservingErrno(optionp, sizeof(*optionp))) NotImplemented_nid_no_patch(__func__);
    const std::string text = CopyGuestCString(option, __func__);

    std::size_t pos = 0;
    while (pos < text.size() && (text[pos] == ',' || text[pos] == ' ' || text[pos] == '\t')) ++pos;
    if (pos == text.size()) {
        suboptarg_nid_postfix = nullptr;
        if (valuep) {
            char* nullValue = nullptr;
            if (!CopyOutPreservingErrno(valuep, &nullValue, sizeof(nullValue))) NotImplemented_nid_no_patch(__func__);
        }
        char* nextOption = option + pos;
        if (!CopyOutPreservingErrno(optionp, &nextOption, sizeof(nextOption))) NotImplemented_nid_no_patch(__func__);
        return -1;
    }

    const std::size_t suboptionStart = pos;
    ++pos;
    while (pos < text.size() && text[pos] != ',' && text[pos] != '=' && text[pos] != ' ' && text[pos] != '\t') ++pos;
    const std::size_t suboptionEnd = pos;

    bool writeSep1 = false;
    std::size_t sep1Pos = 0;
    bool writeSep2 = false;
    std::size_t sep2Pos = 0;
    bool hasValue = false;
    std::size_t valuePos = 0;
    if (pos < text.size()) {
        if (text[pos] == '=') {
            writeSep1 = true;
            sep1Pos = pos++;
            hasValue = true;
            valuePos = pos;
            while (pos < text.size() && text[pos] != ',' && text[pos] != ' ' && text[pos] != '\t') ++pos;
            if (pos < text.size()) {
                writeSep2 = true;
                sep2Pos = pos++;
            }
        } else {
            writeSep1 = true;
            sep1Pos = pos++;
        }
        while (pos < text.size() && (text[pos] == ',' || text[pos] == ' ' || text[pos] == '\t')) ++pos;
    }

    if (writeSep1 && !WritablePreservingErrno(option + sep1Pos, 1)) NotImplemented_nid_no_patch(__func__);
    if (writeSep2 && !WritablePreservingErrno(option + sep2Pos, 1)) NotImplemented_nid_no_patch(__func__);

    int match = -1;
    if (tokens) {
        const std::string_view suboption(text.data() + suboptionStart, suboptionEnd - suboptionStart);
        for (int index = 0;; ++index) {
            char* tokenPtr = nullptr;
            if (!CopyInPreservingErrno(&tokenPtr, tokens + index, sizeof(tokenPtr))) NotImplemented_nid_no_patch(__func__);
            if (!tokenPtr) break;
            if (CopyGuestCString(tokenPtr, __func__) == suboption) {
                match = index;
                break;
            }
        }
    }

    const char nul = '\0';
    if (writeSep1 && !CopyOutPreservingErrno(option + sep1Pos, &nul, 1)) NotImplemented_nid_no_patch(__func__);
    if (writeSep2 && !CopyOutPreservingErrno(option + sep2Pos, &nul, 1)) NotImplemented_nid_no_patch(__func__);
    if (valuep) {
        char* outValue = hasValue ? option + valuePos : nullptr;
        if (!CopyOutPreservingErrno(valuep, &outValue, sizeof(outValue))) NotImplemented_nid_no_patch(__func__);
    }
    char* nextOption = option + pos;
    if (!CopyOutPreservingErrno(optionp, &nextOption, sizeof(nextOption))) NotImplemented_nid_no_patch(__func__);
    suboptarg_nid_postfix = option + suboptionStart;
    return match;
}

}
