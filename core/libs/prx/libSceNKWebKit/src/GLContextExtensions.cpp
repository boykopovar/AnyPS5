// Derived from Source/WebCore/platform/graphics/egl/GLContext.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba (lines 566-580)
/*
 * Copyright (C) 2012,2023 Igalia, S.L.
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

#include "prx/libSceNKWebKit/include/GLContextExtensionsExports.hpp"
#include "prx/libc/include/General.hpp"

#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

bool CopyGuestCString(const char* source, std::string& destination) {
    destination.clear();
    if (!source) return false;
    constexpr std::size_t kChunkSize = 4096;
    char chunk[kChunkSize];
    auto address = reinterpret_cast<std::uintptr_t>(source);
    for (;;) {
        const std::size_t pageRemaining = kChunkSize - (address & (kChunkSize - 1));
        const std::size_t maxStep = std::numeric_limits<std::uintptr_t>::max() - address;
        const std::size_t count = std::min(pageRemaining, maxStep);
        if (count == 0) {
            char last = '\0';
            if (!GuestCopyIn_nid_no_patch(&last, reinterpret_cast<const void*>(address), 1))
                return false;
            return last == '\0';
        }
        if (!GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), count))
            return false;
        if (const auto* end = static_cast<const char*>(std::memchr(chunk, '\0', count))) {
            destination.append(chunk, static_cast<std::size_t>(end - chunk));
            return true;
        }
        destination.append(chunk, count);
        address += count;
    }
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_(const char* extensionList, const char* extension) {
    if (!extensionList)
        return false;

    if (!extension)
        NotImplemented_nid_no_patch(__func__);
    std::string localExtension;
    if (CopyGuestString_nid_no_patch(extension, 65536, localExtension) != 0)
        throw std::invalid_argument(std::string(__func__) + ": unreadable extension string");
    std::string localExtensionList;
    if (!CopyGuestCString(extensionList, localExtensionList))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    auto length = localExtension.size();
    const char* listText = localExtensionList.c_str();
    if (length > INT_MAX || (!length && *listText != ' ' && *listText != '\0'))
        NotImplemented_nid_no_patch(__func__);
    int extensionLen = static_cast<int>(length);
    const char* extensionText = localExtension.c_str();
    if (!extensionText[0]) {
        return true;
    }
    const char* extensionListPtr = listText;
    while ((extensionListPtr = std::strstr(extensionListPtr, extensionText))) {
        if (extensionListPtr[extensionLen] == ' ' || extensionListPtr[extensionLen] == '\0')
            return true;
        extensionListPtr += extensionLen;
    }
    return false;
}

}
