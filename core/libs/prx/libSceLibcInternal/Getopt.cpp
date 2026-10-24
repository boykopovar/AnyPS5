#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

struct GuestOption {
    const char* name;
    int hasArgument;
    int* flag;
    int value;
};
static_assert(sizeof(GuestOption) == 32);
static_assert(offsetof(GuestOption, hasArgument) == 8);
static_assert(offsetof(GuestOption, flag) == 16);
static_assert(offsetof(GuestOption, value) == 24);

extern "C" {
extern const char* __progname_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
char* APS5_VABI getenv_nid_postfix(const char* name);

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

void Warn(std::string_view message) {
    const char* const name = __progname_nid_postfix ? __progname_nid_postfix : "(null)";
    std::string line(name);
    line.append(": ").append(message).append("\n");
    FileStream* const stream = __stderrp_nid_postfix;
    std::fwrite(line.data(), 1, line.size(), GetNativeStream(stream));
    stream->SyncStatus();
}

std::string Character(int character) {
    return std::string(1, static_cast<char>(character));
}

char* Argument(char* const* arguments, int index, const char* function) {
    if (!arguments) throw std::invalid_argument(std::string(function) + ": null argument vector");
    if (index < 0) throw std::invalid_argument(std::string(function) + ": negative optind " + std::to_string(index));
    char* const argument = arguments[index];
    if (!argument) throw std::invalid_argument(std::string(function) + ": null argument " + std::to_string(index));
    return argument;
}

int OptionCharacter(const char* place) {
    return static_cast<signed char>(*place);
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

void PermuteArguments(int nonOptionStart, int nonOptionEnd, int optionEnd, char* const* arguments) {
    char** const vector = const_cast<char**>(arguments);
    const int nonOptions = nonOptionEnd - nonOptionStart;
    const int options = optionEnd - nonOptionEnd;
    if (nonOptions <= 0 || options <= 0) throw std::logic_error("getopt_long: optind moved inside a permutation");
    const int cycles = Gcd(nonOptions, options);
    const int cycleLength = (optionEnd - nonOptionStart) / cycles;
    for (int i = 0; i < cycles; ++i) {
        const int cycleStart = nonOptionEnd + i;
        int position = cycleStart;
        for (int j = 0; j < cycleLength; ++j) {
            if (position >= nonOptionEnd) position -= nonOptions;
            else position += options;
            char* const swap = vector[position];
            vector[position] = vector[cycleStart];
            vector[cycleStart] = swap;
        }
    }
}

int ParseLongOptions(char* const* arguments, const char* options, const GuestOption* longOptions, int* index, const char* dash, bool shortToo, int flags) {
    char* const current = g_longPlace;
    int match = -1;
    bool exactMatch = false;
    bool secondPartialMatch = false;

    ++optind_nid_postfix;

    char* hasEqual = std::strchr(current, '=');
    std::size_t length;
    if (hasEqual) {
        length = static_cast<std::size_t>(hasEqual - current);
        ++hasEqual;
    } else {
        length = std::strlen(current);
    }

    for (int i = 0; longOptions[i].name; ++i) {
        if (std::strncmp(current, longOptions[i].name, length) != 0) continue;
        if (std::strlen(longOptions[i].name) == length) {
            match = i;
            exactMatch = true;
            break;
        }
        if (shortToo && length == 1) continue;
        if (match == -1) match = i;
        else if ((flags & FlagLongOnly) || longOptions[i].hasArgument != longOptions[match].hasArgument ||
                 longOptions[i].flag != longOptions[match].flag ||
                 longOptions[i].value != longOptions[match].value)
            secondPartialMatch = true;
    }
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
    const GuestOption& option = longOptions[match];
    if (option.hasArgument == NoArgument && hasEqual) {
        if (PrintError(options)) Warn(std::string("option `") + dash + std::string(name) + "' doesn't allow an argument");
        optopt_nid_postfix = option.flag ? 0 : option.value;
        return BadCharacter;
    }
    if (option.hasArgument == RequiredArgument || option.hasArgument == OptionalArgument) {
        if (hasEqual) optarg_nid_postfix = hasEqual;
        else if (option.hasArgument == RequiredArgument) optarg_nid_postfix = arguments[optind_nid_postfix++];
    }
    if (option.hasArgument == RequiredArgument && !optarg_nid_postfix) {
        if (PrintError(options)) Warn(std::string("option `") + dash + current + "' requires an argument");
        optopt_nid_postfix = option.flag ? 0 : option.value;
        --optind_nid_postfix;
        return BadArgument(options);
    }
    if (index) *index = match;
    if (option.flag) {
        *option.flag = option.value;
        return 0;
    }
    return option.value;
}

int GetoptInternal(int count, char* const* arguments, const char* options, const GuestOption* longOptions, int* index, int flags) {
    if (!options) return -1;

    if (optind_nid_postfix == 0) optind_nid_postfix = optreset_nid_postfix = 1;

    if (g_posixlyCorrect == -1 || optreset_nid_postfix)
        g_posixlyCorrect = getenv_nid_postfix("POSIXLY_CORRECT") != nullptr;
    if (*options == '-') flags |= FlagAllArguments;
    else if (g_posixlyCorrect || *options == '+') flags &= ~FlagPermute;
    if (*options == '+' || *options == '-') ++options;

    optarg_nid_postfix = nullptr;
    if (optreset_nid_postfix) g_nonOptionStart = g_nonOptionEnd = -1;

    while (optreset_nid_postfix || !*g_longPlace) {
        optreset_nid_postfix = 0;
        if (optind_nid_postfix >= count) {
            g_longPlace = g_emptyMessage;
            if (g_nonOptionEnd != -1) {
                PermuteArguments(g_nonOptionStart, g_nonOptionEnd, optind_nid_postfix, arguments);
                optind_nid_postfix -= g_nonOptionEnd - g_nonOptionStart;
            } else if (g_nonOptionStart != -1) {
                optind_nid_postfix = g_nonOptionStart;
            }
            g_nonOptionStart = g_nonOptionEnd = -1;
            return -1;
        }
        g_longPlace = Argument(arguments, optind_nid_postfix, "getopt_long");
        if (*g_longPlace != '-' || g_longPlace[1] == '\0') {
            g_longPlace = g_emptyMessage;
            if (flags & FlagAllArguments) {
                optarg_nid_postfix = arguments[optind_nid_postfix++];
                return InOrder;
            }
            if (!(flags & FlagPermute)) return -1;
            if (g_nonOptionStart == -1) {
                g_nonOptionStart = optind_nid_postfix;
            } else if (g_nonOptionEnd != -1) {
                PermuteArguments(g_nonOptionStart, g_nonOptionEnd, optind_nid_postfix, arguments);
                g_nonOptionStart = optind_nid_postfix - (g_nonOptionEnd - g_nonOptionStart);
                g_nonOptionEnd = -1;
            }
            ++optind_nid_postfix;
            continue;
        }
        if (g_nonOptionStart != -1 && g_nonOptionEnd == -1) g_nonOptionEnd = optind_nid_postfix;

        if (g_longPlace[1] != '\0' && *++g_longPlace == '-' && g_longPlace[1] == '\0') {
            ++optind_nid_postfix;
            g_longPlace = g_emptyMessage;
            if (g_nonOptionEnd != -1) {
                PermuteArguments(g_nonOptionStart, g_nonOptionEnd, optind_nid_postfix, arguments);
                optind_nid_postfix -= g_nonOptionEnd - g_nonOptionStart;
            }
            g_nonOptionStart = g_nonOptionEnd = -1;
            return -1;
        }
        break;
    }

    if (longOptions && g_longPlace != arguments[optind_nid_postfix] &&
        (*g_longPlace == '-' || (flags & FlagLongOnly))) {
        bool shortToo = false;
        const char* dash = "-";
        if (*g_longPlace == '-') {
            ++g_longPlace;
            dash = "--";
        } else if (*g_longPlace != ':' && std::strchr(options, *g_longPlace)) {
            shortToo = true;
        }
        const int result = ParseLongOptions(arguments, options, longOptions, index, dash, shortToo, flags);
        if (result != -1) {
            g_longPlace = g_emptyMessage;
            return result;
        }
    }

    const int character = OptionCharacter(g_longPlace++);
    const char* found = nullptr;
    if (character == ':' || (character == '-' && *g_longPlace != '\0') ||
        (found = std::strchr(options, character)) == nullptr) {
        if (character == '-' && *g_longPlace == '\0') return -1;
        if (!*g_longPlace) ++optind_nid_postfix;
        if (PrintError(options))
            Warn((g_posixlyCorrect ? "illegal option -- " : "invalid option -- ") + Character(character));
        optopt_nid_postfix = character;
        return BadCharacter;
    }
    if (longOptions && character == 'W' && found[1] == ';') {
        if (!*g_longPlace) {
            if (++optind_nid_postfix >= count) {
                g_longPlace = g_emptyMessage;
                if (PrintError(options)) Warn("option requires an argument -- " + Character(character));
                optopt_nid_postfix = character;
                return BadArgument(options);
            }
            g_longPlace = Argument(arguments, optind_nid_postfix, "getopt_long");
        }
        const int result = ParseLongOptions(arguments, options, longOptions, index, "-W ", false, flags);
        g_longPlace = g_emptyMessage;
        return result;
    }
    if (*++found != ':') {
        if (!*g_longPlace) ++optind_nid_postfix;
    } else {
        optarg_nid_postfix = nullptr;
        if (*g_longPlace) {
            optarg_nid_postfix = g_longPlace;
        } else if (found[1] != ':') {
            if (++optind_nid_postfix >= count) {
                g_longPlace = g_emptyMessage;
                if (PrintError(options)) Warn("option requires an argument -- " + Character(character));
                optopt_nid_postfix = character;
                return BadArgument(options);
            }
            optarg_nid_postfix = arguments[optind_nid_postfix];
        }
        g_longPlace = g_emptyMessage;
        ++optind_nid_postfix;
    }
    return character;
}

}

extern "C" {

int APS5_VABI getopt_nid_postfix(int count, char* const* arguments, const char* options) {
    if (!options) throw std::invalid_argument("getopt: null option string");
    if (optreset_nid_postfix || *g_shortPlace == '\0') {
        optreset_nid_postfix = 0;
        if (optind_nid_postfix >= count) {
            g_shortPlace = g_emptyMessage;
            return -1;
        }
        g_shortPlace = Argument(arguments, optind_nid_postfix, "getopt");
        if (*g_shortPlace++ != '-') {
            g_shortPlace = g_emptyMessage;
            return -1;
        }
        optopt_nid_postfix = OptionCharacter(g_shortPlace++);
        if (optopt_nid_postfix == '-' && *g_shortPlace == '\0') {
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
        optopt_nid_postfix = OptionCharacter(g_shortPlace++);
    }

    const char* found = nullptr;
    if (optopt_nid_postfix == ':' || (found = std::strchr(options, optopt_nid_postfix)) == nullptr) {
        if (*g_shortPlace == '\0') ++optind_nid_postfix;
        if (opterr_nid_postfix && *options != ':') Warn("illegal option -- " + Character(optopt_nid_postfix));
        return BadCharacter;
    }

    if (found[1] != ':') {
        optarg_nid_postfix = nullptr;
        if (*g_shortPlace == '\0') ++optind_nid_postfix;
    } else {
        if (*g_shortPlace) {
            optarg_nid_postfix = g_shortPlace;
        } else if (found[2] == ':') {
            optarg_nid_postfix = nullptr;
        } else if (count > ++optind_nid_postfix) {
            optarg_nid_postfix = arguments[optind_nid_postfix];
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
    return GetoptInternal(count, arguments, options, longOptions, index, FlagPermute);
}

int APS5_VABI getopt_long_only_nid_postfix(int count, char* const* arguments, const char* options, const GuestOption* longOptions, int* index) {
    return GetoptInternal(count, arguments, options, longOptions, index, FlagPermute | FlagLongOnly);
}

}
