/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FileSystem.cpp

#include "prx/libSceNKWebKit/include/FileSystemCopyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

template <typename Character>
std::string ReadCharacters(const StringImpl& impl) {
    std::vector<Character> characters(impl.length);
    if (!GuestCopyIn_nid_no_patch(characters.data(), impl.data, characters.size() * sizeof(Character)))
        throw std::invalid_argument("WTF::FileSystemImpl::hardLinkOrCopyFile: unreadable path characters");
    const StringView view{characters.data(), impl.length, sizeof(Character) == 1};
    const auto utf8 = _ZNK3WTF10StringView4utf8ENS_14ConversionModeE(&view, 0);
    return reinterpret_cast<const char*>(utf8.buffer + 1);
}

std::string ReadPath(const String* path) {
    StringImpl* pointer = nullptr;
    if (!GuestCopyIn_nid_no_patch(&pointer, path, sizeof(pointer)))
        throw std::invalid_argument("WTF::FileSystemImpl::hardLinkOrCopyFile: unreadable path");
    if (!pointer) return {};
    StringImpl impl{};
    if (!GuestCopyIn_nid_no_patch(&impl, pointer, sizeof(impl)))
        throw std::invalid_argument("WTF::FileSystemImpl::hardLinkOrCopyFile: unreadable path storage");
    if (!impl.length) return {};
    if (impl.length > 0x7fffffff)
        throw std::invalid_argument("WTF::FileSystemImpl::hardLinkOrCopyFile: invalid string length");
    return impl.hashAndFlags & 4 ? ReadCharacters<unsigned char>(impl) : ReadCharacters<char16_t>(impl);
}

}  // namespace

extern "C" bool APS5_VABI _ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_(const String* targetPath, const String* linkPath) {
    const auto target = ReadPath(targetPath);
    const auto link = ReadPath(linkPath);
    if (target.empty() || link.empty()) return false;
    const auto source = ResolvePath_nid_no_patch(target.c_str());
    const auto destination = ResolveDiskPath_nid_no_patch(link.c_str());
    std::error_code error;
    const auto resolvedSource = std::filesystem::canonical(source, error);
    if (!error) std::filesystem::create_hard_link(resolvedSource, destination, error);
    if (error) std::filesystem::copy_file(source, destination, error);
    if (error) return false;
    RecordWrittenPath_nid_no_patch(destination);
    return true;
}
