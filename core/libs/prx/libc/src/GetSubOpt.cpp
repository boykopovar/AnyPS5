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
#include <cstring>

extern "C" {

int APS5_VABI getsubopt_nid_postfix(char** optionp, char* const* tokens, char** valuep) {
    if (valuep) *valuep = nullptr;
    if (!optionp || !*optionp) return -1;

    char* p = *optionp;
    while (*p == ',' || *p == ' ' || *p == '\t') ++p;
    if (*p == '\0') {
        *optionp = p;
        return -1;
    }

    char* const suboption = p;
    while (*++p != '\0' && *p != ',' && *p != '=' && *p != ' ' && *p != '\t');

    if (*p != '\0') {
        if (*p == '=') {
            *p = '\0';
            char* const value = ++p;
            while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t') ++p;
            if (*p != '\0') *p++ = '\0';
            if (valuep) *valuep = value;
        } else {
            *p++ = '\0';
        }
        while (*p == ',' || *p == ' ' || *p == '\t') ++p;
    }

    *optionp = p;

    if (tokens) {
        for (int index = 0; *tokens; ++tokens, ++index) {
            if (std::strcmp(suboption, *tokens) == 0) return index;
        }
    }
    return -1;
}

}
