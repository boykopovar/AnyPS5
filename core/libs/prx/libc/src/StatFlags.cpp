/* Derived from FreeBSD lib/libc/gen/strtofflags.c (8c639945a445) */
/*-
 * Copyright (c) 1993
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
 * 4. Neither the name of the University nor the names of its contributors
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
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::uint64_t UfNodump = 0x00000001;
constexpr std::uint64_t UfImmutable = 0x00000002;
constexpr std::uint64_t UfAppend = 0x00000004;
constexpr std::uint64_t UfOpaque = 0x00000008;
constexpr std::uint64_t UfNounlink = 0x00000010;
constexpr std::uint64_t UfSystem = 0x00000080;
constexpr std::uint64_t UfSparse = 0x00000100;
constexpr std::uint64_t UfOffline = 0x00000200;
constexpr std::uint64_t UfReparse = 0x00000400;
constexpr std::uint64_t UfArchive = 0x00000800;
constexpr std::uint64_t UfReadonly = 0x00001000;
constexpr std::uint64_t UfHidden = 0x00008000;
constexpr std::uint64_t SfArchived = 0x00010000;
constexpr std::uint64_t SfImmutable = 0x00020000;
constexpr std::uint64_t SfAppend = 0x00040000;
constexpr std::uint64_t SfNounlink = 0x00100000;
constexpr std::uint64_t SfSnapshot = 0x00200000;

struct FlagMapping {
    const char* name;
    bool invert;
    std::uint64_t flag;
};

constexpr FlagMapping Mappings[] = {
    {"nosappnd", false, SfAppend},
    {"nosappend", false, SfAppend},
    {"noarch", false, SfArchived},
    {"noarchived", false, SfArchived},
    {"noschg", false, SfImmutable},
    {"noschange", false, SfImmutable},
    {"nosimmutable", false, SfImmutable},
    {"nosunlnk", false, SfNounlink},
    {"nosunlink", false, SfNounlink},
    {"nosnapshot", false, SfSnapshot},
    {"nouappnd", false, UfAppend},
    {"nouappend", false, UfAppend},
    {"nouarch", false, UfArchive},
    {"nouarchive", false, UfArchive},
    {"nohidden", false, UfHidden},
    {"nouhidden", false, UfHidden},
    {"nouchg", false, UfImmutable},
    {"nouchange", false, UfImmutable},
    {"nouimmutable", false, UfImmutable},
    {"nodump", true, UfNodump},
    {"nouunlnk", false, UfNounlink},
    {"nouunlink", false, UfNounlink},
    {"nooffline", false, UfOffline},
    {"nouoffline", false, UfOffline},
    {"noopaque", false, UfOpaque},
    {"nordonly", false, UfReadonly},
    {"nourdonly", false, UfReadonly},
    {"noreadonly", false, UfReadonly},
    {"noureadonly", false, UfReadonly},
    {"noreparse", false, UfReparse},
    {"noureparse", false, UfReparse},
    {"nosparse", false, UfSparse},
    {"nousparse", false, UfSparse},
    {"nosystem", false, UfSystem},
    {"nousystem", false, UfSystem},
};

void Store(std::uint64_t* destination, std::uint64_t value) {
    if (destination != nullptr && !GuestCopyOut_nid_no_patch(destination, &value, sizeof(value))) {
        throw std::invalid_argument("strtofflags: unwritable output");
    }
}

const FlagMapping* Find(const char* token) {
    for (const FlagMapping& mapping : Mappings) {
        if (std::string_view(token) == std::string_view(mapping.name).substr(2)) return &mapping;
        if (std::string_view(token) == mapping.name) return &mapping;
    }
    return nullptr;
}

}

extern "C" int APS5_VABI strtofflags_nid_postfix(char** stringp, std::uint64_t* setp, std::uint64_t* clrp) {
    if (stringp == nullptr) throw std::invalid_argument("strtofflags: null string pointer");
    std::uint64_t set = 0;
    std::uint64_t clear = 0;
    Store(setp, 0);
    Store(clrp, 0);
    char* string = nullptr;
    if (!GuestCopyIn_nid_no_patch(&string, stringp, sizeof(string))) throw std::invalid_argument("strtofflags: unreadable string");
    const auto flush = [&] {
        Store(setp, set);
        Store(clrp, clear);
    };
    for (char* token = strsep_nid_postfix(&string, "\t ,"); token != nullptr; token = strsep_nid_postfix(&string, "\t ,")) {
        if (!GuestCopyOut_nid_no_patch(stringp, &token, sizeof(token))) throw std::invalid_argument("strtofflags: unwritable string");
        if (*token == '\0') continue;
        const FlagMapping* mapping = Find(token);
        if (mapping == nullptr) {
            flush();
            return 1;
        }
        const bool negatedSpelling = std::string_view(token) == std::string_view(mapping->name);
        if (mapping->invert != negatedSpelling) {
            clear |= mapping->flag;
        } else {
            set |= mapping->flag;
        }
    }
    flush();
    return 0;
}
