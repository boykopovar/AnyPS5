/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/JavaScriptCore/inspector/scripts/codegen/generate_cpp_protocol_types_implementation.py@c55364d53f971f63a709976f168d932d558b8909
 */

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Derived from Source/WTF/wtf/text/StringImpl.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libSceJsc/include/InspectorEnumParsingExports.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using Jsc::InspectorAbi::ConsoleMessageLevel;
using Jsc::InspectorAbi::CoordinateSystem;
using Jsc::InspectorAbi::ExecutionContextType;
using Jsc::InspectorAbi::OptionalEnum;
using Jsc::InspectorAbi::ProtocolString;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringImplFlag8Bit;

constexpr std::size_t MaxEnumNameLength = 8;

template<typename TEnum>
struct EnumEntry {
    std::string_view name;
    TEnum value;
};

constexpr EnumEntry<ExecutionContextType> ExecutionContextTypes[] = {
    {"normal", ExecutionContextType::Normal},
    {"user", ExecutionContextType::User},
    {"internal", ExecutionContextType::Internal},
};

constexpr EnumEntry<CoordinateSystem> CoordinateSystems[] = {
    {"Viewport", CoordinateSystem::Viewport},
    {"Page", CoordinateSystem::Page},
};

constexpr EnumEntry<ConsoleMessageLevel> ConsoleMessageLevels[] = {
    {"log", ConsoleMessageLevel::Log},
    {"info", ConsoleMessageLevel::Info},
    {"warning", ConsoleMessageLevel::Warning},
    {"error", ConsoleMessageLevel::Error},
    {"debug", ConsoleMessageLevel::Debug},
};

template<typename TEnum, std::size_t N>
OptionalEnum<TEnum> ParseEnum(const ProtocolString* protocolString, const EnumEntry<TEnum> (&entries)[N], const char* function) {
    ProtocolString string{};
    if (!GuestPtr<ProtocolString>(protocolString).CopyIn(string)) {
        throw std::invalid_argument(std::string(function) + ": invalid String reference");
    }
    if (!string.impl) return {};

    StringImpl impl{};
    if (!GuestPtr<StringImpl>(string.impl).CopyIn(impl)) {
        throw std::invalid_argument(std::string(function) + ": invalid StringImpl");
    }
    if (impl.length == 0) return {};
    if (!impl.data) throw std::invalid_argument(std::string(function) + ": null StringImpl data");

    bool lengthCouldMatch = false;
    for (const auto& entry : entries) {
        if (impl.length == entry.name.size()) {
            lengthCouldMatch = true;
            break;
        }
    }
    if (!lengthCouldMatch || impl.length > MaxEnumNameLength) return {};

    std::array<char16_t, MaxEnumNameLength> units{};
    if ((impl.hashAndFlags & StringImplFlag8Bit) != 0) {
        std::array<std::uint8_t, MaxEnumNameLength> bytes{};
        if (!GuestBytes(impl.data).CopyIn(bytes.data(), impl.length)) {
            throw std::invalid_argument(std::string(function) + ": invalid 8-bit StringImpl data");
        }
        for (unsigned i = 0; i < impl.length; ++i) units[i] = bytes[i];
    } else {
        if (!GuestBytes(impl.data).CopyIn(units.data(), static_cast<std::size_t>(impl.length) * sizeof(char16_t))) {
            throw std::invalid_argument(std::string(function) + ": invalid 16-bit StringImpl data");
        }
    }

    for (const auto& entry : entries) {
        if (entry.name.size() != impl.length) continue;
        bool matched = true;
        for (unsigned i = 0; i < impl.length; ++i) {
            if (units[i] != static_cast<char16_t>(static_cast<unsigned char>(entry.name[i]))) {
                matched = false;
                break;
            }
        }
        if (matched) return {true, entry.value};
    }
    return {};
}

} // namespace

extern "C" {

OptionalEnum<ExecutionContextType> APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE(const ProtocolString* protocolString) {
    return ParseEnum(protocolString, ExecutionContextTypes, __func__);
}

OptionalEnum<CoordinateSystem> APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE(const ProtocolString* protocolString) {
    return ParseEnum(protocolString, CoordinateSystems, __func__);
}

OptionalEnum<ConsoleMessageLevel> APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE(const ProtocolString* protocolString) {
    return ParseEnum(protocolString, ConsoleMessageLevels, __func__);
}

}
