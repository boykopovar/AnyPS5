// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEFONTFT_INCLUDE_FONTFTDRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEFONTFT_INCLUDE_FONTFTDRIVER_HPP

#include <cstdint>
#include <initializer_list>
#include <limits>

#include "prx/libSceFont/include/FontDriver.hpp"

namespace FontFt {

const Font::SysDriver* DriverTable();
const Font::RendererSelection* RendererTable();
int SupportFormats(FontLibrary library, std::initializer_list<const char*> modules);
int SupportFontFormats(FontLibrary library, std::uint32_t formats);

inline bool FitsGuestSize(long size) {
    return static_cast<unsigned long long>(size) <= std::numeric_limits<std::uint32_t>::max();
}

}

#endif
