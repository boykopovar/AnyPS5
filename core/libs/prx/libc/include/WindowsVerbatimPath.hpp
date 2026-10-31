#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSVERBATIMPATH_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSVERBATIMPATH_HPP

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

constexpr std::size_t WindowsMaxPathLength = 260;
constexpr std::size_t WindowsMaxDirectoryPathLength = 248;

template <typename CharT>
constexpr CharT ToAsciiLower_nid_no_patch(CharT c) {
    return (c >= static_cast<CharT>('A') && c <= static_cast<CharT>('Z'))
        ? static_cast<CharT>(c + (static_cast<CharT>('a') - static_cast<CharT>('A')))
        : c;
}

template <typename CharT>
constexpr bool IsDosDeviceStem_nid_no_patch(std::basic_string_view<CharT> stem) {
    while (!stem.empty() && (stem.back() == static_cast<CharT>(' ') || stem.back() == static_cast<CharT>('.'))) {
        stem.remove_suffix(1);
    }
    if (stem.size() == 3) {
        const auto c0 = ToAsciiLower_nid_no_patch(stem[0]);
        const auto c1 = ToAsciiLower_nid_no_patch(stem[1]);
        const auto c2 = ToAsciiLower_nid_no_patch(stem[2]);
        return (c0 == static_cast<CharT>('c') && c1 == static_cast<CharT>('o') && c2 == static_cast<CharT>('n')) ||
               (c0 == static_cast<CharT>('p') && c1 == static_cast<CharT>('r') && c2 == static_cast<CharT>('n')) ||
               (c0 == static_cast<CharT>('a') && c1 == static_cast<CharT>('u') && c2 == static_cast<CharT>('x')) ||
               (c0 == static_cast<CharT>('n') && c1 == static_cast<CharT>('u') && c2 == static_cast<CharT>('l'));
    }
    if (stem.size() == 4) {
        const auto c0 = ToAsciiLower_nid_no_patch(stem[0]);
        const auto c1 = ToAsciiLower_nid_no_patch(stem[1]);
        const auto c2 = ToAsciiLower_nid_no_patch(stem[2]);
        if ((c0 == static_cast<CharT>('c') && c1 == static_cast<CharT>('o') && c2 == static_cast<CharT>('m')) ||
            (c0 == static_cast<CharT>('l') && c1 == static_cast<CharT>('p') && c2 == static_cast<CharT>('t'))) {
            const auto c3 = stem[3];
            if (c3 >= static_cast<CharT>('1') && c3 <= static_cast<CharT>('9')) {
                return true;
            }
            using UCharT = std::make_unsigned_t<CharT>;
            const auto u3 = static_cast<UCharT>(c3);
            if (u3 == static_cast<UCharT>(0x00B9) || u3 == static_cast<UCharT>(0x00B2) || u3 == static_cast<UCharT>(0x00B3)) {
                return true;
            }
        }
    }
    if constexpr (sizeof(CharT) == 1) {
        if (stem.size() == 5) {
            const auto c0 = ToAsciiLower_nid_no_patch(stem[0]);
            const auto c1 = ToAsciiLower_nid_no_patch(stem[1]);
            const auto c2 = ToAsciiLower_nid_no_patch(stem[2]);
            if ((c0 == static_cast<CharT>('c') && c1 == static_cast<CharT>('o') && c2 == static_cast<CharT>('m')) ||
                (c0 == static_cast<CharT>('l') && c1 == static_cast<CharT>('p') && c2 == static_cast<CharT>('t'))) {
                using UCharT = std::make_unsigned_t<CharT>;
                const auto u3 = static_cast<UCharT>(stem[3]);
                const auto u4 = static_cast<UCharT>(stem[4]);
                if (u3 == static_cast<UCharT>(0xC2) &&
                    (u4 == static_cast<UCharT>(0xB9) || u4 == static_cast<UCharT>(0xB2) || u4 == static_cast<UCharT>(0xB3))) {
                    return true;
                }
            }
        }
    }
    if (stem.size() == 6) {
        const auto c0 = ToAsciiLower_nid_no_patch(stem[0]);
        const auto c1 = ToAsciiLower_nid_no_patch(stem[1]);
        const auto c2 = ToAsciiLower_nid_no_patch(stem[2]);
        const auto c3 = ToAsciiLower_nid_no_patch(stem[3]);
        const auto c4 = ToAsciiLower_nid_no_patch(stem[4]);
        const auto c5 = stem[5];
        return c0 == static_cast<CharT>('c') && c1 == static_cast<CharT>('o') && c2 == static_cast<CharT>('n') &&
               c3 == static_cast<CharT>('i') && c4 == static_cast<CharT>('n') && c5 == static_cast<CharT>('$');
    }
    if (stem.size() == 7) {
        const auto c0 = ToAsciiLower_nid_no_patch(stem[0]);
        const auto c1 = ToAsciiLower_nid_no_patch(stem[1]);
        const auto c2 = ToAsciiLower_nid_no_patch(stem[2]);
        const auto c3 = ToAsciiLower_nid_no_patch(stem[3]);
        const auto c4 = ToAsciiLower_nid_no_patch(stem[4]);
        const auto c5 = ToAsciiLower_nid_no_patch(stem[5]);
        const auto c6 = stem[6];
        return c0 == static_cast<CharT>('c') && c1 == static_cast<CharT>('o') && c2 == static_cast<CharT>('n') &&
               c3 == static_cast<CharT>('o') && c4 == static_cast<CharT>('u') && c5 == static_cast<CharT>('t') &&
               c6 == static_cast<CharT>('$');
    }
    return false;
}

