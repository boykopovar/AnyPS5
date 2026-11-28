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

#include "prx/libcairo/include/FtFontOptions.hpp"
#include "prx/libcairo/include/FontOptions.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"

extern "C" void APS5_VABI cairo_ft_font_options_substitute(const cairo_font_options_t* options, FcPattern* pattern) {
    CairoFontOptionsValues values;
    if (!CairoFontOptionsRead(options, values)) return;

    FcValue value;

    if (values.antialias != CAIRO_ANTIALIAS_DEFAULT) {
        if (FcPatternGet_nid_postfix(pattern, "antialias", 0, &value) == FcResultNoMatch) {
            if (!FcPatternAddBool_nid_postfix(pattern, "antialias", values.antialias != CAIRO_ANTIALIAS_NONE)) return;
            if (values.antialias != CAIRO_ANTIALIAS_SUBPIXEL) {
                FcPatternDel_nid_postfix(pattern, "rgba");
                if (!FcPatternAddInteger_nid_postfix(pattern, "rgba", 5)) return;
            }
        }
    }

    if (values.antialias != CAIRO_ANTIALIAS_DEFAULT) {
        if (FcPatternGet_nid_postfix(pattern, "rgba", 0, &value) == FcResultNoMatch) {
            int rgba;
            if (values.antialias == CAIRO_ANTIALIAS_SUBPIXEL) {
                switch (values.subpixelOrder) {
                case CAIRO_SUBPIXEL_ORDER_DEFAULT:
                case CAIRO_SUBPIXEL_ORDER_RGB:
                default:
                    rgba = 1;
                    break;
                case CAIRO_SUBPIXEL_ORDER_BGR:
                    rgba = 2;
                    break;
                case CAIRO_SUBPIXEL_ORDER_VRGB:
                    rgba = 3;
                    break;
                case CAIRO_SUBPIXEL_ORDER_VBGR:
                    rgba = 4;
                    break;
                }
            } else {
                rgba = 5;
            }
            if (!FcPatternAddInteger_nid_postfix(pattern, "rgba", rgba)) return;
        }
    }

    if (values.hintStyle != CAIRO_HINT_STYLE_DEFAULT) {
        if (FcPatternGet_nid_postfix(pattern, "hinting", 0, &value) == FcResultNoMatch) {
            if (!FcPatternAddBool_nid_postfix(pattern, "hinting", values.hintStyle != CAIRO_HINT_STYLE_NONE)) return;
        }
        if (FcPatternGet_nid_postfix(pattern, "hintstyle", 0, &value) == FcResultNoMatch) {
            int hintStyle;
            switch (values.hintStyle) {
            case CAIRO_HINT_STYLE_NONE:
                hintStyle = 0;
                break;
            case CAIRO_HINT_STYLE_SLIGHT:
                hintStyle = 1;
                break;
            case CAIRO_HINT_STYLE_MEDIUM:
                hintStyle = 2;
                break;
            case CAIRO_HINT_STYLE_FULL:
            case CAIRO_HINT_STYLE_DEFAULT:
            default:
                hintStyle = 3;
                break;
            }
            if (!FcPatternAddInteger_nid_postfix(pattern, "hintstyle", hintStyle)) return;
        }
    }
}
