// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2016, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <stdexcept>

extern "C" {
void APS5_VABI _ZNK6icu_6813UnicodeString6toUTF8ERNS_8ByteSinkE_nid_postfix(const Icu::Abi::UnicodeString* self, Icu::Abi::ByteSink* sink) {
    using namespace Icu::Abi;
    if (!self) throw std::invalid_argument("ICU UTF-8 sink: null string");
    const auto length = self->Length();
    if (!length) return;
    if (length < 0) throw std::invalid_argument("ICU UTF-8 sink: negative string length");
    char scratch[1024]{};
    std::int32_t capacity = sizeof(scratch);
    const auto desired = CheckedInt_nid_no_patch(static_cast<std::int64_t>(length) * 3);
    auto getBuffer = Virtual_nid_no_patch<char* (APS5_VABI *)(ByteSink*, std::int32_t, std::int32_t, char*, std::int32_t, std::int32_t*)>(sink, 3);
    auto* buffer = getBuffer(sink, std::min(length, capacity), desired, scratch, capacity, &capacity);
    if (capacity < 0 || (!buffer && capacity) || (!self->Buffer() && length)) return;
    UnicodeString alias{};
    alias.Init();
    alias.storage.fields.lengthAndFlags = UnicodeString::BufferIsReadonly;
    alias.SetLength(length);
    alias.storage.fields.capacity = length;
    alias.storage.fields.array = const_cast<char16_t*>(self->Buffer());
    auto written = _ZNK6icu_6813UnicodeString6toUTF8EiiPci_nid_postfix(&alias, 0, length, buffer, capacity);
    bool owned = false;
    if (written > capacity) {
        buffer = static_cast<char*>(Allocate_nid_no_patch(written));
        if (!buffer) return;
        owned = true;
        written = _ZNK6icu_6813UnicodeString6toUTF8EiiPci_nid_postfix(&alias, 0, length, buffer, written);
    }
    Virtual_nid_no_patch<void (APS5_VABI *)(ByteSink*, const char*, std::int32_t)>(sink, 2)(sink, buffer, written);
    Virtual_nid_no_patch<void (APS5_VABI *)(ByteSink*)>(sink, 4)(sink);
    if (owned) Free_nid_no_patch(buffer);
}
}
