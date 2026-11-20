/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2000 Keith Packard
 * Copyright © 2005 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Graydon Hoare <graydon@redhat.com>
 *	Owen Taylor <otaylor@redhat.com>
 *      Keith Packard <keithp@keithp.com>
 *      Carl Worth <cworth@cworth.org>
 */

#include "prx/libcairo/include/FontFace.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"

#include <CairoBackend.h>

#include <new>

namespace {

constexpr int kReadable = 4;
constexpr int kLoadNoBitmap = 1 << 3;
constexpr int kLoadVerticalLayout = 1 << 4;
constexpr int kLoadForceAutohint = 1 << 5;
constexpr unsigned kSynthesizeBold = 1;
constexpr int kLcdFilterNone = 1;
constexpr int kLcdFilterIntraPixel = 2;
constexpr int kLcdFilterFir3 = 3;
constexpr int kLcdFilterFir5 = 4;

bool Bool(FcPattern* pattern, const char* object, bool fallback) {
    FcBool value{};
    if (FcPatternGetBool_nid_postfix(pattern, object, 0, &value) != FcResultMatch) return fallback;
    return value != 0;
}

int Integer(FcPattern* pattern, const char* object, int fallback) {
    int value{};
    if (FcPatternGetInteger_nid_postfix(pattern, object, 0, &value) != FcResultMatch) return fallback;
    return value;
}

void ReadPatternOptions(FcPattern* pattern, CairoFtFontFaceKey& key) {
    bool bitmap = Bool(pattern, "embeddedbitmap", false);
    if (Bool(pattern, "antialias", true)) {
        const bool hinting = Bool(pattern, "hinting", true);
        int subpixelOrder;
        switch (Integer(pattern, "rgba", 0)) {
        case 1:
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_RGB;
            break;
        case 2:
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_BGR;
            break;
        case 3:
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_VRGB;
            break;
        case 4:
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_VBGR;
            break;
        default:
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_DEFAULT;
            break;
        }
        if (subpixelOrder != CAIRO_SUBPIXEL_ORDER_DEFAULT) {
            key.subpixelOrder = subpixelOrder;
            key.antialias = CAIRO_ANTIALIAS_SUBPIXEL;
        }
        int lcdFilter{};
        if (FcPatternGetInteger_nid_postfix(pattern, "lcdfilter", 0, &lcdFilter) == FcResultMatch) {
            switch (lcdFilter) {
            case 0:
                key.lcdFilter = kLcdFilterNone;
                break;
            case 1:
                key.lcdFilter = kLcdFilterFir5;
                break;
            case 2:
                key.lcdFilter = kLcdFilterFir3;
                break;
            case 3:
                key.lcdFilter = kLcdFilterIntraPixel;
                break;
            }
        }
        switch (hinting ? Integer(pattern, "hintstyle", 3) : 0) {
        case 0:
            key.hintStyle = CAIRO_HINT_STYLE_NONE;
            break;
        case 1:
            key.hintStyle = CAIRO_HINT_STYLE_SLIGHT;
            break;
        case 3:
            key.hintStyle = CAIRO_HINT_STYLE_FULL;
            break;
        default:
            key.hintStyle = CAIRO_HINT_STYLE_MEDIUM;
            break;
        }
        if (key.hintStyle == CAIRO_HINT_STYLE_NONE) bitmap = false;
        if (!bitmap) key.loadFlags |= kLoadNoBitmap;
    } else {
        key.antialias = CAIRO_ANTIALIAS_NONE;
    }
    if (Bool(pattern, "autohint", false)) key.loadFlags |= kLoadForceAutohint;
    if (Bool(pattern, "verticallayout", false)) key.loadFlags |= kLoadVerticalLayout;
    if (Bool(pattern, "embolden", false)) key.synthFlags |= kSynthesizeBold;
    FcChar8* variations{};
    if (FcPatternGetString_nid_postfix(pattern, "fontvariations", 0, &variations) == FcResultMatch)
        key.variations.emplace(reinterpret_cast<const char*>(variations));
}

}

extern "C" CairoFontFace* APS5_VABI cairo_ft_font_face_create_for_pattern(FcPattern* pattern) {
    if (!pattern) NotImplemented_nid_no_patch(__func__);
    CairoFtFontFaceKey key;
    void* face{};
    FcResult result = FcPatternGetFTFace_nid_postfix(pattern, "ftface", 0, &face);
    if (result == FcResultOutOfMemory) return CairoFontFaceNilNoMemory();
    if (result == FcResultMatch) {
        if (!face) NotImplemented_nid_no_patch(__func__);
        key.fromFace = true;
        key.face = face;
    } else {
        FcChar8* filename{};
        result = FcPatternGetString_nid_postfix(pattern, "file", 0, &filename);
        if (result == FcResultOutOfMemory) return CairoFontFaceNilNoMemory();
        if (result != FcResultMatch) {
            FcPattern* duplicate = FcPatternDuplicate_nid_postfix(pattern);
            if (!duplicate) return CairoFontFaceNilNoMemory();
            return CairoFtFontFaceForPattern(duplicate);
        }
        if (access_nid_postfix(reinterpret_cast<const char*>(filename), kReadable) != 0)
            return CairoFontFaceNilFileNotFound();
        int index{};
        if (FcPatternGetInteger_nid_postfix(pattern, "index", 0, &index) == FcResultOutOfMemory)
            return CairoFontFaceNilNoMemory();
        key.id = index;
        try {
            key.filename = reinterpret_cast<const char*>(filename);
        } catch (const std::bad_alloc&) {
            return CairoFontFaceNilNoMemory();
        }
    }
    try {
        ReadPatternOptions(pattern, key);
    } catch (const std::bad_alloc&) {
        return CairoFontFaceNilNoMemory();
    }
    return CairoFtFontFaceFind(key);
}