template <typename CharT>
inline bool IsDosDeviceStem_nid_no_patch(const std::basic_string<CharT>& stem) {
    return IsDosDeviceStem_nid_no_patch(std::basic_string_view<CharT>(stem));
}

template <typename CharT>
inline bool IsDosDeviceStem_nid_no_patch(const CharT* stem) {
    return IsDosDeviceStem_nid_no_patch(std::basic_string_view<CharT>(stem));
}

template <typename CharT>
constexpr bool ComponentNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT> component) {
    if (component.empty()) return false;
    if (component.size() == 1 && component[0] == static_cast<CharT>('.')) return false;
    if (component.size() == 2 && component[0] == static_cast<CharT>('.') && component[1] == static_cast<CharT>('.')) return false;
    if (component.back() == static_cast<CharT>('.') || component.back() == static_cast<CharT>(' ')) return true;
    const auto dotPos = component.find(static_cast<CharT>('.'));
    const auto stem = (dotPos == std::basic_string_view<CharT>::npos) ? component : component.substr(0, dotPos);
    return IsDosDeviceStem_nid_no_patch(stem);
}

template <typename CharT>
inline bool ComponentNeedsVerbatimPrefix_nid_no_patch(const std::basic_string<CharT>& component) {
    return ComponentNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT>(component));
}

template <typename CharT>
inline bool ComponentNeedsVerbatimPrefix_nid_no_patch(const CharT* component) {
    return ComponentNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT>(component));
}

template <typename CharT>
bool PathNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT> path) {
    const CharT backslash = static_cast<CharT>('\\');
    const CharT slash = static_cast<CharT>('/');
    const CharT qmark = static_cast<CharT>('?');
    const CharT dot = static_cast<CharT>('.');
    if (path.size() >= 4 &&
        (path[0] == backslash || path[0] == slash) &&
        (path[1] == backslash || path[1] == slash) &&
        (path[2] == qmark || path[2] == dot) &&
        (path[3] == backslash || path[3] == slash)) {
        return false;
    }
    std::size_t start = 0;
    while (start < path.size()) {
        while (start < path.size() && (path[start] == slash || path[start] == backslash)) {
            ++start;
        }
        if (start >= path.size()) break;
        std::size_t end = start;
        while (end < path.size() && path[end] != slash && path[end] != backslash) {
            ++end;
        }
        auto component = path.substr(start, end - start);
        if (component.size() >= 2 && component[1] == static_cast<CharT>(':') &&
            ((component[0] >= static_cast<CharT>('a') && component[0] <= static_cast<CharT>('z')) ||
             (component[0] >= static_cast<CharT>('A') && component[0] <= static_cast<CharT>('Z')))) {
            component = component.substr(2);
        }
        if (ComponentNeedsVerbatimPrefix_nid_no_patch(component)) return true;
        start = end;
    }
    return false;
}

template <typename CharT>
inline bool PathNeedsVerbatimPrefix_nid_no_patch(const std::basic_string<CharT>& path) {
    return PathNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT>(path));
}

template <typename CharT>
inline bool PathNeedsVerbatimPrefix_nid_no_patch(const CharT* path) {
    return PathNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT>(path));
}

