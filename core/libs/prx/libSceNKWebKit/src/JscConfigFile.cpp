/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * Derived from Source/JavaScriptCore/runtime/ConfigFile.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * and Source/JavaScriptCore/runtime/ConfigFile.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/ConfigFileExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/ConfigFileAbi.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <regex>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::JscAbi::ConfigFile;
using NKWebKit::JscAbi::ConfigFilePathMax;

constexpr std::size_t ProcessNameMax = 128;
constexpr std::size_t ScanBufferSize = 1024;
constexpr std::size_t GuestStringMax = 65536;

char ProcessName[ProcessNameMax + 1];
char ParentProcessName[ProcessNameMax + 1];

bool IsSpace(char character) {
    return character == ' ' || character == '\n' || character == '\t' || character == '\r' || character == '\f' || character == '\v';
}

void RequireGuestCopy(bool copied, int savedErrno, const std::string& message) {
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(message);
}

std::string GuestString(const char* text, const std::string& message) {
    std::string value;
    if (CopyGuestString_nid_no_patch(text, GuestStringMax, value) != 0) throw std::invalid_argument(message);
    return value;
}

void CopyPadded(char* destination, std::size_t count, const std::string& source) {
    std::memset(destination, 0, count);
    std::memcpy(destination, source.data(), std::min(count, source.size()));
}

void StoreProcessName(char* destination, const char* value, const std::string& message) {
    if (!value) throw std::invalid_argument(message);
    CopyPadded(destination, ProcessNameMax, GuestString(value, message));
}

class Scanner {
public:
    explicit Scanner(const char* configFilename) : filename(configFilename) {
        srcPtr = &buffer[0];
        bufferEnd = &buffer[0];
    }

    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;

    ~Scanner() {
        if (file) fclose_nid_postfix(file);
    }

    bool Start() {
        file = fopen_nid_postfix(filename, "r");
        if (!file) {
            _ZN3WTF8dataLogFEPKcz("Failed to open file JSC Config file '%s'.\n", filename);
            return false;
        }
        return true;
    }

    unsigned LineNumber() const { return lineNumber; }

    const char* CurrentBuffer() const {
        if (!srcPtr || srcPtr == bufferEnd) return "";
        return srcPtr;
    }

    bool AtFileEnd() { return !FillBufferIfNeeded(); }

    bool TryConsume(char character) {
        if (!FillBufferIfNeeded()) return false;
        if (character == *srcPtr) {
            srcPtr++;
            return true;
        }
        return false;
    }

    bool TryConsume(const char* token, std::size_t tokenLength) {
        if (!FillBufferIfNeeded()) return false;
        if (!std::strncmp(srcPtr, token, tokenLength)) {
            srcPtr += tokenLength;
            return true;
        }
        return false;
    }

    char* TryConsumeString() {
        if (!FillBufferIfNeeded()) return nullptr;
        if (*srcPtr != '"') return nullptr;
        char* stringStart = ++srcPtr;
        char* stringEnd = std::strchr(srcPtr, '"');
        if (!stringEnd) return nullptr;
        *stringEnd = '\0';
        srcPtr = stringEnd + 1;
        return stringStart;
    }

    char* TryConsumeRegExPattern(bool& ignoreCase) {
        if (!FillBufferIfNeeded()) return nullptr;
        if (*srcPtr != '/') return nullptr;
        char* stringStart = srcPtr + 1;
        char* stringEnd = std::strchr(stringStart, '/');
        if (!stringEnd) return nullptr;
        *stringEnd = '\0';
        srcPtr = stringEnd + 1;
        if (*srcPtr == 'i') {
            ignoreCase = true;
            srcPtr++;
        } else
            ignoreCase = false;
        return stringStart;
    }

    char* TryConsumeUpto(bool& foundChar, char character) {
        if (!FillBufferIfNeeded()) return nullptr;
        char* start = srcPtr;
        foundChar = false;
        char* characterPosition = std::strchr(srcPtr, character);
        if (characterPosition) {
            *characterPosition = '\0';
            srcPtr = characterPosition + 1;
            foundChar = true;
        } else
            srcPtr = bufferEnd;
        return start;
    }

private:
    bool FillBufferIfNeeded() {
        if (!srcPtr) return false;
        while (true) {
            while (srcPtr != bufferEnd && IsSpace(*srcPtr)) srcPtr++;
            if (srcPtr != bufferEnd) break;
            if (!FillBuffer()) return false;
        }
        return true;
    }

    bool FillBuffer() {
        do {
            srcPtr = fgets_nid_postfix(buffer, static_cast<int>(sizeof buffer), file);
            if (!srcPtr) {
                fclose_nid_postfix(file);
                file = nullptr;
                return false;
            }
            lineNumber++;
            bufferEnd = std::strchr(srcPtr, '#');
            if (bufferEnd)
                *bufferEnd = '\0';
            else {
                bufferEnd = srcPtr + std::strlen(srcPtr);
                if (bufferEnd > srcPtr && bufferEnd[-1] == '\n') {
                    bufferEnd--;
                    *bufferEnd = '\0';
                }
            }
        } while (bufferEnd == srcPtr);
        return true;
    }

