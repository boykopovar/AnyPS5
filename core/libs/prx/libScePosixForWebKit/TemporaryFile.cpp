/*
 * Copyright (c) 1987, 1993
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

#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr char Characters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr int SharedLock = 0x10;
constexpr int ExclusiveLock = 0x20;

int Failure(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

int TemporaryFile(char* pattern, int flags) {
    if (!pattern) return Failure(14);
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(pattern, copy)) return Failure(error);
    const auto length = copy.size();
    if (length == 0) return Failure(22);
    auto start = length;
    while (start > 0 && copy[start - 1] == 'X') --start;
    const GuestArena::HostWrite destination(pattern + start, length - start);
    if (!destination.Open() || !GuestWritable_nid_no_patch(pattern + start, length - start)) return Failure(14);
    if (start != length) {
        std::random_device random;
        std::uniform_int_distribution<unsigned int> character(0, sizeof(Characters) - 2);
        for (auto position = start; position < length; ++position)
            pattern[position] = Characters[character(random)];
    }
    const std::string initial(pattern + start, length - start);
    if (const auto slash = std::string_view(pattern, start).find_last_of('/'); slash != std::string_view::npos && slash > 0) {
        const std::string directory(pattern, slash);
        FileStat status{};
        if (stat_nid_postfix(directory.c_str(), &status) != 0) return -1;
        if ((status.st_mode & 0170000) != 0040000) return Failure(20);
    }
    for (;;) {
        const int descriptor = open_nid_postfix(pattern, SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL |
            (flags & ~(SharedLock | ExclusiveLock)), 0600);
        if (descriptor >= 0) {
            try {
                if ((flags & (SharedLock | ExclusiveLock)) &&
                    flock_nid_postfix(descriptor, (flags & ExclusiveLock) ? 2 : 1) != 0) {
                    const int error = *__error_nid_postfix();
                    close_nid_postfix(descriptor);
                    return Failure(error);
                }
            } catch (...) {
                const int error = *__error_nid_postfix();
                close_nid_postfix(descriptor);
                *__error_nid_postfix() = error;
                throw;
            }
            return descriptor;
        }
        if (*__error_nid_postfix() != 17) return -1;
        auto position = start;
        for (; position < length; ++position) {
            const auto index = std::strchr(Characters, pattern[position]) - Characters;
            pattern[position] = Characters[(index + 1) % (sizeof(Characters) - 1)];
            if (pattern[position] != initial[position - start]) break;
        }
        if (position == length) return -1;
    }
}

}

extern "C" int APS5_VABI mkstemp_nid_postfix(char* pattern) {
    return TemporaryFile(pattern, 0);
}

extern "C" int APS5_VABI mkostemp_nid_postfix(char* pattern, int flags) {
    constexpr int accepted = SCE_KERNEL_O_APPEND | SCE_KERNEL_O_DIRECT | SharedLock | ExclusiveLock |
        SCE_KERNEL_O_SYNC | SCE_KERNEL_O_CLOEXEC;
    if (flags & ~accepted) return Failure(22);
    if (!pattern) throw std::invalid_argument("mkostemp: null template");
    return TemporaryFile(pattern, flags);
}