inline bool NeedsVerbatimPrefix_nid_no_patch(const std::filesystem::path& path) {
    using CharT = std::filesystem::path::value_type;
    const auto& native = path.native();
    return PathNeedsVerbatimPrefix_nid_no_patch(std::basic_string_view<CharT>(native));
}

template <typename StringT>
inline StringT FormatVerbatimWindowsPath_nid_no_patch(const StringT& normalizedPath) {
    using CharT = typename StringT::value_type;
    const CharT backslash = static_cast<CharT>('\\');
    const CharT slash = static_cast<CharT>('/');
    const CharT qmark = static_cast<CharT>('?');
    const CharT dot = static_cast<CharT>('.');

    if (normalizedPath.size() >= 4 &&
        (normalizedPath[0] == backslash || normalizedPath[0] == slash) &&
        (normalizedPath[1] == backslash || normalizedPath[1] == slash) &&
        (normalizedPath[2] == qmark || normalizedPath[2] == dot) &&
        (normalizedPath[3] == backslash || normalizedPath[3] == slash)) {
        StringT result = normalizedPath;
        for (auto& c : result) {
            if (c == slash) c = backslash;
        }
        return result;
    }

    StringT clean = normalizedPath;
    for (auto& c : clean) {
        if (c == slash) c = backslash;
    }

    if (clean.size() >= 2 && clean[0] == backslash && clean[1] == backslash) {
        if constexpr (std::is_same_v<CharT, wchar_t>) {
            return std::wstring(L"\\\\?\\UNC\\") + clean.substr(2);
        } else {
            return std::string(R"(\\?\UNC\)") + clean.substr(2);
        }
    }

    if constexpr (std::is_same_v<CharT, wchar_t>) {
        return std::wstring(L"\\\\?\\") + clean;
    } else {
        return std::string(R"(\\?\)") + clean;
    }
}

template <typename CharT>
inline std::basic_string<CharT> MakeVerbatimWindowsPathForTest_nid_no_patch(
    std::basic_string_view<CharT> normalizedPath,
    std::size_t maxPathLength = WindowsMaxDirectoryPathLength) {
    if (normalizedPath.size() < maxPathLength && !PathNeedsVerbatimPrefix_nid_no_patch(normalizedPath)) {
        return std::basic_string<CharT>(normalizedPath);
    }
    return FormatVerbatimWindowsPath_nid_no_patch(std::basic_string<CharT>(normalizedPath));
}

template <typename CharT>
inline std::basic_string<CharT> MakeVerbatimWindowsPathForTest_nid_no_patch(
    const std::basic_string<CharT>& normalizedPath,
    std::size_t maxPathLength = WindowsMaxDirectoryPathLength) {
    return MakeVerbatimWindowsPathForTest_nid_no_patch(std::basic_string_view<CharT>(normalizedPath), maxPathLength);
}

template <typename CharT>
inline std::basic_string<CharT> MakeVerbatimWindowsPathForTest_nid_no_patch(
    const CharT* normalizedPath,
    std::size_t maxPathLength = WindowsMaxDirectoryPathLength) {
    return MakeVerbatimWindowsPathForTest_nid_no_patch(std::basic_string_view<CharT>(normalizedPath), maxPathLength);
}

#ifdef _WIN32
inline std::filesystem::path NativeVerbatimPath_nid_no_patch(
    const std::filesystem::path& path,
    std::error_code& error,
    std::size_t maxPathLength = WindowsMaxDirectoryPathLength) {
    const auto& input = path.native();
    if (input.empty()) return path;
    if (input.starts_with(L"\\\\?\\") || input.starts_with(L"\\\\.\\")) return path;
    auto absolute = path.is_absolute() ? path : (std::filesystem::current_path(error) / path);
    if (error) return {};
    absolute = absolute.lexically_normal().make_preferred();
    const auto& native = absolute.native();
    if (native.size() < maxPathLength && !NeedsVerbatimPrefix_nid_no_patch(absolute)) return absolute;
    if (native.starts_with(L"\\\\?\\") || native.starts_with(L"\\\\.\\")) return absolute;
    return std::filesystem::path(FormatVerbatimWindowsPath_nid_no_patch(native));
}
#else
inline std::filesystem::path NativeVerbatimPath_nid_no_patch(
    const std::filesystem::path& path,
    std::error_code&,
    std::size_t = WindowsMaxDirectoryPathLength) {
    return path;
}
#endif

#endif