    const char* filename;
    unsigned lineNumber = 0;
    FileStream* file = nullptr;
    char buffer[ScanBufferSize];
    char* srcPtr;
    char* bufferEnd;
};

enum class Nesting { TopLevel, Nested, NestedFailedCriteria };
enum class Result { Ok, Error, NestedDone };

class Parser {
public:
    Parser(Scanner& scanner, const ConfigFile& configFile) : scanner(scanner), configFile(configFile) {}

    Result ParseStatement(Nesting nesting) {
        if (scanner.TryConsume("jscOptions", 10)) return ParseJSCOptions(nesting);
        if (scanner.TryConsume("logFile", 7)) return ParseLogFile(nesting);
        if (nesting == Nesting::TopLevel) return ParseConditionalBlock(Nesting::Nested);
        return Result::Error;
    }

    const char* LogPathname() const { return logPathname; }
    const std::string& Options() const { return jscOptions; }

private:
    Result ParseLogFile(Nesting nesting) {
        char* filename = nullptr;
        if (scanner.TryConsume('=') && (filename = scanner.TryConsumeString())) {
            if (nesting != Nesting::NestedFailedCriteria) {
                if (filename[0] != '/') {
                    int spaceRequired = std::snprintf(logPathname, ConfigFilePathMax + 1, "%s/%s", configFile.configDirectory, filename);
                    if (static_cast<unsigned>(spaceRequired) > ConfigFilePathMax) return Result::Error;
                } else
                    std::strncpy(logPathname, filename, ConfigFilePathMax);
            }
            return Result::Ok;
        }
        return Result::Error;
    }

    Result ParseJSCOptions(Nesting nesting) {
        if (!scanner.TryConsume('{')) return Result::Error;
        std::string builder;
        bool foundClosingBrace = false;
        char* currentLine = nullptr;
        while ((currentLine = scanner.TryConsumeUpto(foundClosingBrace, '}'))) {
            char* p = currentLine;
            do {
                if (foundClosingBrace && !*p) break;
                char* optionNameStart = p;
                while (*p && !IsSpace(*p) && *p != '=') p++;
                builder.append(optionNameStart, p - optionNameStart);
                while (*p && IsSpace(*p) && *p != '=') p++;
                if (!*p) return Result::Error;
                p++;
                builder.push_back('=');
                while (*p && IsSpace(*p)) p++;
                if (!*p) return Result::Error;
                char* optionValueStart = p;
                while (*p && !IsSpace(*p)) p++;
                builder.append(optionValueStart, p - optionValueStart);
                builder.push_back('\n');
                while (*p && IsSpace(*p)) p++;
            } while (*p);
            if (foundClosingBrace) break;
        }
        if (nesting != Nesting::NestedFailedCriteria) jscOptions.append(builder);
        return Result::Ok;
    }

    Result ParseNestedStatement(Nesting nesting) {
        if (scanner.TryConsume("jscOptions", 10)) return ParseJSCOptions(nesting);
        if (scanner.TryConsume("logFile", 7)) return ParseLogFile(nesting);
        if (scanner.TryConsume('}')) return Result::NestedDone;
        return Result::Error;
    }

    bool ParsePredicate(bool& predicateMatches, const char* matchValue) {
        if (scanner.TryConsume("==", 2)) {
            char* predicateValue = nullptr;
            if ((predicateValue = scanner.TryConsumeString()) && matchValue) {
                predicateMatches = !std::strcmp(predicateValue, matchValue);
                return true;
            }
            return false;
        }
        if (scanner.TryConsume("=~", 2)) {
            char* pattern = nullptr;
            bool ignoreCase = false;
            if ((pattern = scanner.TryConsumeRegExPattern(ignoreCase)) && matchValue) {
                std::regex::flag_type flags = std::regex::extended;
                if (ignoreCase) flags |= std::regex::icase;
                std::regex expression;
                try {
                    expression.assign(pattern, flags);
                } catch (const std::regex_error&) {
                    return false;
                }
                predicateMatches = std::regex_search(matchValue, expression);
                return true;
            }
        }
        return false;
    }

    Result ParseConditionalBlock(Nesting nesting) {
        if (nesting != Nesting::Nested) return Result::Error;
        Nesting subNesting = Nesting::Nested;
        while (true) {
            bool predicateMatches = false;
            const char* actualValue = nullptr;
            if (scanner.TryConsume("processName", 11))
                actualValue = ProcessName;
            else if (scanner.TryConsume("parentProcessName", 17))
                actualValue = ParentProcessName;
            else if (scanner.TryConsume("build", 5))
                actualValue = "Release";
            else
                return Result::Error;

            if (!ParsePredicate(predicateMatches, actualValue)) return Result::Error;
            if (!predicateMatches) subNesting = Nesting::NestedFailedCriteria;
            if (!scanner.TryConsume("&&", 2)) break;
        }
        if (!scanner.TryConsume('{')) return Result::Error;
        Result parseResult = Result::Ok;
        while (parseResult == Result::Ok && !scanner.AtFileEnd())
            parseResult = ParseNestedStatement(subNesting);
        if (parseResult == Result::NestedDone) return Result::Ok;
        return Result::Error;
    }

