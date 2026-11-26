/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 */
/* Adapted from FreeBSD release/11.4.0 lib/libc/inet/inet_neta.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/inet/inet_neta.c */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" char* APS5_VABI inet_neta_nid_postfix(std::uint32_t source, char* destination, std::size_t size) {
    char* const origin = destination;
    while (source != 0) {
        const auto octet = static_cast<unsigned char>(source >> 24);
        source <<= 8;
        if (octet == 0) continue;
        if (size < sizeof "255.") {
            errno = EMSGSIZE;
            return nullptr;
        }
        char* const start = destination;
        destination += std::snprintf(destination, size, "%u", static_cast<unsigned int>(octet));
        if (source != 0) {
            *destination++ = '.';
            *destination = '\0';
        }
        size -= static_cast<std::size_t>(destination - start);
    }
    if (destination == origin) {
        if (size < sizeof "0.0.0.0") {
            errno = EMSGSIZE;
            return nullptr;
        }
        std::strcpy(destination, "0.0.0.0");
    }
    return origin;
}
