#include "prx/libc/include/general/VabiMacros.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <link.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Function = std::uint64_t(APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

constexpr unsigned char Sentinel = 0xAB;
constexpr std::size_t DescriptorBytes = 0x80;
constexpr std::size_t CursorUp = 0x10;

struct Argument {
    std::string kind;
    std::vector<unsigned char> buffer;
    std::vector<unsigned char> descriptor;
    int relativeTo = -1;
};

using Arguments = std::vector<Argument>;

std::vector<std::string> Split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

bool HasFlag(const std::string& expect, const std::string& flag) {
    for (const auto& part : Split(expect, ','))
        if (part == flag) return true;
    return false;
}

std::uint64_t Integer(const std::string& text) {
    return text.find('-') != std::string::npos ? static_cast<std::uint64_t>(std::stoll(text, nullptr, 0)) : std::stoull(text, nullptr, 0);
}

std::vector<unsigned char> FromHex(const std::string& text) {
    std::vector<unsigned char> bytes;
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) bytes.push_back(static_cast<unsigned char>(std::stoul(text.substr(i, 2), nullptr, 16)));
    return bytes;
}

std::string ToHex(const unsigned char* data, std::size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

std::string Unescape(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == '\\') {
            out += '\\';
            ++i;
        } else if (text[i] == '\\' && i + 3 < text.size() && text[i + 1] == 'x') {
            out += static_cast<char>(std::stoul(text.substr(i + 2, 2), nullptr, 16));
            i += 3;
        } else {
            out += text[i];
        }
    }
    return out;
}

void* OpenModule(const std::string& name) {
    static std::map<std::string, void*> modules;
    auto found = modules.find(name);
    if (found != modules.end()) return found->second;
#if defined(_WIN32)
    void* handle = reinterpret_cast<void*>(LoadLibraryA(name.c_str()));
#else
    void* handle = dlopen(name.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
    modules[name] = handle;
    return handle;
}

void* Symbol(void* module, const std::string& name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name.c_str()));
#else
    void* address = dlsym(module, name.c_str());
    link_map* map = nullptr;
    Dl_info info{};
    if (!address || dlinfo(module, RTLD_DI_LINKMAP, &map) != 0 || !map || !dladdr(address, &info)) return address;
    const bool own = info.dli_fbase == reinterpret_cast<void*>(map->l_addr) || (info.dli_fname && map->l_name && std::strcmp(info.dli_fname, map->l_name) == 0);
    return own ? address : nullptr;
#endif
}

void* Resolve(const std::string& library, const std::string& function) {
    std::vector<std::string> modules;
    if (library != "-") modules.push_back(library.substr(0, library.find('.')) + ".prx");
    for (const char* fallback : {"libc.prx", "libSceLibcInternal.prx", "libkernel.prx", "libkernel_sys.prx", "libScePosix.prx"}) modules.push_back(fallback);
    for (const auto& module : modules) {
        void* handle = OpenModule(module);
        if (!handle) continue;
        for (const std::string& name : {function, function + "_nid_postfix", function + "_nid_disambig1_nid_postfix"})
            if (void* address = Symbol(handle, name)) return address;
    }
    return nullptr;
}

std::uint64_t Address(const Argument& argument) {
    return reinterpret_cast<std::uint64_t>(argument.kind == "dcb" ? argument.descriptor.data() : argument.buffer.data());
}

std::string Relative(std::uint64_t value, const Argument* base) {
    if (value == 0) return "null";
    if (base) {
        const auto start = reinterpret_cast<std::uint64_t>(base->buffer.data());
        if (value >= start && value <= start + base->buffer.size()) return "ptr+" + std::to_string(value - start);
    }
    return "ptr:foreign";
}