    Scanner& scanner;
    const ConfigFile& configFile;
    char logPathname[ConfigFilePathMax + 1] = {0};
    std::string jscOptions;
};

void CanonicalizePaths(ConfigFile& configFile) {
    if (!configFile.filename[0]) return;
    if (configFile.filename[0] != '/') {
        char filenameBuffer[ConfigFilePathMax + 1];
        if (getcwd_nid_postfix(filenameBuffer, sizeof filenameBuffer)) {
            const std::size_t pathnameLength = std::strlen(filenameBuffer);
            const bool shouldAddPathSeparator = pathnameLength && filenameBuffer[pathnameLength - 1] != '/';
            if (pathnameLength && ConfigFilePathMax >= pathnameLength + shouldAddPathSeparator) {
                if (shouldAddPathSeparator) std::strncat(filenameBuffer, "/", 2);
                std::strncat(filenameBuffer, configFile.filename, sizeof(filenameBuffer) - std::strlen(filenameBuffer) - 1);
                std::strncpy(configFile.filename, filenameBuffer, ConfigFilePathMax);
                configFile.filename[ConfigFilePathMax] = '\0';
            }
        }
    }

    char* lastPathSeparator = std::strrchr(configFile.filename, '/');
    if (lastPathSeparator) {
        const unsigned dirnameLength = static_cast<unsigned>(lastPathSeparator - &configFile.filename[0]);
        std::strncpy(configFile.configDirectory, configFile.filename, dirnameLength);
        configFile.configDirectory[dirnameLength] = '\0';
    } else {
        configFile.configDirectory[0] = '/';
        configFile.configDirectory[1] = '\0';
    }
}

void ParseStatements(ConfigFile& configFile) {
    Scanner scanner(configFile.filename);
    if (!scanner.Start()) return;

    Parser parser(scanner, configFile);
    Result parseResult = Result::Ok;
    while (parseResult == Result::Ok && !scanner.AtFileEnd())
        parseResult = parser.ParseStatement(Nesting::TopLevel);

    if (parseResult != Result::Ok) {
        _ZN3WTF8dataLogFEPKcz("Error in JSC Config file on or near line %u, parsing '%s'\n", scanner.LineNumber(), scanner.CurrentBuffer());
        return;
    }
    if (std::strlen(parser.LogPathname()))
        throw std::logic_error("JSC::ConfigFile::parse: WTF::setDataFile is not implemented");
    if (!parser.Options().empty())
        throw std::logic_error("JSC::ConfigFile::parse: JSC::Options::setOptions is not implemented");
}

void RequireTerminated(const ConfigFile& configFile, const std::string& message) {
    if (configFile.filename[ConfigFilePathMax] || configFile.configDirectory[ConfigFilePathMax]) throw std::invalid_argument(message);
}

void Construct(NKWebKit::JscAbi::ConfigFile* self, const char* filename, const std::string& message) {
    if (!self) throw std::invalid_argument(message + ": null object");
    const int savedErrno = errno;
    ConfigFile configFile{};
    if (filename) {
        const std::string name = GuestString(filename, message + ": unreadable file name");
        std::strncpy(configFile.filename, name.c_str(), ConfigFilePathMax);
        configFile.filename[ConfigFilePathMax] = '\0';
    }
    configFile.configDirectory[0] = '\0';
    RequireGuestCopy(GuestCopyOut_nid_no_patch(self, &configFile, sizeof configFile), savedErrno, message + ": unwritable object");
}

}

extern "C" {

void APS5_VABI _ZN3JSC10ConfigFileC1EPKc(NKWebKit::JscAbi::ConfigFile* self, const char* filename) {
    Construct(self, filename, __func__);
}

void APS5_VABI _ZN3JSC10ConfigFileC2EPKc(NKWebKit::JscAbi::ConfigFile* self, const char* filename) {
    Construct(self, filename, __func__);
}

void APS5_VABI _ZN3JSC10ConfigFile14setProcessNameEPKc(const char* processName) {
    StoreProcessName(ProcessName, processName, std::string(__func__) + ": null process name");
}

void APS5_VABI _ZN3JSC10ConfigFile20setParentProcessNameEPKc(const char* parentProcessName) {
    StoreProcessName(ParentProcessName, parentProcessName, std::string(__func__) + ": null parent process name");
}

void APS5_VABI _ZN3JSC10ConfigFile5parseEv(NKWebKit::JscAbi::ConfigFile* self) {
    if (!self) throw std::invalid_argument(std::string(__func__) + ": null object");
    const int savedErrno = errno;
    ConfigFile configFile{};
    RequireGuestCopy(GuestCopyIn_nid_no_patch(&configFile, self, sizeof configFile), savedErrno, std::string(__func__) + ": unreadable object");
    RequireTerminated(configFile, std::string(__func__) + ": unterminated paths");
    CanonicalizePaths(configFile);
    RequireGuestCopy(GuestCopyOut_nid_no_patch(self, &configFile, sizeof configFile), savedErrno, std::string(__func__) + ": unwritable object");
    ParseStatements(configFile);
}

}
