/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WTF/wtf/posix/FileSystemPOSIX.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/include/WtfFileSystemExports.hpp"
#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr int GuestEintr = 4;

const char* CStringData(const Jsc::WtfAbi::CString& string) {
    return string.buffer ? reinterpret_cast<const char*>(string.buffer + 1) : nullptr;
}

constexpr unsigned StringImplRefCountIncrement = 2;
constexpr std::size_t StringImplCharactersOffset = offsetof(Jsc::WtfAbi::StringImpl, hashAndFlags) + sizeof(Jsc::WtfAbi::StringImpl::hashAndFlags);

struct HostString {
    std::vector<unsigned char> bytes;
    unsigned length = 0;
    bool is8Bit = true;
    const void* data = nullptr;

    char16_t At(unsigned index) const {
        if (is8Bit) return bytes[index];
        char16_t character;
        std::memcpy(&character, bytes.data() + index * sizeof(char16_t), sizeof(character));
        return character;
    }
};

HostString ReadString(const Jsc::WtfAbi::String* reference, const char* error) {
    Jsc::WtfAbi::String string{};
    if (!GuestPtr<Jsc::WtfAbi::String>(reference).CopyIn(string)) throw std::invalid_argument(error);
    HostString result;
    if (!string.impl) return result;
    Jsc::WtfAbi::StringImpl impl{};
    if (!GuestPtr<Jsc::WtfAbi::StringImpl>(string.impl).CopyIn(impl)) throw std::invalid_argument(error);
    result.length = impl.length;
    result.is8Bit = impl.hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit;
    result.data = impl.data;
    if (result.length && !result.data) throw std::invalid_argument(error);
    return result;
}

void LoadCharacters(HostString& string, const char* error) {
    string.bytes.resize(static_cast<std::size_t>(string.length) * (string.is8Bit ? 1 : sizeof(char16_t)));
    if (!string.bytes.empty() && !GuestBytes(string.data).CopyIn(string.bytes.data(), string.bytes.size())) throw std::invalid_argument(error);
}

void Append(unsigned char* destination, bool is8Bit, unsigned& offset, const HostString& string) {
    for (unsigned index = 0; index < string.length; ++index, ++offset) {
        const char16_t character = string.At(index);
        if (is8Bit) destination[offset] = static_cast<unsigned char>(character);
        else std::memcpy(destination + offset * sizeof(char16_t), &character, sizeof(character));
    }
}

} // namespace

extern "C" {

int APS5_VABI _ZN3WTF14FileSystemImpl12readFromFileEiPci(int handle, char* data, int length) {
    do {
        const std::int64_t bytesRead = read_nid_postfix(handle, data, static_cast<std::size_t>(length));
        if (bytesRead >= 0) return static_cast<int>(bytesRead);
    } while (*__error_nid_postfix() == GuestEintr);
    return -1;
}

Jsc::WtfAbi::OptionalInt32 APS5_VABI _ZN3WTF14FileSystemImpl15getFileDeviceIdERKNS_7CStringE(const Jsc::WtfAbi::CString* fsFile) {
    Jsc::WtfAbi::CString file{};
    if (!GuestPtr<Jsc::WtfAbi::CString>(fsFile).CopyIn(file)) throw std::invalid_argument("WTF::FileSystemImpl::getFileDeviceId: invalid CString reference");
    FileStat fileStat{};
    if (stat_nid_postfix(CStringData(file), &fileStat) == -1) return {};
    return {true, static_cast<std::int32_t>(fileStat.st_dev)};
}

Jsc::WtfAbi::StringImpl** APS5_VABI _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(Jsc::WtfAbi::StringImpl** result, const Jsc::WtfAbi::String* path, const Jsc::WtfAbi::String* component) {
    constexpr const char* pathError = "WTF::FileSystemImpl::pathByAppendingComponent: invalid path reference";
    constexpr const char* componentError = "WTF::FileSystemImpl::pathByAppendingComponent: invalid component reference";
    HostString head = ReadString(path, pathError);
    HostString tail = ReadString(component, componentError);
    bool endsWithSlash = false;
    if (head.length) {
        HostString last{{}, 1, head.is8Bit, static_cast<const unsigned char*>(head.data) + (head.length - 1) * (head.is8Bit ? 1 : sizeof(char16_t))};
        LoadCharacters(last, pathError);
        endsWithSlash = last.At(0) == '/';
    }
    const HostString separator{{'/'}, endsWithSlash ? 0u : 1u, true};
    const std::uint64_t length = static_cast<std::uint64_t>(head.length) + separator.length + tail.length;
    if (length > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("WTF::FileSystemImpl::pathByAppendingComponent: result longer than String::MaxLength");
    LoadCharacters(head, pathError);
    LoadCharacters(tail, componentError);
    const auto* allocator = RegisteredPersistenceAllocator_nid_no_patch();
    if (!allocator || !allocator->allocate || !allocator->deallocate)
        throw std::runtime_error("WTF::FileSystemImpl::pathByAppendingComponent: the string requires libSceNKWebKit fastMalloc");
    const bool is8Bit = head.is8Bit && tail.is8Bit;
    void* storage = allocator->allocate(StringImplCharactersOffset + length * (is8Bit ? 1 : sizeof(char16_t)));
    if (!storage) throw std::bad_alloc();
    auto* characters = static_cast<unsigned char*>(storage) + StringImplCharactersOffset;
    unsigned offset = 0;
    Append(characters, is8Bit, offset, head);
    Append(characters, is8Bit, offset, separator);
    Append(characters, is8Bit, offset, tail);
    auto* string = new (storage) Jsc::WtfAbi::StringImpl{StringImplRefCountIncrement, offset, characters, is8Bit ? Jsc::WtfAbi::StringImplFlag8Bit : 0u};
    if (!GuestPtr<Jsc::WtfAbi::StringImpl*>(result).CopyOut(string)) {
        allocator->deallocate(storage);
        throw std::invalid_argument("WTF::FileSystemImpl::pathByAppendingComponent: invalid return storage");
    }
    return result;
}

}
