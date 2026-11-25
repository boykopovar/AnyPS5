/* Derived from FreeBSD lib/libc/gen/getbsize.c (8c639945a445) */
/*-
 * Copyright (c) 1991, 1993
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

#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int64_t KiloByte = 1024;
constexpr std::int64_t MegaByte = 1024 * KiloByte;
constexpr std::int64_t GigaByte = 1024 * MegaByte;
constexpr std::int64_t MaximumBlock = GigaByte;
constexpr std::int64_t MinimumBlock = 512;

struct BlockSize {
    std::int64_t count;
    std::int64_t blocksize;
    const char* form;
};

BlockSize Underflow() {
    warnx_nid_postfix("minimum blocksize is 512");
    return BlockSize{MinimumBlock, MinimumBlock, ""};
}

BlockSize Parse(const char* p) {
    char* end = nullptr;
    std::int64_t n = std::strtoll(p, &end, 10);
    if (n < 0) return Underflow();
    if (n == 0) n = 1;
    std::int64_t max = MaximumBlock;
    std::int64_t mul = 1;
    const char* form = "";
    if (*end != '\0' && end[1] != '\0') {
        warnx_nid_postfix("%s: unknown blocksize", p);
        n = MinimumBlock;
    } else {
        switch (*end) {
            case 'G': case 'g':
                form = "G";
                max = MaximumBlock / GigaByte;
                mul = GigaByte;
                break;
            case 'K': case 'k':
                form = "K";
                max = MaximumBlock / KiloByte;
                mul = KiloByte;
                break;
            case 'M': case 'm':
                form = "M";
                max = MaximumBlock / MegaByte;
                mul = MegaByte;
                break;
            case '\0':
                break;
            default:
                warnx_nid_postfix("%s: unknown blocksize", p);
                n = MinimumBlock;
                break;
        }
    }
    if (n > max) {
        warnx_nid_postfix("maximum blocksize is %lldG", static_cast<long long>(MaximumBlock / GigaByte));
        n = max;
    }
    const std::int64_t blocksize = n * mul;
    if (blocksize < MinimumBlock) return Underflow();
    return BlockSize{n, blocksize, form};
}

}

char* APS5_VABI getbsize_nid_postfix(int* headerlenp, std::int64_t* blocksizep) {
    if (headerlenp == nullptr || blocksizep == nullptr) throw std::invalid_argument("getbsize: null output");
    static char header[20];
    const char* p = getenv_nid_postfix("BLOCKSIZE");
    const BlockSize size = (p != nullptr && *p != '\0') ? Parse(p) : BlockSize{MinimumBlock, MinimumBlock, ""};
    std::snprintf(header, sizeof(header), "%lld%s-blocks", static_cast<long long>(size.count), size.form);
    const int headerLength = static_cast<int>(std::strlen(header));
    if (!GuestCopyOut_nid_no_patch(headerlenp, &headerLength, sizeof(headerLength)) ||
        !GuestCopyOut_nid_no_patch(blocksizep, &size.blocksize, sizeof(size.blocksize))) {
        throw std::invalid_argument("getbsize: unwritable output");
    }
    return header;
}
