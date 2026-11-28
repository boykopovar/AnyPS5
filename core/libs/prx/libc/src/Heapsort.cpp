// SPDX-FileCopyrightText: Copyright (c) 1991, 1993 The Regents of the University of California. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2014 David T. Chisnall
// SPDX-License-Identifier: BSD-3-Clause
// Adapted from FreeBSD lib/libc/stdlib/heapsort.c (identical in release/11.4.0): https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/heapsort.c
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2014 David T. Chisnall
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ronnie Kon at Mindcraft Inc., Kevin Lew and Elmer Yglesias.
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
#include "prx/libc/include/GuestErrno.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

void ProbeGuestRange(void* base, std::size_t totalBytes) {
    constexpr std::uintptr_t pageSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(base);
    const auto last = start + (totalBytes - 1);
    auto probeAddress = [](std::uintptr_t address) {
        unsigned char byte = 0;
        auto* const ptr = reinterpret_cast<void*>(address);
        if (!GuestCopyIn_nid_no_patch(&byte, ptr, 1) || !GuestCopyOut_nid_no_patch(ptr, &byte, 1)) {
            throw std::invalid_argument("heapsort: inaccessible guest buffer");
        }
    };
    probeAddress(start);
    const std::uintptr_t firstPage = start / pageSize;
    const std::uintptr_t lastPage = last / pageSize;
    for (std::uintptr_t page = firstPage + 1; page <= lastPage; ++page) {
        probeAddress(page * pageSize);
    }
}

class HeapSorter {
public:
    HeapSorter(unsigned char* first, std::size_t size, GuestCompare compare) : first(first), size(size), compare(compare) {}

    void Sort(std::size_t count, unsigned char* displaced) const {
        for (std::size_t start = count / 2; start != 0; --start) Create(start, count);
        while (count > 1) {
            Copy(displaced, Element(count));
            Copy(Element(count), Element(1));
            --count;
            Select(count, displaced);
        }
    }

private:
    unsigned char* Element(std::size_t number) const {
        return first + (number - 1) * size;
    }

    void Copy(unsigned char* to, const unsigned char* from) const {
        std::memcpy(to, from, size);
    }

    std::size_t LargerChild(std::size_t child, std::size_t count) const {
        return child < count && compare(Element(child), Element(child + 1)) < 0 ? child + 1 : child;
    }

    void Create(std::size_t start, std::size_t count) const {
        std::size_t parent = start;
        while (parent <= count / 2) {
            const std::size_t child = LargerChild(parent * 2, count);
            if (compare(Element(child), Element(parent)) <= 0) break;
            std::swap_ranges(Element(parent), Element(parent) + size, Element(child));
            parent = child;
        }
    }

    void Select(std::size_t count, const unsigned char* displaced) const {
        std::size_t parent = 1;
        while (parent <= count / 2) {
            const std::size_t child = LargerChild(parent * 2, count);
            Copy(Element(parent), Element(child));
            parent = child;
        }
        for (;;) {
            const std::size_t child = parent;
            parent = child / 2;
            if (child == 1 || compare(displaced, Element(parent)) < 0) {
                Copy(Element(child), displaced);
                break;
            }
            Copy(Element(child), Element(parent));
        }
    }

    unsigned char* first;
    std::size_t size;
    GuestCompare compare;
};

}

int APS5_VABI heapsort_nid_postfix(void* base, size_t count, size_t size, GuestCompare compare) {
    if (count <= 1) return 0;
    if (size == 0) {
        *__error_nid_postfix() = 22;
        return -1;
    }
    if (!compare) throw std::invalid_argument("heapsort: null comparator");
    if (!base) throw std::invalid_argument("heapsort: null base");
    if (count > std::numeric_limits<size_t>::max() / size) throw std::overflow_error("heapsort: array size overflow");
    const std::size_t totalBytes = count * size;
    if (reinterpret_cast<std::uintptr_t>(base) > UINTPTR_MAX - totalBytes) {
        throw std::overflow_error("heapsort: array address overflow");
    }
    ProbeGuestRange(base, totalBytes);
    auto* const displaced = static_cast<unsigned char*>(malloc_nid_postfix(size));
    if (!displaced) return -1;
    const std::unique_ptr<unsigned char, decltype(&free_nid_postfix)> owner(displaced, free_nid_postfix);
    HeapSorter(static_cast<unsigned char*>(base), size, compare).Sort(count, displaced);
    return 0;
}