std::string Reported(const Arguments& arguments, const Argument& argument) {
    if (argument.kind == "outptr") {
        bool untouched = true;
        for (unsigned char c : argument.buffer)
            if (c != Sentinel) untouched = false;
        std::uint64_t pointer;
        std::memcpy(&pointer, argument.buffer.data(), sizeof(pointer));
        const Argument* base = argument.relativeTo >= 0 && argument.relativeTo < static_cast<int>(arguments.size()) ? &arguments[argument.relativeTo] : nullptr;
        return untouched ? "untouched" : Relative(pointer, base);
    }
    if (argument.kind == "dcb") {
        std::uint64_t cursor;
        std::memcpy(&cursor, argument.descriptor.data() + CursorUp, sizeof(cursor));
        const auto start = reinterpret_cast<std::uint64_t>(argument.buffer.data());
        if (cursor < start || cursor > start + argument.buffer.size() || ((cursor - start) & 3)) return "bad";
        return std::to_string((cursor - start) / 4) + ":" + ToHex(argument.buffer.data(), cursor - start);
    }
    return ToHex(argument.buffer.data(), argument.buffer.size());
}

bool Build(const std::vector<std::string>& tokens, const std::map<std::string, std::unique_ptr<Arguments>>& earlier, Arguments& arguments, std::uint64_t (&registers)[6]) {
    arguments.resize(tokens.size());
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        Argument& argument = arguments[k];
        const auto colon = tokens[k].find(':');
        argument.kind = tokens[k].substr(0, colon);
        const std::string value = colon == std::string::npos ? "" : tokens[k].substr(colon + 1);
        if (argument.kind == "i") {
            registers[k] = Integer(value);
        } else if (argument.kind == "null") {
            registers[k] = 0;
        } else if (argument.kind == "in") {
            argument.buffer = FromHex(value);
        } else if (argument.kind == "p32" || argument.kind == "p64") {
            const std::uint64_t number = Integer(value);
            argument.buffer.resize(argument.kind == "p32" ? 4 : 8);
            std::memcpy(argument.buffer.data(), &number, argument.buffer.size());
            argument.kind = "in";
        } else if (argument.kind == "out") {
            argument.buffer.assign(std::stoul(value), Sentinel);
        } else if (argument.kind == "s") {
            const std::string text = Unescape(value);
            argument.buffer.assign(text.begin(), text.end());
            argument.buffer.push_back(0);
        } else if (argument.kind == "outptr") {
            argument.relativeTo = std::stoi(value);
            argument.buffer.assign(8, Sentinel);
        } else if (argument.kind == "dcb") {
            argument.buffer.assign(std::stoul(value) * 4, Sentinel);
            argument.descriptor.assign(DescriptorBytes, 0);
            const auto start = reinterpret_cast<std::uint64_t>(argument.buffer.data());
            const auto end = start + argument.buffer.size();
            std::memcpy(argument.descriptor.data() + 0x00, &start, 8);
            std::memcpy(argument.descriptor.data() + 0x08, &end, 8);
            std::memcpy(argument.descriptor.data() + 0x10, &start, 8);
            std::memcpy(argument.descriptor.data() + 0x18, &end, 8);
        } else if (argument.kind == "ref" || argument.kind == "use") {
            const auto parts = Split(value, '.');
            const auto found = earlier.find(parts[0]);
            if (found == earlier.end() || parts.size() < 2 || std::stoul(parts[1]) >= found->second->size()) return false;
            const Argument& source = (*found->second)[std::stoul(parts[1])];
            if (argument.kind == "ref") {
                registers[k] = Address(source);
            } else {
                if (parts.size() < 4) return false;
                const std::size_t offset = std::stoul(parts[2]), size = std::stoul(parts[3]);
                if (size > 8 || offset + size > source.buffer.size()) return false;
                std::uint64_t number = 0;
                std::memcpy(&number, source.buffer.data() + offset, size);
                registers[k] = number;
            }
        } else {
            return false;
        }
    }
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        const auto& kind = arguments[k].kind;
        if (kind == "in" || kind == "out" || kind == "s" || kind == "outptr" || kind == "dcb") registers[k] = Address(arguments[k]);
    }
    return true;
}

}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s MODULE_DIRECTORY [--skip ID,...] GOLDEN.tsv...\n", argv[0]);
        return 2;
    }
#if defined(_WIN32)
    SetDllDirectoryA(argv[1]);
