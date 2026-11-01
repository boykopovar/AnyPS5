// SPDX-FileCopyrightText: Copyright (c) 1992, 1993 The Regents of the University of California. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// Adapted from FreeBSD release/11.4.0 lib/libc/stdlib/merge.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdlib/merge.c
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t LinkSize = sizeof(unsigned char*);
constexpr std::size_t NaturalRunThreshold = 16;
constexpr std::size_t GallopAfter = 6;

unsigned char* LinkSlot(unsigned char* position) {
    const auto address = reinterpret_cast<std::uintptr_t>(position);
    return reinterpret_cast<unsigned char*>((address + LinkSize - 1) & ~static_cast<std::uintptr_t>(LinkSize - 1));
}

unsigned char* LoadLink(unsigned char* position) {
    unsigned char* link;
    std::memcpy(&link, LinkSlot(position), sizeof(link));
    return link;
}

void StoreLink(unsigned char* position, unsigned char* link) {
    std::memcpy(LinkSlot(position), &link, sizeof(link));
}

class MergeSorter {
public:
    MergeSorter(std::size_t size, GuestCompare compare) : size(size), compare(compare) {}

    unsigned char* Sort(unsigned char* base, std::size_t count, unsigned char* scratch) {
        unsigned char* list1 = base;
        unsigned char* list2 = scratch;
        Setup(list1, list2, count);
        unsigned char* last = list2 + count * size;
        while (LoadLink(list2) != last) {
            MergePass(list1, list2, last);
            std::swap(list1, list2);
            last = list2 + count * size;
        }
        if (list2 == base) {
            std::memmove(list2, list1, count * size);
            list2 = list1;
        }
        return list2;
    }

private:
    int Compare(const unsigned char* left, const unsigned char* right) const {
        return compare(left, right);
    }

    void SwapElements(unsigned char* left, unsigned char* right) const {
        std::swap_ranges(left, left + size, right);
    }

    void Reverse(unsigned char* bottom, unsigned char* top) const {
        do {
            SwapElements(bottom, top);
            bottom += size;
            top -= size;
        } while (bottom < top);
    }

    void InsertionSort(unsigned char* first, std::size_t count) const {
        for (unsigned char* inserted = first + size; --count >= 1; inserted += size) {
            for (unsigned char* current = inserted; current > first; current -= size) {
                unsigned char* previous = current - size;
                if (Compare(previous, current) <= 0) break;
                SwapElements(previous, current);
            }
        }
    }

    static void Link(unsigned char*& tail, unsigned char* next) {
        StoreLink(tail, next);
        tail = next;
    }

    void Setup(unsigned char* list1, unsigned char* list2, std::size_t count) const {
        const std::size_t pair = size * 2;
        if (count <= 5) {
            InsertionSort(list1, count);
            StoreLink(list2, list2 + count * size);
            return;
        }
        const std::size_t tailCount = 4 + (count & 1);
        InsertionSort(list1 + (count - tailCount) * size, tailCount);
        unsigned char* const last = list1 + size * (count - tailCount);
        StoreLink(list2 + (last - list1), list2 + count * size);
        unsigned char* tail = list2;
        unsigned char* first = list1;
        bool descending = Compare(first, first + size) > 0;
        for (; first < last; descending = !descending) {
            std::size_t length = 2;
            unsigned char* next = first + pair;
            for (; next < last; next += pair) {
                if ((Compare(next, next + size) > 0) != descending) break;
                length += 2;
            }
            if (length < NaturalRunThreshold) {
                do {
                    Link(tail, list2 + (first + pair - list1));
                    if (descending) SwapElements(first, first + size);
                } while ((first += pair) < next);
                continue;
            }
            unsigned char* const end = next;
            for (next = first + pair; next < end; next += pair) {
                if ((Compare(next - size, next) > 0) != descending) {
                    Link(tail, list2 + (next - list1));
                    if (descending) Reverse(first, next - size);
                    first = next;
                }
            }
            if (descending) Reverse(first, next - size);
            first = next;
            if (next < last || Compare(next - size, next) > 0) Link(tail, list2 + (next - list1));
            else Link(tail, list2 + count * size);
        }
    }

