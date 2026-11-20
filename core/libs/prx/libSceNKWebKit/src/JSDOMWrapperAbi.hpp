// Derived from Source/WebCore/bindings/js/JSDOMWrapper.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2018 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "prx/libSceNKWebKit/src/NodeAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct Document;
struct Element;
struct Event;

struct JSCellHeader {
    std::uint32_t structureID;
    std::uint8_t indexingTypeAndMisc;
    std::uint8_t type;
    std::uint8_t flags;
    std::uint8_t cellState;
};

struct JSDOMWrapper {
    JSCellHeader cell;
    void* butterfly;
    const void* classInfo;
    void* wrapped;
};

constexpr std::uint64_t JSValueNotCellMask = 0xfffe000000000002ull;
constexpr std::uint8_t JSEventType = 0b11101111;
constexpr std::uint8_t JSNodeType = 0b11110000;
constexpr std::uint8_t JSNodeTypeMask = 0b00001111;
constexpr std::uint8_t JSElementType = JSNodeType | 1;
constexpr std::uint8_t JSDocumentWrapperType = JSNodeType | 9;

static_assert(sizeof(JSCellHeader) == 8);
static_assert(sizeof(JSDOMWrapper) == 0x20);
static_assert(offsetof(JSDOMWrapper, cell.type) == 0x5);
static_assert(offsetof(JSDOMWrapper, wrapped) == 0x18);

}
