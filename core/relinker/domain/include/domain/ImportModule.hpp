#ifndef DOMAIN_IMPORTMODULE_HPP
#define DOMAIN_IMPORTMODULE_HPP

#include <domain/Types.hpp>
#include <algorithm>
#include <span>
#include <string_view>

namespace Domain {

inline std::string ImportModuleName(std::string name) {
    if (name.ends_with(".prx")) name.resize(name.size() - 4);
    else if (name.ends_with(".suprx")) name.resize(name.size() - 6);
    if (name.ends_with("-module")) name.resize(name.size() - 7);
    std::replace(name.begin(), name.end(), '.', '_');
    return name;
}

inline std::string ImportModule(const std::string& symbol, const std::map<std::uint64_t, std::string>& modules,
    std::span<const std::string> dependencies = {}, const std::map<std::uint64_t, std::string>& libraries = {}) {
    if (modules.empty()) return {};
    const auto first = symbol.find('#');
    if (first == std::string::npos) return {};
    const auto second = symbol.find('#', first + 1);
    if (second == std::string::npos || second + 1 == symbol.size())
        throw RelinkerException("Invalid qualified import: " + symbol);
    const auto resolve = [&](std::string_view encoded, const auto& names, const std::string& kind) {
        constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
        if (encoded.empty()) throw RelinkerException("Invalid import " + kind + " ID: " + symbol);
        std::uint64_t id = 0;
        for (const char character : encoded) {
            const auto digit = alphabet.find(character);
            if (digit == std::string_view::npos || id > (65535u - digit) / 64u)
                throw RelinkerException("Invalid import " + kind + " ID: " + symbol);
            id = id * 64u + digit;
        }
        const auto entry = names.find(id);
        if (entry == names.end()) throw RelinkerException("Unknown import " + kind + " ID: " + symbol);
        auto name = entry->second;
        if (name.empty() || name.find_first_of("/\\:$\r\n") != std::string::npos)
            throw RelinkerException("Invalid import " + kind + " name: " + name);
        if (!name.ends_with(".prx") && !name.ends_with(".suprx")) name += ".prx";
        if (std::find(dependencies.begin(), dependencies.end(), name) != dependencies.end()) return name;
        std::string matched;
        for (const auto& dependency : dependencies) {
            if (ImportModuleName(dependency) != ImportModuleName(name)) continue;
            if (!matched.empty() && matched != dependency) throw RelinkerException("Ambiguous import " + kind + " dependency: " + name);
            matched = dependency;
        }
        return matched;
    };
    auto matched = resolve(std::string_view(symbol).substr(second + 1), modules, "module");
    if (!matched.empty() || libraries.empty()) return matched;
    return resolve(std::string_view(symbol).substr(first + 1, second - first - 1), libraries, "library");
}

}

#endif