    void Gallop(const unsigned char* key, unsigned char*& low, unsigned char*& high, int sense, bool& galloping) const {
        std::size_t step = size;
        for (;; step <<= 1) {
            if (step >= static_cast<std::size_t>(high - low)) {
                unsigned char* const probe = high - size;
                if (probe > low && Compare(key, probe) <= sense) high = probe;
                else low = probe;
                while (high > low + size) {
                    unsigned char* const middle = low + (((high - low) / size) >> 1) * size;
                    if (Compare(key, middle) <= sense) high = middle;
                    else low = middle;
                }
                return;
            }
            unsigned char* const probe = low + step;
            if (Compare(key, probe) <= sense) {
                high = probe;
                if (step == size) galloping = false;
                break;
            }
            low = probe;
        }
        while (step > size) {
            step >>= 1;
            unsigned char* const probe = low + step;
            if (Compare(key, probe) <= sense) high = probe;
            else low = probe;
        }
    }

    void MergePass(unsigned char* list1, unsigned char* list2, unsigned char* last) {
        unsigned char* end2 = list1;
        unsigned char* runLink = list1;
        unsigned char* target = list2;
        auto copyRun = [&target](unsigned char*& from, const unsigned char* to) {
            const auto bytes = static_cast<std::size_t>(to - from);
            std::memcpy(target, from, bytes);
            target += bytes;
            from += bytes;
        };
        for (unsigned char* link = list2; link != last; runLink = end2) {
            link = LoadLink(link);
            unsigned char* first1 = end2;
            unsigned char* const end1 = list1 + (link - list2);
            unsigned char* first2 = end1;
            if (link != last) link = LoadLink(link);
            end2 = list1 + (link - list2);
            while (first1 < end1 && first2 < end2) {
                const unsigned char* key;
                unsigned char* low;
                unsigned char* high;
                int sense;
                if (Compare(first1, first2) <= 0) {
                    key = first2;
                    low = first1;
                    high = end1;
                    sense = -1;
                } else {
                    key = first1;
                    low = first2;
                    high = end2;
                    sense = 0;
                }
                bool gallop = galloping;
                if (!galloping) {
                    while ((low += size) < high && Compare(key, low) > sense) {
                        if (++linearWins == GallopAfter) {
                            galloping = true;
                            gallop = true;
                            break;
                        }
                    }
                }
                if (gallop) {
                    Gallop(key, low, high, sense, galloping);
                    low = high;
                }
                linearWins = 0;
                if (key == first1) {
                    copyRun(first2, low);
                    copyRun(first1, first1 + size);
                } else {
                    copyRun(first1, low);
                    copyRun(first2, first2 + size);
                }
            }
            if (first2 < end2) copyRun(first2, end2);
            else if (first1 < end1) copyRun(first1, end1);
            StoreLink(runLink, end2);
        }
    }

    std::size_t size;
    GuestCompare compare;
    std::size_t linearWins = 0;
    bool galloping = false;
};

}

int APS5_VABI mergesort_nid_postfix(void* base, size_t count, size_t size, GuestCompare compare) {
    if (size < LinkSize / 2) {
        errno = 22;
        return -1;
    }
    if (count == 0) return 0;
    if (!compare) throw std::invalid_argument("mergesort: null comparator");
    if (!base) throw std::invalid_argument("mergesort: null base");
    if (count > (std::numeric_limits<size_t>::max() - LinkSize) / size) throw std::overflow_error("mergesort: array size overflow");
    auto* const scratch = static_cast<unsigned char*>(malloc_nid_postfix(count * size + LinkSize));
    if (!scratch) return -1;
    MergeSorter sorter(size, compare);
    free_nid_postfix(sorter.Sort(static_cast<unsigned char*>(base), count, scratch));
    return 0;
}
