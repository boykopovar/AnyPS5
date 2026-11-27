/* Adapted for AnyPS5 from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * src/regress/lib/libcrypto/bn/bn_convert.c
 * (https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/regress/lib/libcrypto/bn/bn_convert.c).
 * Altered: only the hex round-trip table (bn_convert_tests) is kept; the decimal and ASCII cases are
 * left out because libSceLibreSsl3 does not export BN_bn2dec, BN_dec2bn or BN_asc2bn.
 */
/*	$OpenBSD: bn_convert.c,v 1.6 2024/04/17 08:51:11 jsing Exp $ */
/*
 * Copyright (c) 2023 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <array>

namespace LibreSslRegressBn {

struct ConvertCase {
    const char* hex;
    int neg;
    const char* binHex;
    int binLen;
    int numBits;
};

inline constexpr std::array<ConvertCase, 15> ConvertCases {{
    {"0", 0, "", 0, 0},
    {"01", 0, "01", 1, 1},
    {"-01", 1, "01", 1, 1},
    {"7FFFFF", 0, "7fffff", 3, 23},
    {"-7FFFFF", 1, "7fffff", 3, 23},
    {"FFFFFFFF", 0, "ffffffff", 4, 32},
    {"-FFFFFFFF", 1, "ffffffff", 4, 32},
    {"FFFFFFFF00000000", 0, "ffffffff00000000", 8, 64},
    {"-FFFFFFFF00000000", 1, "ffffffff00000000", 8, 64},
    {"8001800180018001", 0, "8001800180018001", 8, 64},
    {"-8001800180018001", 1, "8001800180018001", 8, 64},
    {"018001800180018001", 0, "018001800180018001", 9, 65},
    {"-018001800180018001", 1, "018001800180018001", 9, 65},
    {"7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF", 0, "7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff", 32, 255},
    {"-7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF", 1, "7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff", 32, 255},
}};

}
