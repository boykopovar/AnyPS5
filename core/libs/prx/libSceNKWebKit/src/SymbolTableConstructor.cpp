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

#include "prx/libSceNKWebKit/include/SymbolTableExports.hpp"
#include "prx/libSceNKWebKit/src/SymbolTableAbi.hpp"

#include <cstring>
#include <stdexcept>

namespace {

using namespace NKWebKit::JscAbi;

template <typename T>
T Load(const unsigned char* base, std::size_t offset) {
    T value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

template <typename T>
void Store(unsigned char* base, std::size_t offset, T value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

const unsigned char* SymbolTableStructure(const unsigned char* vm) {
    const unsigned char* slot = Load<const unsigned char*>(vm, kVmSymbolTableStructureSlotOffset);
    if (!slot) throw std::invalid_argument("null JSC::VM symbol table structure slot");
    const unsigned char* structure = Load<const unsigned char*>(slot, 0);
    if (!structure) throw std::invalid_argument("null JSC::VM symbol table structure");
    return structure;
}

void ConstructSymbolTable(void* self, void* vmPointer) {
    if (!self) throw std::invalid_argument("null JSC::SymbolTable");
    if (!vmPointer) throw std::invalid_argument("null JSC::VM");
    unsigned char* table = static_cast<unsigned char*>(self);
    const unsigned char* structure = SymbolTableStructure(static_cast<const unsigned char*>(vmPointer));

    Store<std::uint32_t>(table, kCellStructureIdOffset, Load<std::uint32_t>(structure, kStructureBlobOffset));
    Store<std::uint8_t>(table, kCellIndexingTypeOffset, Load<std::uint8_t>(structure, kStructureIndexingTypeOffset));
    Store<std::uint8_t>(table, kCellTypeByteOffset, Load<std::uint8_t>(structure, kStructureTypeOffset));
    Store<std::uint8_t>(table, kCellInlineTypeFlagsOffset, Load<std::uint8_t>(structure, kStructureInlineTypeFlagsOffset));
    Store<std::uint8_t>(table, kCellStateOffset, kCellStateDefinitelyWhite);

    Store<std::uint64_t>(table, kSymbolTableMapOffset, 0);
    Store<std::uint32_t>(table, kSymbolTableMaxScopeOffsetOffset, kInvalidScopeOffset);
    Store<std::uint8_t>(table, kSymbolTableLockOffset, 0);
    Store<std::uint8_t>(table, kSymbolTableFlagsOffset, Load<std::uint8_t>(table, kSymbolTableFlagsOffset) & kSymbolTableFlagsKeepMask);
    Store<std::uint64_t>(table, kSymbolTableRareDataOffset, 0);
    Store<std::uint64_t>(table, kSymbolTableArgumentsOffset, 0);
    Store<std::uint64_t>(table, kSymbolTableSingletonOffset, kSymbolTableSingletonInitial);
    Store<std::uint64_t>(table, kSymbolTableLocalToEntryOffset, 0);
}

}

extern "C" {

void APS5_VABI _ZN3JSC11SymbolTableC1ERNS_2VME(void* self, void* vm) {
    ConstructSymbolTable(self, vm);
}

void APS5_VABI _ZN3JSC11SymbolTableC2ERNS_2VME(void* self, void* vm) {
    ConstructSymbolTable(self, vm);
}

}
