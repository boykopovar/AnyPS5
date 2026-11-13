// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEGNMDRIVERFORNEOMODE_INCLUDE_EMBEDDEDSHADERS_HPP
#define CORE_LIBS_PRX_LIBSCEGNMDRIVERFORNEOMODE_INCLUDE_EMBEDDEDSHADERS_HPP

#include <array>
#include <cstdint>

namespace GnmNeo {

alignas(256) constexpr std::array<std::uint32_t, 15> FullscreenVs{
    0xbeeb0387u, 0x36020081u, 0x34020281u, 0x360000c2u, 0x4a0202c1u, 0x4a0000c1u, 0x7e020b01u, 0x7e000b00u,
    0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020001u, 0xf800020fu, 0x03030303u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 7> BlackPs{
    0xbeeb0383u, 0x7e000280u, 0x5e000100u, 0xbf800000u, 0xf8001c0fu, 0x00000000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 5> DepthOnlyPs{
    0xbeeb0383u, 0x7e040280u, 0xf8001803u, 0x00000202u, 0xbf810000u,
};

} // namespace GnmNeo

#endif
