/*
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/gen/stringlist.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/stringlist.c */
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::size_t StringListChunk = 20;

[[noreturn]] void StringListFailed() {
    std::fprintf(stderr, "stringlist: %s\n", std::strerror(ENOMEM));
    exit_nid_postfix(1);
    std::abort();
}

}

extern "C" StringList* APS5_VABI sl_init_nid_postfix() {
    auto* list = static_cast<StringList*>(malloc_nid_postfix(sizeof(StringList)));
    if (list == nullptr) StringListFailed();
    list->sl_cur = 0;
    list->sl_max = StringListChunk;
    list->sl_str = static_cast<char**>(malloc_nid_postfix(list->sl_max * sizeof(char*)));
    if (list->sl_str == nullptr) StringListFailed();
    return list;
}

extern "C" int APS5_VABI sl_add_nid_postfix(StringList* list, char* name) {
    if (list->sl_cur == list->sl_max - 1) {
        list->sl_max += StringListChunk;
        list->sl_str = static_cast<char**>(reallocf_nid_postfix(list->sl_str, list->sl_max * sizeof(char*)));
        if (list->sl_str == nullptr) return -1;
    }
    list->sl_str[list->sl_cur++] = name;
    return 0;
}

extern "C" void APS5_VABI sl_free_nid_postfix(StringList* list, int all) {
    if (list == nullptr) return;
    if (list->sl_str) {
        if (all) {
            for (std::size_t i = 0; i < list->sl_cur; ++i) free_nid_postfix(list->sl_str[i]);
        }
        free_nid_postfix(list->sl_str);
    }
    free_nid_postfix(list);
}

extern "C" char* APS5_VABI sl_find_nid_postfix(StringList* list, const char* name) {
    for (std::size_t i = 0; i < list->sl_cur; ++i) {
        if (std::strcmp(list->sl_str[i], name) == 0) return list->sl_str[i];
    }
    return nullptr;
}
