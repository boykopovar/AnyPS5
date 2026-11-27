// Derived from Source/WebCore/page/UserContentURLPattern.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/WebCore/page/UserContentURLPattern.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libSceNKWebKit/include/UserContentURLPatternExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::UserContentURLPattern;
using NKWebKit::WtfAbi::CodeUnitAt;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr std::size_t NotFound = static_cast<std::size_t>(-1);

StringView View(const String& string) {
    return NKWebKit::WtfAbi::ViewOf(string.impl);
}

char16_t ToASCIILower(char16_t c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char16_t>(c | 0x20) : c;
}

std::size_t Find(const StringView& view, const char* literal, unsigned start) {
    unsigned literalLength = 0;
    while (literal[literalLength]) ++literalLength;
    if (start > view.length || view.length - start < literalLength) return NotFound;
    for (unsigned i = start; i + literalLength <= view.length; ++i) {
        unsigned j = 0;
        while (j < literalLength && CodeUnitAt(view, i + j) == static_cast<unsigned char>(literal[j])) ++j;
        if (j == literalLength) return i;
    }
    return NotFound;
}

bool EqualLiteral(const String& string, const char* literal) {
    if (!string.impl) return false;
    StringView view = View(string);
    unsigned i = 0;
    for (; literal[i]; ++i) {
        if (i >= view.length || CodeUnitAt(view, i) != static_cast<unsigned char>(literal[i])) return false;
    }
    return i == view.length;
}

bool EqualLettersIgnoringASCIICase(const String& string, const char* letters) {
    StringView view = View(string);
    unsigned i = 0;
    for (; letters[i]; ++i) {
        if (i >= view.length || ToASCIILower(CodeUnitAt(view, i)) != static_cast<unsigned char>(letters[i])) return false;
    }
    return i == view.length;
}

bool EqualIgnoringASCIICase(const StringView& a, unsigned aStart, const StringView& b, unsigned length) {
    for (unsigned i = 0; i < length; ++i) {
        if (ToASCIILower(CodeUnitAt(a, aStart + i)) != ToASCIILower(CodeUnitAt(b, i))) return false;
    }
    return true;
}

bool EqualIgnoringASCIICase(const StringView& a, const StringView& b) {
    return a.length == b.length && EqualIgnoringASCIICase(a, 0, b, a.length);
}

bool EndsWithIgnoringASCIICase(const StringView& string, const StringView& suffix) {
    if (suffix.length > string.length) return false;
    return EqualIgnoringASCIICase(string, string.length - suffix.length, suffix, suffix.length);
}

String Substring(const String& string, unsigned start, unsigned length) {
    return _ZN3WTF10StringImpl9substringEjj(string.impl, start, length);
}

bool Parse(UserContentURLPattern& self, const String& pattern) {
    StringView view = View(pattern);
    std::size_t schemeEndPos = Find(view, "://", 0);
    if (schemeEndPos == NotFound)
        return false;

    self.scheme = Substring(pattern, 0, static_cast<unsigned>(schemeEndPos));

    unsigned hostStartPos = static_cast<unsigned>(schemeEndPos) + 3;
    if (hostStartPos >= view.length)
        return false;

    unsigned pathStartPos = 0;

    if (EqualLettersIgnoringASCIICase(self.scheme, "file"))
        pathStartPos = hostStartPos;
    else {
        std::size_t hostEndPos = Find(view, "/", hostStartPos);
        if (hostEndPos == NotFound)
            return false;

        self.host = Substring(pattern, hostStartPos, static_cast<unsigned>(hostEndPos) - hostStartPos);
        self.matchSubdomains = false;

        if (EqualLiteral(self.host, "*")) {
            self.host = String(NKWebKit::WtfAbi::EmptyStringImpl());
            self.matchSubdomains = true;
        } else if (Find(View(self.host), "*.", 0) == 0) {
            String host = self.host;
            self.host = Substring(host, 2, View(host).length - 2);
            self.matchSubdomains = true;
        }

        if (Find(View(self.host), "*", 0) != NotFound)
            return false;

        pathStartPos = static_cast<unsigned>(hostEndPos);
    }

    self.path = Substring(pattern, pathStartPos, view.length - pathStartPos);

    return true;
}

bool MatchesHost(const UserContentURLPattern& self, const URL& test) {
    StringView host = _ZNK3WTF3URL4hostEv(&test);
    StringView patternHost = View(self.host);
    if (EqualIgnoringASCIICase(host, patternHost))
        return true;

    if (!self.matchSubdomains)
        return false;

    if (!patternHost.length)
        return true;

    if (!EndsWithIgnoringASCIICase(host, patternHost))
        return false;

    return CodeUnitAt(host, host.length - patternHost.length - 1) == '.';
}

struct MatchTester {
    StringView pattern;
    unsigned patternIndex { 0 };
    StringView test;
    unsigned testIndex { 0 };

    MatchTester(StringView patternView, StringView testView)
        : pattern(patternView)
        , test(testView)
    {
    }

    bool TestStringFinished() const { return testIndex >= test.length; }
    bool PatternStringFinished() const { return patternIndex >= pattern.length; }

    void EatWildcard() {
        while (!PatternStringFinished()) {
            if (CodeUnitAt(pattern, patternIndex) != '*')
                return;
            patternIndex++;
        }
    }

    void EatSameChars() {
        while (!PatternStringFinished() && !TestStringFinished()) {
            if (CodeUnitAt(pattern, patternIndex) == '*')
                return;
            if (CodeUnitAt(pattern, patternIndex) != CodeUnitAt(test, testIndex))
                return;
            patternIndex++;
            testIndex++;
        }
    }

    bool Test() {
        EatSameChars();

        if (TestStringFinished()) {
            EatWildcard();
            return PatternStringFinished();
        }

        if (PatternStringFinished())
            return false;

        if (CodeUnitAt(pattern, patternIndex) != '*')
            return false;

        while (!TestStringFinished()) {
            MatchTester nextMatch(*this);
            nextMatch.patternIndex++;
            if (nextMatch.Test())
                return true;
            testIndex++;
        }

        EatWildcard();
        return PatternStringFinished();
    }
};

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore21UserContentURLPattern5parseERKN3WTF6StringE(UserContentURLPattern* self, const String* pattern) {
    if (!self || !pattern) throw std::invalid_argument("WebCore::UserContentURLPattern::parse requires a pattern object and a string");
    return Parse(*self, *pattern);
}

bool APS5_VABI _ZNK7WebCore21UserContentURLPattern7matchesERKN3WTF3URLE(const UserContentURLPattern* self, const URL* url) {
    if (!self || !url) throw std::invalid_argument("WebCore::UserContentURLPattern::matches requires a pattern object and a URL");
    if (self->invalid)
        return false;

    if (!EqualLiteral(self->scheme, "*") && !EqualIgnoringASCIICase(_ZNK3WTF3URL8protocolEv(url), View(self->scheme)))
        return false;

    if (!EqualLettersIgnoringASCIICase(self->scheme, "file") && !MatchesHost(*self, *url))
        return false;

    return MatchTester(View(self->path), _ZNK3WTF3URL4pathEv(url)).Test();
}

}
