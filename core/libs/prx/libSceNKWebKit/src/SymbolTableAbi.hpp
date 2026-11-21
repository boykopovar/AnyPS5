// Derived from Source/JavaScriptCore/runtime/SymbolTable.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscAbi {

inline constexpr std::size_t kVmSymbolTableStructureSlotOffset = 0x9b00;

inline constexpr std::size_t kStructureBlobOffset = 0x8;
inline constexpr std::size_t kStructureIndexingTypeOffset = 0xc;
inline constexpr std::size_t kStructureTypeOffset = 0xd;
inline constexpr std::size_t kStructureInlineTypeFlagsOffset = 0xe;

inline constexpr std::size_t kCellStructureIdOffset = 0x0;
inline constexpr std::size_t kCellIndexingTypeOffset = 0x4;
inline constexpr std::size_t kCellTypeByteOffset = 0x5;
inline constexpr std::size_t kCellInlineTypeFlagsOffset = 0x6;
inline constexpr std::size_t kCellStateOffset = 0x7;
inline constexpr std::uint8_t kCellStateDefinitelyWhite = 0x1;

inline constexpr std::size_t kSymbolTableMapOffset = 0x8;
inline constexpr std::size_t kSymbolTableMaxScopeOffsetOffset = 0x10;
inline constexpr std::uint32_t kInvalidScopeOffset = 0xffffffffu;
inline constexpr std::size_t kSymbolTableLockOffset = 0x14;
inline constexpr std::size_t kSymbolTableFlagsOffset = 0x15;
inline constexpr std::uint8_t kSymbolTableFlagsKeepMask = 0xe0;
inline constexpr std::size_t kSymbolTableRareDataOffset = 0x18;
inline constexpr std::size_t kSymbolTableArgumentsOffset = 0x20;
inline constexpr std::size_t kSymbolTableSingletonOffset = 0x28;
inline constexpr std::uint64_t kSymbolTableSingletonInitial = 0x1;
inline constexpr std::size_t kSymbolTableLocalToEntryOffset = 0x30;
inline constexpr std::size_t kSymbolTableSize = 0x38;

}
