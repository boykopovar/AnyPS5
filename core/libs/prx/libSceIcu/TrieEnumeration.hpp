// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Trie2.hpp"

namespace Icu {
using Trie2EnumValue = std::uint32_t (APS5_VABI *)(const void* context, std::uint32_t value);
using Trie2EnumRange = std::int8_t (APS5_VABI *)(const void* context, std::int32_t start, std::int32_t end, std::uint32_t value);
}

extern "C" {
void APS5_VABI utrie2_enum_nid_postfix(const Icu::Trie2* trie, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange, const void* context);
void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const Icu::Trie2* trie, std::int32_t lead, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange, const void* context);
}
