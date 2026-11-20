/*

Copyright (C) 2014-2019 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Derived from Source/WTF/wtf/text/StringView.cpp@c55364d53f971f63a709976f168d932d558b8909
*/

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfHelperExports.hpp"

#include <stdexcept>

extern "C" {

bool APS5_VABI _ZNK3WTF10StringView16GraphemeClusters8IteratorneERKS2_(const Jsc::WtfAbi::GraphemeClusterIterator* self, const Jsc::WtfAbi::GraphemeClusterIterator* other) {
    if (!self || !other || !self->impl || !other->impl) throw std::invalid_argument("WTF::StringView::GraphemeClusters::Iterator::operator!=: null iterator");
    return self->impl->index != other->impl->index;
}

}