#endif
    std::set<std::string> skip;
    int first = 2;
    if (argc > 3 && std::string(argv[2]) == "--skip") {
        for (const auto& id : Split(argv[3], ',')) skip.insert(id);
        first = 4;
    }
    std::map<std::string, std::unique_ptr<Arguments>> ran;
    int compared = 0, mismatched = 0, skipped = 0;
    for (int f = first; f < argc; ++f) {
        std::ifstream in(argv[f]);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            const auto columns = Split(line, '\t');
            if (columns.size() < 8) continue;
            const std::string& id = columns[0];
            const std::string& library = columns[1];
            const std::string& function = columns[2];
            const std::string& spec = columns[3];
            const std::string& expect = columns[4];
            if (skip.count(id) || skip.count(function) || columns[5] != "ok" || HasFlag(expect, "none")) {
                ++skipped;
                continue;
            }
            void* target = Resolve(library, function);
            if (!target) {
                std::printf("MISSING\t%s\t%s\n", id.c_str(), function.c_str());
                ++skipped;
                continue;
            }
            auto arguments = std::make_unique<Arguments>();
            std::uint64_t registers[6] {};
            const auto tokens = spec == "-" ? std::vector<std::string>{} : Split(spec, ',');
            if (tokens.size() > 6 || !Build(tokens, ran, *arguments, registers)) {
                std::printf("BADSPEC\t%s\t%s\n", id.c_str(), spec.c_str());
                ++skipped;
                continue;
            }
            std::fprintf(stderr, "RUN\t%s\n", id.c_str());
            std::fflush(stderr);
            std::uint64_t result = 0;
            std::string thrown;
            try {
                result = reinterpret_cast<Function>(target)(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5], 0, 0);
            } catch (const std::exception& error) {
                thrown = error.what();
            } catch (...) {
                thrown = "unknown exception";
            }
            ++compared;
            std::ostringstream difference;
            if (!thrown.empty()) {
                difference << " threw '" << thrown << "'";
            } else {
                int returnOffset = -1;
                for (const auto& flag : Split(expect, ','))
                    if (flag.rfind("retoff:", 0) == 0) returnOffset = std::stoi(flag.substr(7));
                if (returnOffset >= 0) {
                    const Argument* base = returnOffset < static_cast<int>(arguments->size()) ? &(*arguments)[returnOffset] : nullptr;
                    const auto got = Relative(result, base);
                    if (got != columns[7]) difference << " retn console=" << columns[7] << " anyps5=" << got;
                } else {
                    const std::uint64_t mask = HasFlag(expect, "r64") ? ~0ull : 0xFFFFFFFFull;
                    const std::uint64_t console = std::stoull(columns[6], nullptr, 16) & mask;
                    if ((result & mask) != console) difference << " ret console=0x" << std::hex << console << " anyps5=0x" << (result & mask) << std::dec;
                }
                if (!HasFlag(expect, "ret") && !HasFlag(expect, "redact")) {
                    for (std::size_t c = 8; c < columns.size(); ++c) {
                        const auto& field = columns[c];
                        const auto equals = field.find('=');
                        if (field.size() < 3 || field[0] != 'a' || equals == std::string::npos) continue;
                        const std::string console = field.substr(equals + 1);
                        if (console == "volatile" || console.rfind("redacted", 0) == 0) continue;
                        const auto k = std::stoul(field.substr(1, equals - 1));
                        if (k >= arguments->size()) continue;
                        const auto got = Reported(*arguments, (*arguments)[k]);
                        if (got != console) difference << " a" << k << " console=" << console << " anyps5=" << got;
                    }
                }
            }
            if (!difference.str().empty()) {
                ++mismatched;
                std::printf("DIFF\t%s\t%s(%s)%s\n", id.c_str(), function.c_str(), spec.c_str(), difference.str().c_str());
            }
            ran[id] = std::move(arguments);
        }
    }
    std::printf("SUMMARY\tcompared %d\tmismatched %d\tskipped %d\n", compared, mismatched, skipped);
    std::fflush(stdout);
#if defined(_WIN32)
    TerminateProcess(GetCurrentProcess(), 0);
#endif
    std::_Exit(0);
}
