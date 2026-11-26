/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/conf/conf_mod.c (conf_mod.c,v 1.27),
 * conf/conf_def.c (conf_def.c,v 1.32) and bio/bss_file.c (bss_file.c,v 1.32) */
/* $OpenBSD: conf_mod.c,v 1.27 2017/01/29 17:49:22 beck Exp $ */
/* Written by Stephen Henson (steve@openssl.org) for the OpenSSL
 * project 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* $OpenBSD: conf_def.c,v 1.32 2017/01/29 17:49:22 beck Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* $OpenBSD: bss_file.c,v 1.32 2017/01/29 17:49:22 beck Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int SystemLibrary = 2;
constexpr int ConfLibrary = 14;
constexpr int Asn1Library = 13;
constexpr int BioLibrary = 32;
constexpr int DsoLibrary = 37;
constexpr int EngineLibrary = 38;
constexpr int UnnamedFunction = 0xfff;
constexpr int SystemLibraryFailure = 2;
constexpr int MallocFailure = 65;
constexpr int ConfNoSuchFile = 114;
constexpr int BioNoSuchFile = 128;
constexpr int GuestNoSuchFile = 2;
constexpr std::uint64_t IgnoreErrors = 0x1;
constexpr std::uint64_t Silent = 0x4;
constexpr std::uint64_t NoDso = 0x8;
constexpr std::uint64_t IgnoreMissingFile = 0x10;
constexpr std::uint64_t DefaultSection = 0x20;
constexpr std::size_t ConfSize = 32;
constexpr const char* CertificateArea = "/etc/ssl";
constexpr int ConfMissingCloseSquareBracket = 100;
constexpr int ConfMissingEqualSign = 101;
constexpr int ConfNoCloseBrace = 102;
constexpr int ConfVariableHasNoValue = 104;
constexpr int ConfModuleInitializationError = 109;
constexpr int ConfErrorLoadingDso = 110;
constexpr int ConfUnknownModule = 113;
constexpr int DsoErrorLoadingDso = 108;
constexpr int Asn1AddingObject = 171;
constexpr int Asn1ErrorLoadingSection = 172;
constexpr int EngineConfigurationError = 102;
constexpr int EngineEnginesSectionError = 148;
constexpr int EngineEngineSectionError = 149;
constexpr int EngineNoSuchEngine = 116;

int Reason(std::uint64_t code) {
    return static_cast<int>(code & 0xfff);
}

void ConfError(int reason) {
    LibreSslPutError_nid_no_patch(ConfLibrary, UnnamedFunction, reason);
}

bool IsSpace(char character) {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

bool IsAlphaNumeric(char character) {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool IsNameCharacter(char character) {
    return IsAlphaNumeric(character) || character == '.' || character == '-';
}

std::string StripComment(const std::string& line) {
    std::size_t index = 0;
    while (index < line.size() && IsSpace(line[index])) ++index;
    if (index < line.size() && line[index] == ';') return {};
    bool singleQuoted = false;
    bool doubleQuoted = false;
    for (; index < line.size(); ++index) {
        const char character = line[index];
        if (character == '\\') {
            if (index + 1 < line.size()) ++index;
            continue;
        }
        if (!doubleQuoted && character == '\'') {
            singleQuoted = !singleQuoted;
            continue;
        }
        if (!singleQuoted && character == '"') {
            doubleQuoted = !doubleQuoted;
            continue;
        }
        if (!singleQuoted && !doubleQuoted && character == '#') return line.substr(0, index);
    }
    return line;
}

std::size_t ParseName(const std::string& line, std::size_t index, std::string& name) {
    name.clear();
    while (index < line.size()) {
        const char character = line[index];
        if (character == '\\' && index + 1 < line.size()) {
            name.push_back(character);
            name.push_back(line[index + 1]);
            index += 2;
            continue;
        }
        if (!IsNameCharacter(character)) break;
        name.push_back(character);
        ++index;
    }
    return index;
}

struct ConfValue {
    std::string section;
    std::string name;
    std::string value;
};

struct ConfData {
    std::map<std::string, std::vector<ConfValue>, std::less<>> sections;
    std::string currentSection = "default";
};

const std::string* FindString(const ConfData& data, const char* section, const char* name) {
    if (!name) return nullptr;
    if (section) {
        const auto found = data.sections.find(section);
        if (found != data.sections.end()) {
            for (const auto& value : found->second) {
                if (value.name == name) return &value.value;
            }
        }
    }
    const auto fallback = data.sections.find("default");
    if (fallback != data.sections.end()) {
        for (const auto& value : fallback->second) {
            if (value.name == name) return &value.value;
        }
    }
    return nullptr;
}

std::optional<std::string> Expand(const std::string& source, const std::string& section, const ConfData& data) {
    std::string result;
    std::size_t index = 0;
    while (index < source.size()) {
        const char character = source[index];
        if (character == '\'' || character == '"') {
            const char quote = character;
            ++index;
            while (index < source.size() && source[index] != quote) {
                if (source[index] == '\\' && index + 1 < source.size()) ++index;
                result.push_back(source[index++]);
            }
            if (index < source.size() && source[index] == quote) ++index;
            continue;
        }
        if (character == '\\') {
            if (index + 1 >= source.size()) break;
            char value = source[++index];
            if (value == 'r') value = '\r';
            else if (value == 'n') value = '\n';
            else if (value == 'b') value = '\b';
            else if (value == 't') value = '\t';
            result.push_back(value);
            ++index;
            continue;
        }
        if (character == '$') {
            std::size_t next = index + 1;
            char close = 0;
            if (next < source.size() && (source[next] == '{' || source[next] == '(')) {
                close = source[next] == '{' ? '}' : ')';
                ++next;
            }
            std::string variableSection = section;
            const std::size_t nameBegin = next;
            while (next < source.size() && IsAlphaNumeric(source[next])) ++next;
            if (next + 1 < source.size() && source[next] == ':' && source[next + 1] == ':') {
                variableSection = source.substr(nameBegin, next - nameBegin);
                next += 2;
                const std::size_t secondBegin = next;
                while (next < source.size() && IsAlphaNumeric(source[next])) ++next;
                const std::string* replacement = FindString(data, variableSection.c_str(), source.substr(secondBegin, next - secondBegin).c_str());
                if (!replacement) {
                    if (close && (next >= source.size() || source[next] != close)) ConfError(ConfNoCloseBrace);
                    else ConfError(ConfVariableHasNoValue);
                    return std::nullopt;
                }
                if (close && (next >= source.size() || source[next] != close)) {
                    ConfError(ConfNoCloseBrace);
                    return std::nullopt;
                }
                if (close) ++next;
                result += *replacement;
                index = next;
                continue;
            }
            const std::string name = source.substr(nameBegin, next - nameBegin);
            if (close && (next >= source.size() || source[next] != close)) {
                ConfError(ConfNoCloseBrace);
                return std::nullopt;
            }
            if (close) ++next;
            const std::string* replacement = FindString(data, variableSection.c_str(), name.c_str());
            if (!replacement) {
                ConfError(ConfVariableHasNoValue);
                return std::nullopt;
            }
            result += *replacement;
            index = next;
            continue;
        }
        result.push_back(character);
        ++index;
    }
    return result;
}

bool ParseLine(const std::string& raw, ConfData& data) {
    const std::string line = StripComment(raw);
    std::size_t index = 0;
    while (index < line.size() && IsSpace(line[index])) ++index;
    if (index >= line.size()) return true;
    if (line[index] == '[') {
        ++index;
        while (index < line.size() && IsSpace(line[index])) ++index;
        const std::size_t begin = index;
        std::size_t end = index;
        for (;;) {
            std::string part;
            end = ParseName(line, index, part);
            index = end;
            while (index < line.size() && IsSpace(line[index])) ++index;
            if (index < line.size() && line[index] == ']') break;
            if (index >= line.size() || index == end || end == begin) {
                ConfError(ConfMissingCloseSquareBracket);
                return false;
            }
        }
        const auto section = Expand(line.substr(begin, end - begin), "", data);
        if (!section) return false;
        data.currentSection = *section;
        data.sections.try_emplace(*section);
        return true;
    }
    std::string name;
    index = ParseName(line, index, name);
    std::string section = data.currentSection;
    while (index < line.size() && IsSpace(line[index])) ++index;
    if (index + 1 < line.size() && line[index] == ':' && line[index + 1] == ':') {
        section = name;
        index += 2;
        while (index < line.size() && IsSpace(line[index])) ++index;
        index = ParseName(line, index, name);
        while (index < line.size() && IsSpace(line[index])) ++index;
    }
    if (index >= line.size() || line[index] != '=') {
        ConfError(ConfMissingEqualSign);
        return false;
    }
    ++index;
    while (index < line.size() && IsSpace(line[index])) ++index;
    std::size_t end = line.size();
    while (end > index && IsSpace(line[end - 1])) --end;
    const auto expanded = Expand(line.substr(index, end - index), section, data);
    if (!expanded) return false;
    auto& values = data.sections[section];
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it->name == name) {
            values.erase(it);
            break;
        }
    }
    values.push_back({section, name, *expanded});
    return true;
}

bool Parse(const std::string& content, ConfData& data, long* line) {
    std::vector<std::pair<std::string, long>> lines;
    std::string current;
    std::istringstream stream(content);
    std::string text;
    long inputLine = 0;
    while (std::getline(stream, text)) {
        ++inputLine;
        if (!text.empty() && text.back() == '\r') text.pop_back();
        const bool continued = !text.empty() && text.back() == '\\' && (text.size() < 2 || text[text.size() - 2] != '\\');
        if (continued) text.pop_back();
        current += text;
        if (continued) continue;
        lines.emplace_back(std::move(current), inputLine);
        current.clear();
    }
    if (!current.empty()) lines.emplace_back(std::move(current), inputLine);
    data.currentSection = "default";
    data.sections.try_emplace("default");
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (ParseLine(lines[index].first, data)) continue;
        if (line) *line = lines[index].second;
        return false;
    }
    return true;
}

struct ConfModule {
    std::string name;
    int (*init)(void*, const void*);
    void (*finish)(void*);
};

struct ConfImodule {
    const ConfModule* module;
    std::string name;
    std::string value;
    unsigned long flags;
};

std::mutex moduleMutex;
std::vector<std::unique_ptr<ConfModule>> modules;
std::vector<std::unique_ptr<ConfImodule>> initializedModules;

void AddModule(const char* name, int (*init)(void*, const void*), void (*finish)(void*)) {
    if (!name) return;
    auto module = std::make_unique<ConfModule>();
    module->name = name;
    module->init = init;
    module->finish = finish;
    modules.push_back(std::move(module));
}

const ConfModule* FindModule(const char* name) {
    if (!name) return nullptr;
    const std::string text(name);
    const auto dot = text.rfind('.');
    const std::string prefix = dot == std::string::npos ? text : text.substr(0, dot);
    for (const auto& module : modules) {
        if (module->name.size() >= prefix.size() && module->name.compare(0, prefix.size(), prefix) == 0) return module.get();
    }
    return nullptr;
}

bool DoCreate(const std::string& value, const std::string& name) {
    const auto comma = value.rfind(',');
    std::string oid;
    const char* longName = nullptr;
    std::string longNameStorage;
    if (comma == std::string::npos) {
        oid = value;
        longName = name.c_str();
    } else {
        oid = value.substr(comma + 1);
        const auto first = oid.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return false;
        oid = oid.substr(first);
        std::size_t last = comma;
        while (last > 0 && IsSpace(value[last - 1])) --last;
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos || begin >= last) return false;
        longNameStorage = value.substr(begin, last - begin);
        longName = longNameStorage.c_str();
    }
    return OBJ_create_nid_postfix(oid.c_str(), name.c_str(), longName) != 0;
}

int OidModuleInit(void* md, const void* cnf) {
    const auto* imodule = static_cast<const ConfImodule*>(md);
    void* section = NCONF_get_section_nid_no_patch(const_cast<void*>(cnf), imodule->value.c_str());
    if (!section) {
        LibreSslPutError_nid_no_patch(Asn1Library, UnnamedFunction, Asn1ErrorLoadingSection);
        return 0;
    }
    const int count = sk_CONF_VALUE_num_nid_no_patch(section);
    for (int index = 0; index < count; ++index) {
        const auto* value = static_cast<const ConfValue*>(sk_CONF_VALUE_value_nid_no_patch(section, index));
        if (!DoCreate(value->value, value->name)) {
            LibreSslPutError_nid_no_patch(Asn1Library, UnnamedFunction, Asn1AddingObject);
            return 0;
        }
    }
    return 1;
}

void OidModuleFinish(void* md) {
    static_cast<void>(md);
    OBJ_cleanup_nid_postfix();
}

std::string SkipDot(const std::string& name) {
    const auto dot = name.find('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

bool IntEngineConfigure(const std::string& name, const std::string& value, const void* cnf) {
    std::string engineName = SkipDot(name);
    void* commands = NCONF_get_section_nid_no_patch(const_cast<void*>(cnf), value.c_str());
    if (!commands) {
        LibreSslPutError_nid_no_patch(EngineLibrary, UnnamedFunction, EngineEngineSectionError);
        return false;
    }
    const int count = sk_CONF_VALUE_num_nid_no_patch(commands);
    bool soft = false;
    bool found = false;
    for (int index = 0; index < count; ++index) {
        const auto* command = static_cast<const ConfValue*>(sk_CONF_VALUE_value_nid_no_patch(commands, index));
        const std::string control = SkipDot(command->name);
        const std::string& argument = command->value;
        if (control == "engine_id") {
            engineName = argument;
            continue;
        }
        if (control == "soft_load") {
            soft = true;
            continue;
        }
        if (!found) found = ENGINE_by_id_nid_postfix(control == "dynamic_path" ? "dynamic" : engineName.c_str()) != nullptr;
        if (!found && soft) {
            ERR_clear_error_nid_postfix();
            return true;
        }
        if (!found) {
            LibreSslPutError_nid_no_patch(EngineLibrary, UnnamedFunction, EngineConfigurationError);
            return false;
        }
        static_cast<void>(argument);
    }
    return true;
}

int EngineModuleInit(void* md, const void* cnf) {
    const auto* imodule = static_cast<const ConfImodule*>(md);
    void* list = NCONF_get_section_nid_no_patch(const_cast<void*>(cnf), imodule->value.c_str());
    if (!list) {
        LibreSslPutError_nid_no_patch(EngineLibrary, UnnamedFunction, EngineEnginesSectionError);
        return 0;
    }
    const int count = sk_CONF_VALUE_num_nid_no_patch(list);
    for (int index = 0; index < count; ++index) {
        const auto* entry = static_cast<const ConfValue*>(sk_CONF_VALUE_value_nid_no_patch(list, index));
        if (!IntEngineConfigure(entry->name, entry->value, cnf)) return 0;
    }
    return 1;
}

void EngineModuleFinish(void* md) {
    static_cast<void>(md);
}

int ModuleInit(const ConfModule& module, const char* name, const char* value, const void* cnf) {
    auto imodule = std::make_unique<ConfImodule>();
    imodule->module = &module;
    imodule->name = name ? name : "";
    imodule->value = value ? value : "";
    imodule->flags = 0;
    if (module.init) {
        const int result = module.init(imodule.get(), cnf);
        if (result <= 0) {
            if (module.finish) module.finish(imodule.get());
            return -1;
        }
    }
    initializedModules.push_back(std::move(imodule));
    return 1;
}

int ModuleRun(const void* cnf, const char* name, const char* value, std::uint64_t flags) {
    const ConfModule* module = FindModule(name);
    if (!module && (flags & NoDso) == 0) {
        LibreSslPutError_nid_no_patch(DsoLibrary, UnnamedFunction, DsoErrorLoadingDso);
        ConfError(ConfErrorLoadingDso);
    }
    if (!module) {
        if ((flags & Silent) == 0) ConfError(ConfUnknownModule);
        return -1;
    }
    const int result = ModuleInit(*module, name, value, cnf);
    if (result <= 0 && (flags & Silent) == 0) ConfError(ConfModuleInitializationError);
    return result;
}

} // namespace

extern "C" {

void* APS5_VABI NCONF_new_nid_no_patch(const void* method) {
    static_cast<void>(method);
    return new ConfData();
}

int APS5_VABI NCONF_load_nid_no_patch(void* conf, const char* file, long* eline) {
    auto* data = static_cast<ConfData*>(conf);
    if (!data || !file) return 0;
    FileStream* stream = fopen_nid_postfix(file, "rb");
    if (!stream) {
        const int openError = errno;
        LibreSslPutError_nid_no_patch(SystemLibrary, UnnamedFunction, openError, __FILE__, __LINE__);
        LibreSslPutError_nid_no_patch(BioLibrary, UnnamedFunction, openError == GuestNoSuchFile ? BioNoSuchFile : SystemLibraryFailure,
                                       __FILE__, __LINE__);
        ConfError(Reason(LibreSslPeekLastError_nid_no_patch()) == BioNoSuchFile ? ConfNoSuchFile : SystemLibraryFailure);
        return 0;
    }
    std::string content;
    char buffer[512];
    while (char* received = fgets_nid_postfix(buffer, sizeof(buffer), stream)) content.append(received);
    fclose_nid_postfix(stream);
    const bool parsed = Parse(content, *data, eline);
    return parsed ? 1 : 0;
}

void APS5_VABI NCONF_free_nid_no_patch(void* conf) {
    delete static_cast<ConfData*>(conf);
}

const char* APS5_VABI NCONF_get_string_nid_no_patch(const void* conf, const char* section, const char* name) {
    const auto* data = static_cast<const ConfData*>(conf);
    if (!data) return nullptr;
    const std::string* found = FindString(*data, section, name);
    return found ? found->c_str() : nullptr;
}

void* APS5_VABI NCONF_get_section_nid_no_patch(void* conf, const char* section) {
    auto* data = static_cast<ConfData*>(conf);
    if (!data || !section) return nullptr;
    const auto found = data->sections.find(section);
    if (found == data->sections.end()) return nullptr;
    return &found->second;
}

int APS5_VABI sk_CONF_VALUE_num_nid_no_patch(const void* section) {
    const auto* values = static_cast<const std::vector<ConfValue>*>(section);
    return values ? static_cast<int>(values->size()) : 0;
}

void* APS5_VABI sk_CONF_VALUE_value_nid_no_patch(const void* section, int index) {
    const auto* values = static_cast<const std::vector<ConfValue>*>(section);
    if (!values || index < 0 || static_cast<std::size_t>(index) >= values->size()) return nullptr;
    return const_cast<ConfValue*>(&(*values)[static_cast<std::size_t>(index)]);
}

int APS5_VABI CONF_modules_load_file_nid_postfix(const char* filename, const char* appname, std::uint64_t flags) {
    void* conf = malloc_nid_postfix(ConfSize);
    if (!conf) {
        LibreSslPutError_nid_no_patch(ConfLibrary, UnnamedFunction, MallocFailure, __FILE__, __LINE__);
        return 0;
    }
    char* file = nullptr;
    const auto cleanup = [&] {
        try {
            free_nid_postfix(std::exchange(file, nullptr));
        } catch (...) {
            try { free_nid_postfix(std::exchange(conf, nullptr)); } catch (...) {}
            throw;
        }
        free_nid_postfix(std::exchange(conf, nullptr));
    };
    int result = 0;
    try {
        if (filename == nullptr && asprintf_nid_postfix(&file, "%s/openssl.cnf", CertificateArea) == -1) file = nullptr;
        const char* const path = filename != nullptr ? filename : file;
        if (path != nullptr) {
            void* nconf = NCONF_new_nid_no_patch(nullptr);
            if (!nconf) {
                LibreSslPutError_nid_no_patch(ConfLibrary, UnnamedFunction, MallocFailure, __FILE__, __LINE__);
            } else {
                long eline = 0;
                if (NCONF_load_nid_no_patch(nconf, path, &eline) <= 0) {
                    if ((flags & IgnoreMissingFile) != 0 && Reason(LibreSslPeekLastError_nid_no_patch()) == ConfNoSuchFile) {
                        ERR_clear_error_nid_postfix();
                        result = 1;
                    }
                } else {
                    result = CONF_modules_load_nid_no_patch(nconf, appname, flags);
                }
                NCONF_free_nid_no_patch(nconf);
            }
        }
    } catch (...) {
        try { cleanup(); } catch (...) {}
        throw;
    }
    cleanup();
    return result;
}

int APS5_VABI CONF_modules_load_nid_no_patch(const void* cnf, const char* appname, std::uint64_t flags) {
    if (!cnf) return 1;
    const char* found = NCONF_get_string_nid_no_patch(cnf, nullptr, appname);
    if (!appname || (!found && (flags & DefaultSection) != 0)) found = NCONF_get_string_nid_no_patch(cnf, nullptr, "openssl_conf");
    if (!found) {
        ERR_clear_error_nid_postfix();
        return 1;
    }
    const std::string sectionName(found);
    void* values = NCONF_get_section_nid_no_patch(const_cast<void*>(cnf), sectionName.c_str());
    if (!values) return 0;
    const int count = sk_CONF_VALUE_num_nid_no_patch(values);
    for (int index = 0; index < count; ++index) {
        const auto* value = static_cast<const ConfValue*>(sk_CONF_VALUE_value_nid_no_patch(values, index));
        const int result = ModuleRun(cnf, value->name.c_str(), value->value.c_str(), flags);
        if (result <= 0 && (flags & IgnoreErrors) == 0) return result;
    }
    return 1;
}

int APS5_VABI CONF_module_add_nid_no_patch(const char* name, int (*init)(void*, const void*), void (*finish)(void*)) {
    if (!name) return 0;
    std::lock_guard<std::mutex> lock(moduleMutex);
    AddModule(name, init, finish);
    return 1;
}

void APS5_VABI CONF_modules_free_nid_postfix(void) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    for (auto it = initializedModules.rbegin(); it != initializedModules.rend(); ++it) {
        if ((*it)->module && (*it)->module->finish) (*it)->module->finish(it->get());
    }
    initializedModules.clear();
    modules.clear();
}

void APS5_VABI OPENSSL_load_builtin_modules_nid_postfix(void) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    if (!FindModule("oid_section")) AddModule("oid_section", OidModuleInit, OidModuleFinish);
    if (!FindModule("engines")) AddModule("engines", EngineModuleInit, EngineModuleFinish);
}

}
