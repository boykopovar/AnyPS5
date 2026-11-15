#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static_assert(sizeof(GuestOption) == 32);
static_assert(offsetof(GuestOption, hasArgument) == 8);
static_assert(offsetof(GuestOption, flag) == 16);
static_assert(offsetof(GuestOption, value) == 24);

extern "C" {
extern const char* __progname_nid_postfix;
extern FileStream* __stderrp_nid_postfix;

int opterr_nid_postfix = 1;
int optind_nid_postfix = 1;
int optopt_nid_postfix = 0;
int optreset_nid_postfix = 0;
char* optarg_nid_postfix = nullptr;
}

namespace {

constexpr int NoArgument = 0;
constexpr int RequiredArgument = 1;
constexpr int OptionalArgument = 2;

constexpr int FlagPermute = 1;
constexpr int FlagAllArguments = 2;
constexpr int FlagLongOnly = 4;

constexpr int BadCharacter = '?';
constexpr int InOrder = 1;

char g_emptyMessage[1] {};
char* g_shortPlace = g_emptyMessage;
char* g_longPlace = g_emptyMessage;
int g_nonOptionStart = -1;
int g_nonOptionEnd = -1;
int g_posixlyCorrect = -1;

template <typename T>
bool GuestCopyInPreservingErrno(T& out, const void* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&out, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
bool GuestCopyOutPreservingErrno(void* destination, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

bool GuestWritablePreservingErrno(void* destination, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    return ok;
}

std::string CopyGuestText(const char* text, const char* caller) {
    const int savedErrno = errno;
    std::string copied;
    constexpr std::size_t maxChunk = 64;
    std::size_t offset = 0;
    for (;;) {
        char chunk[maxChunk]{};
        std::size_t chunkSize = maxChunk;
        bool readOk = false;
        while (chunkSize > 0) {
            if (GuestCopyIn_nid_no_patch(chunk, text + offset, chunkSize)) {
                readOk = true;
                break;
            }
            chunkSize /= 2;
        }
        if (!readOk) {
            errno = savedErrno;
            NotImplemented_nid_no_patch(caller);
        }
        for (std::size_t i = 0; i < chunkSize; ++i) {
            if (chunk[i] == '\0') {
                errno = savedErrno;
                return copied;
            }
            copied.push_back(chunk[i]);
        }
        offset += chunkSize;
    }
}

char* ReadArgumentSlot(char* const* arguments, int index, const char* caller) {
    char* entry = nullptr;
    if (!GuestCopyInPreservingErrno(entry, arguments + index)) NotImplemented_nid_no_patch(caller);
    return entry;
}

void WriteArgumentSlot(char* const* arguments, int index, char* value, const char* caller) {
    if (!GuestCopyOutPreservingErrno(const_cast<char**>(arguments) + index, value)) {
        NotImplemented_nid_no_patch(caller);
    }
}

void Warn(std::string_view message) {
    const std::string name = __progname_nid_postfix ? CopyGuestText(__progname_nid_postfix, "getopt") : "(null)";
    std::string line(name);
    line.append(": ").append(message).append("\n");
    FileStream* const stream = __stderrp_nid_postfix;
    std::fwrite(line.data(), 1, line.size(), GetNativeStream(stream));
    stream->SyncStatus();
}

std::string Character(int character) {
    return std::string(1, static_cast<char>(character));
}

char* Argument(char* const* arguments, int index, const char* function, const char* caller) {
    if (!arguments) throw std::invalid_argument(std::string(function) + ": null argument vector");
    if (index < 0) throw std::invalid_argument(std::string(function) + ": negative optind " + std::to_string(index));
    char* const argument = ReadArgumentSlot(arguments, index, caller);
    if (!argument) throw std::invalid_argument(std::string(function) + ": null argument " + std::to_string(index));
    return argument;
}

int OptionCharacter(char ch) {
    return static_cast<signed char>(ch);
}

bool PrintError(const char* options) {
    return opterr_nid_postfix && *options != ':';
}

int BadArgument(const char* options) {
    return *options == ':' ? ':' : '?';
}

int Gcd(int a, int b) {
    int c = a % b;
    while (c != 0) {
        a = b;
        b = c;
        c = a % b;
    }
    return b;
}

void PermuteArguments(int nonOptionStart, int nonOptionEnd, int optionEnd, char* const* arguments, const char* caller) {
    const int nonOptions = nonOptionEnd - nonOptionStart;
    const int options = optionEnd - nonOptionEnd;
    if (nonOptions <= 0 || options <= 0) throw std::logic_error("getopt_long: optind moved inside a permutation");
    const int cycles = Gcd(nonOptions, options);
    const int cycleLength = (optionEnd - nonOptionStart) / cycles;
    for (int i = 0; i < cycles; ++i) {
        const int cycleStart = nonOptionEnd + i;
        int position = cycleStart;
        char* cycleValue = ReadArgumentSlot(arguments, cycleStart, caller);
        for (int j = 0; j < cycleLength; ++j) {
            if (position >= nonOptionEnd) position -= nonOptions;
            else position += options;
            char* const swap = (position == cycleStart) ? cycleValue : ReadArgumentSlot(arguments, position, caller);
            WriteArgumentSlot(arguments, position, cycleValue, caller);
            cycleValue = swap;
        }
    }
}

struct CopiedOption {
    std::string name;
    GuestOption raw{};
};

int ParseLongOptions(int count, char* const* arguments, const char* options, const GuestOption* longOptions,
                     int* index, const char* dash, bool shortToo, int flags,
                     const std::string& currentText, char* rawCurrent, std::size_t offset, const char* caller) {
    const char* const current = currentText.c_str() + offset;
    std::vector<CopiedOption> copiedOptions;
    for (int i = 0;; ++i) {
        GuestOption raw{};
        if (!GuestCopyInPreservingErrno(raw, longOptions + i)) NotImplemented_nid_no_patch(caller);
        if (!raw.name) break;
        copiedOptions.push_back({CopyGuestText(raw.name, caller), raw});
    }

    int match = -1;
    bool exactMatch = false;
    bool secondPartialMatch = false;

    const char* hasEqual = std::strchr(current, '=');
    std::size_t length;
    if (hasEqual) {
        length = static_cast<std::size_t>(hasEqual - current);
        ++hasEqual;
    } else {
        length = std::strlen(current);
    }

    for (int i = 0; i < static_cast<int>(copiedOptions.size()); ++i) {
        const auto& candidate = copiedOptions[i];
        if (std::strncmp(current, candidate.name.c_str(), length) != 0) continue;
        if (candidate.name.size() == length) {
            match = i;
            exactMatch = true;
            break;
        }
        if (shortToo && length == 1) continue;
        if (match == -1) match = i;
        else if ((flags & FlagLongOnly) || candidate.raw.hasArgument != copiedOptions[match].raw.hasArgument ||
                 candidate.raw.flag != copiedOptions[match].raw.flag ||
                 candidate.raw.value != copiedOptions[match].raw.value)
            secondPartialMatch = true;
    }
    ++optind_nid_postfix;
    const std::string_view name(current, length);
    if (!exactMatch && secondPartialMatch) {
        if (PrintError(options)) Warn(std::string("option `") + dash + std::string(name) + "' is ambiguous");
        optopt_nid_postfix = 0;
        return BadCharacter;
    }
    if (match == -1) {
        if (shortToo) {
            --optind_nid_postfix;
            return -1;
        }
        if (PrintError(options)) Warn(std::string("unrecognized option `") + dash + current + "'");
        optopt_nid_postfix = 0;
        return BadCharacter;
    }
    const GuestOption& option = copiedOptions[match].raw;
    if (option.hasArgument == NoArgument && hasEqual) {
        if (PrintError(options)) Warn(std::string("option `") + dash + std::string(name) + "' doesn't allow an argument");
        optopt_nid_postfix = option.flag ? 0 : option.value;
        return BadCharacter;
    }
    if (index && !GuestWritablePreservingErrno(index, sizeof(*index))) NotImplemented_nid_no_patch(caller);
    if (option.flag && !GuestWritablePreservingErrno(option.flag, sizeof(*option.flag))) NotImplemented_nid_no_patch(caller);
    if (option.hasArgument == RequiredArgument || option.hasArgument == OptionalArgument) {
        if (hasEqual) optarg_nid_postfix = rawCurrent + offset + static_cast<std::size_t>(hasEqual - current);
        else if (option.hasArgument == RequiredArgument && optind_nid_postfix < count)
            optarg_nid_postfix = ReadArgumentSlot(arguments, optind_nid_postfix++, caller);
    }
    if (option.hasArgument == RequiredArgument && !optarg_nid_postfix) {
        if (PrintError(options)) Warn(std::string("option `") + dash + current + "' requires an argument");
        optopt_nid_postfix = option.flag ? 0 : option.value;
        return BadArgument(options);
    }
    if (index && !GuestCopyOutPreservingErrno(index, match)) NotImplemented_nid_no_patch(caller);
    if (option.flag) {
        if (!GuestCopyOutPreservingErrno(option.flag, option.value)) NotImplemented_nid_no_patch(caller);
        return 0;
    }
    return option.value;
}

int GetoptInternal(int count, char* const* arguments, const char* rawOptions, const GuestOption* longOptions, int* index, int flags, const char* caller) {
    if (!rawOptions) return -1;
    const std::string optionsStorage = CopyGuestText(rawOptions, caller);
    const char* options = optionsStorage.c_str();

    if (optind_nid_postfix == 0) optind_nid_postfix = optreset_nid_postfix = 1;

    if (g_posixlyCorrect == -1 || optreset_nid_postfix)
        g_posixlyCorrect = getenv_nid_postfix("POSIXLY_CORRECT") != nullptr;
    if (*options == '-') flags |= FlagAllArguments;
    else if (g_posixlyCorrect || *options == '+') flags &= ~FlagPermute;
    if (*options == '+' || *options == '-') ++options;

    optarg_nid_postfix = nullptr;
    if (optreset_nid_postfix) g_nonOptionStart = g_nonOptionEnd = -1;

    char* rawPlace = g_longPlace;
    std::string placeText = optreset_nid_postfix ? std::string() : CopyGuestText(rawPlace, caller);
    std::size_t placeOffset = 0;

    while (optreset_nid_postfix || placeOffset >= placeText.size()) {
        optreset_nid_postfix = 0;
        if (optind_nid_postfix >= count) {
            g_longPlace = g_emptyMessage;
            if (g_nonOptionEnd != -1) {
                PermuteArguments(g_nonOptionStart, g_nonOptionEnd, optind_nid_postfix, arguments, caller);
                optind_nid_postfix -= g_nonOptionEnd - g_nonOptionStart;
            } else if (g_nonOptionStart != -1) {
                optind_nid_postfix = g_nonOptionStart;
            }
            g_nonOptionStart = g_nonOptionEnd = -1;
            return -1;
        }
        rawPlace = Argument(arguments, optind_nid_postfix, "getopt_long", caller);
        placeText = CopyGuestText(rawPlace, caller);
        placeOffset = 0;
        if (placeText.empty() || placeText[0] != '-' || placeText.size() == 1) {
            g_longPlace = g_emptyMessage;
            placeText.clear();
            placeOffset = 0;
            if (flags & FlagAllArguments) {
                optarg_nid_postfix = ReadArgumentSlot(arguments, optind_nid_postfix++, caller);
                return InOrder;
            }
            if (!(flags & FlagPermute)) return -1;
            if (g_nonOptionStart == -1) {
                g_nonOptionStart = optind_nid_postfix;
            } else if (g_nonOptionEnd != -1) {
                PermuteArguments(g_nonOptionStart, g_nonOptionEnd, optind_nid_postfix, arguments, caller);
                g_nonOptionStart = optind_nid_postfix - (g_nonOptionEnd - g_nonOptionStart);
                g_nonOptionEnd = -1;
            }
            ++optind_nid_postfix;
            continue;
        }
        if (g_nonOptionStart != -1 && g_nonOptionEnd == -1) g_nonOptionEnd = optind_nid_postfix;

        if (placeText.size() > 1) {
            ++placeOffset;
            if (placeText[placeOffset] == '-' && placeOffset + 1 == placeText.size()) {
                ++optind_nid_postfix;
                g_longPlace = g_emptyMessage;
                if (g_nonOptionEnd != -1) {
                    PermuteArguments(g_nonOptionStart, g_nonOptionEnd, optind_nid_postfix, arguments, caller);
                    optind_nid_postfix -= g_nonOptionEnd - g_nonOptionStart;
                }
                g_nonOptionStart = g_nonOptionEnd = -1;
                return -1;
            }
        }
        break;
    }

    if (longOptions && (rawPlace + placeOffset) != ReadArgumentSlot(arguments, optind_nid_postfix, caller) &&
        (placeText[placeOffset] == '-' || (flags & FlagLongOnly))) {
        bool shortToo = false;
        const char* dash = "-";
        if (placeText[placeOffset] == '-') {
            ++placeOffset;
            dash = "--";
        } else if (placeText[placeOffset] != ':' && std::strchr(options, placeText[placeOffset])) {
            shortToo = true;
        }
        const int result = ParseLongOptions(count, arguments, options, longOptions, index, dash, shortToo, flags,
                                            placeText, rawPlace, placeOffset, caller);
        if (result != -1) {
            g_longPlace = g_emptyMessage;
            return result;
        }
    }

    const int character = OptionCharacter(placeText[placeOffset++]);
    const char* found = nullptr;
    if (character == ':' || (character == '-' && placeOffset < placeText.size()) ||
        (found = std::strchr(options, character)) == nullptr) {
        if (character == '-' && placeOffset >= placeText.size()) {
            g_longPlace = rawPlace + placeOffset;
            return -1;
        }
        if (placeOffset >= placeText.size()) ++optind_nid_postfix;
        g_longPlace = rawPlace + placeOffset;
        if (PrintError(options))
            Warn((g_posixlyCorrect ? "illegal option -- " : "invalid option -- ") + Character(character));
        optopt_nid_postfix = character;
        return BadCharacter;
    }
    if (longOptions && character == 'W' && found[1] == ';') {
        if (placeOffset >= placeText.size()) {
            if (++optind_nid_postfix >= count) {
                g_longPlace = g_emptyMessage;
                if (PrintError(options)) Warn("option requires an argument -- " + Character(character));
                optopt_nid_postfix = character;
                return BadArgument(options);
            }
            rawPlace = Argument(arguments, optind_nid_postfix, "getopt_long", caller);
            placeText = CopyGuestText(rawPlace, caller);
            placeOffset = 0;
        }
        const int result = ParseLongOptions(count, arguments, options, longOptions, index, "-W ", false, flags,
                                            placeText, rawPlace, placeOffset, caller);
        g_longPlace = g_emptyMessage;
        return result;
    }
    if (*++found != ':') {
        if (placeOffset >= placeText.size()) ++optind_nid_postfix;
        g_longPlace = rawPlace + placeOffset;
    } else {
        optarg_nid_postfix = nullptr;
        if (placeOffset < placeText.size()) {
            optarg_nid_postfix = rawPlace + placeOffset;
        } else if (found[1] != ':') {
            if (++optind_nid_postfix >= count) {
                g_longPlace = g_emptyMessage;
                if (PrintError(options)) Warn("option requires an argument -- " + Character(character));
                optopt_nid_postfix = character;
                return BadArgument(options);
            }
            optarg_nid_postfix = ReadArgumentSlot(arguments, optind_nid_postfix, caller);
        }
        g_longPlace = g_emptyMessage;
        ++optind_nid_postfix;
    }
    return character;
}

}

extern "C" {

int APS5_VABI getopt_nid_postfix(int count, char* const* arguments, const char* rawOptions) {
    if (!rawOptions) throw std::invalid_argument("getopt: null option string");
    const std::string optionsStorage = CopyGuestText(rawOptions, __func__);
    const char* options = optionsStorage.c_str();

    char* rawPlace = g_shortPlace;
    std::string placeText = optreset_nid_postfix ? std::string() : CopyGuestText(rawPlace, __func__);
    std::size_t placeOffset = 0;

    if (optreset_nid_postfix || placeOffset >= placeText.size()) {
        optreset_nid_postfix = 0;
        if (optind_nid_postfix >= count) {
            g_shortPlace = g_emptyMessage;
            return -1;
        }
        rawPlace = Argument(arguments, optind_nid_postfix, "getopt", __func__);
        placeText = CopyGuestText(rawPlace, __func__);
        placeOffset = 0;
        if (placeText.empty() || placeText[placeOffset++] != '-') {
            g_shortPlace = g_emptyMessage;
            return -1;
        }
        optopt_nid_postfix = placeOffset < placeText.size() ? OptionCharacter(placeText[placeOffset++]) : 0;
        if (optopt_nid_postfix == '-' && placeOffset >= placeText.size()) {
            ++optind_nid_postfix;
            g_shortPlace = g_emptyMessage;
            return -1;
        }
        if (optopt_nid_postfix == 0) {
            g_shortPlace = g_emptyMessage;
            if (!std::strchr(options, '-')) return -1;
            optopt_nid_postfix = '-';
        }
    } else {
        optopt_nid_postfix = OptionCharacter(placeText[placeOffset++]);
    }

    const char* found = nullptr;
    if (optopt_nid_postfix == ':' || (found = std::strchr(options, optopt_nid_postfix)) == nullptr) {
        if (placeOffset >= placeText.size()) ++optind_nid_postfix;
        g_shortPlace = rawPlace + placeOffset;
        if (opterr_nid_postfix && *options != ':') Warn("illegal option -- " + Character(optopt_nid_postfix));
        return BadCharacter;
    }

    if (found[1] != ':') {
        optarg_nid_postfix = nullptr;
        if (placeOffset >= placeText.size()) ++optind_nid_postfix;
        g_shortPlace = rawPlace + placeOffset;
    } else {
        if (placeOffset < placeText.size()) {
            optarg_nid_postfix = rawPlace + placeOffset;
        } else if (found[2] == ':') {
            optarg_nid_postfix = nullptr;
        } else if (count > ++optind_nid_postfix) {
            optarg_nid_postfix = ReadArgumentSlot(arguments, optind_nid_postfix, __func__);
        } else {
            g_shortPlace = g_emptyMessage;
            if (*options == ':') return ':';
            if (opterr_nid_postfix) Warn("option requires an argument -- " + Character(optopt_nid_postfix));
            return BadCharacter;
        }
        g_shortPlace = g_emptyMessage;
        ++optind_nid_postfix;
    }
    return optopt_nid_postfix;
}

int APS5_VABI getopt_long_nid_postfix(int count, char* const* arguments, const char* options, const GuestOption* longOptions, int* index) {
    return GetoptInternal(count, arguments, options, longOptions, index, FlagPermute, __func__);
}

int APS5_VABI getopt_long_only_nid_postfix(int count, char* const* arguments, const char* options, const GuestOption* longOptions, int* index) {
    return GetoptInternal(count, arguments, options, longOptions, index, FlagPermute | FlagLongOnly, __func__);
}

}
