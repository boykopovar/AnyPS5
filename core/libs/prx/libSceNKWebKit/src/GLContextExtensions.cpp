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

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstring>
#include <stdexcept>

extern "C" {

bool APS5_VABI _ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_(const char* extensionList, const char* extension) {
    if (!extensionList)
        return false;

    if (!extension)
        NotImplemented_nid_no_patch(__func__);
    const int savedErrno = errno;
    char probe = 0;
    std::size_t safeLength = 0;
    bool terminated = false;
    while (GuestCopyIn_nid_no_patch(&probe, extension + safeLength, sizeof(probe))) {
        if (!probe) {
            terminated = true;
            break;
        }
        ++safeLength;
    }
    errno = savedErrno;
    if (!terminated)
        throw std::invalid_argument(std::string(__func__) + ": unreadable extension string");
    auto length = safeLength;
    if (length > INT_MAX || (!length && *extensionList != ' ' && *extensionList != '\0'))
        NotImplemented_nid_no_patch(__func__);
    int extensionLen = static_cast<int>(length);
    const char* extensionListPtr = extensionList;
    while ((extensionListPtr = std::strstr(extensionListPtr, extension))) {
        if (extensionListPtr[extensionLen] == ' ' || extensionListPtr[extensionLen] == '\0')
            return true;
        extensionListPtr += extensionLen;
    }
    return false;
}

}
