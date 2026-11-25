#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int GlobAppend = 0x0001;
constexpr int GlobDooffs = 0x0002;
constexpr int GlobErr = 0x0004;
constexpr int GlobMark = 0x0008;
constexpr int GlobNocheck = 0x0010;
constexpr int GlobNosort = 0x0020;
constexpr int GlobNomagic = 0x0200;
constexpr int GlobMagchar = 0x0100;
constexpr int GlobNoescape = 0x2000;
constexpr int GlobNospace = -1;
constexpr int GlobAborted = -2;
constexpr int GlobNomatch = -3;
constexpr int GlobSupported = GlobAppend | GlobDooffs | GlobErr | GlobMark | GlobNocheck | GlobNosort | GlobNomagic | GlobNoescape;

constexpr int FnmNoescapeFlag = 0x01;
constexpr int GuestEnoent = 2;
constexpr int GuestEnotdir = 20;
constexpr std::size_t MaximumPattern = 4096;

struct GuestGlob {
    std::size_t pathc;
    std::size_t matchc;
    std::size_t offs;
    int flags;
    char** pathv;
    int (APS5_VABI *errfunc)(const char*, int);
    void* closedir;
    void* readdir;
    void* opendir;
    void* lstat;
    void* stat;
};
static_assert(sizeof(GuestGlob) == 88 && offsetof(GuestGlob, pathv) == 32 && offsetof(GuestGlob, errfunc) == 40);

struct Segment {
    std::string separators;
    std::string text;
};

struct Walk {
    std::vector<Segment> components;
    std::vector<std::string> matches;
    bool dirOnly = false;
    bool noescape = false;
    bool mark = false;
    bool err = false;
    int (APS5_VABI *errfunc)(const char*, int) = nullptr;
    int status = 0;
};

bool HasMagic(const std::string& text, bool noescape) {
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (!noescape && text[index] == '\\' && index + 1 < text.size()) {
            ++index;
            continue;
        }
        if (text[index] == '*' || text[index] == '?' || text[index] == '[') return true;
    }
    return false;
}

std::string Unescape(const std::string& text, bool noescape) {
    if (noescape) return text;
    std::string output;
    output.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\\' && index + 1 < text.size()) ++index;
        output += text[index];
    }
    return output;
}

std::vector<Segment> Split(const std::string& pattern) {
    std::vector<Segment> segments;
    std::size_t position = 0;
    while (position < pattern.size()) {
        std::size_t separatorsEnd = position;
        while (separatorsEnd < pattern.size() && pattern[separatorsEnd] == '/') ++separatorsEnd;
        std::size_t textEnd = pattern.find('/', separatorsEnd);
        if (textEnd == std::string::npos) textEnd = pattern.size();
        segments.push_back({pattern.substr(position, separatorsEnd - position), pattern.substr(separatorsEnd, textEnd - separatorsEnd)});
        position = textEnd;
    }
    return segments;
}

bool IsDirectory(const FileStat& status) {
    return (status.st_mode & 0170000) == 0040000;
}

void Report(Walk& walk, const std::string& path, int error) {
    const bool stop = walk.errfunc != nullptr && walk.errfunc(path.c_str(), error) != 0;
    if (stop || walk.err) walk.status = GlobAborted;
}

void Expand(Walk& walk, const std::string& prefix, std::size_t index);

void Finish(Walk& walk, const std::string& path) {
    if (path.empty()) return;
    FileStat status{};
    if (stat_nid_postfix(path.c_str(), &status) != 0) return;
    const bool directory = IsDirectory(status);
    if (walk.dirOnly && !directory) return;
    if (walk.mark && directory) {
        walk.matches.push_back(path.back() == '/' ? path : path + "/");
        return;
    }
    walk.matches.push_back(path);
}

void Expand(Walk& walk, const std::string& prefix, std::size_t index) {
    if (walk.status != 0) return;
    if (index == walk.components.size()) {
        Finish(walk, prefix);
        return;
    }
    const Segment& segment = walk.components[index];
    const bool last = index + 1 == walk.components.size();
    const std::string base = prefix + segment.separators;
    if (!HasMagic(segment.text, walk.noescape)) {
        const std::string next = base + Unescape(segment.text, walk.noescape);
        if (!last) {
            FileStat status{};
            if (stat_nid_postfix(next.c_str(), &status) != 0 || !IsDirectory(status)) return;
        }
        Expand(walk, next, index + 1);
        return;
    }
    const std::string directory = base.empty() ? std::string(".") : base;
    void* handle = opendir_nid_postfix(directory.c_str());
    if (handle == nullptr) {
        const int error = *__error_nid_postfix();
        if (error != GuestEnoent && error != GuestEnotdir) Report(walk, directory, error);
        return;
    }
    std::vector<std::string> names;
    while (GuestDirectoryEntry* entry = readdir_nid_postfix(handle)) names.emplace_back(entry->name, entry->nameLength);
    closedir_nid_postfix(handle);
    const bool dotPattern = segment.text[0] == '.';
    for (const std::string& name : names) {
        if (name.empty() || (name[0] == '.' && !dotPattern)) continue;
        if (fnmatch_nid_postfix(segment.text.c_str(), name.c_str(), walk.noescape ? FnmNoescapeFlag : 0) != 0) continue;
        Expand(walk, base + name, index + 1);
        if (walk.status != 0) return;
    }
}

int Publish(GuestGlob& guest, void* pglob, const std::vector<std::string>& matches, bool append, std::size_t offs, int flags, bool magic, int (APS5_VABI *errfunc)(const char*, int)) {
    std::vector<char*> texts;
    const auto release = [&texts] {
        for (char* text : texts) free_nid_postfix(text);
    };
    for (const std::string& match : matches) {
        char* text = static_cast<char*>(malloc_nid_postfix(match.size() + 1));
        if (text == nullptr) {
            release();
            return GlobNospace;
        }
        std::memcpy(text, match.data(), match.size());
        text[match.size()] = '\0';
        texts.push_back(text);
    }
    const std::size_t oldCount = append ? guest.pathc : 0;
    char** previous = append ? guest.pathv : nullptr;
    const std::size_t newCount = oldCount + matches.size();
    const bool replace = !matches.empty() || (previous == nullptr && offs != 0);
    char** array = previous;
    if (replace) {
        const std::size_t slots = offs + newCount + 1;
        char** storage = static_cast<char**>(malloc_nid_postfix(slots * sizeof(char*)));
        if (storage == nullptr) {
            release();
            return GlobNospace;
        }
        for (std::size_t index = 0; index < offs; ++index) storage[index] = nullptr;
        for (std::size_t index = 0; index < oldCount; ++index) storage[offs + index] = previous[offs + index];
        for (std::size_t index = 0; index < texts.size(); ++index) storage[offs + oldCount + index] = texts[index];
        storage[offs + newCount] = nullptr;
        array = storage;
    }
    GuestGlob updated = guest;
    updated.pathc = newCount;
    updated.matchc = matches.size();
    updated.offs = offs;
    updated.pathv = array;
    updated.flags = (flags & ~GlobMagchar) | (magic ? GlobMagchar : 0);
    updated.errfunc = errfunc;
    if (!GuestCopyOut_nid_no_patch(pglob, &updated, sizeof(updated))) {
        if (replace) {
            release();
            free_nid_postfix(array);
        }
        return GlobAborted;
    }
    if (replace && previous != nullptr) free_nid_postfix(previous);
    guest = updated;
    return 0;
}

}

extern "C" int APS5_VABI glob_nid_postfix(const char* pattern, int flags, int (APS5_VABI *errfunc)(const char*, int), void* pglob) {
    if (pattern == nullptr || pglob == nullptr) throw std::invalid_argument("glob: null pattern or output");
    if (flags & ~GlobSupported) NotImplemented_nid_no_patch(__func__);
    GuestGlob guest{};
    if (!GuestCopyIn_nid_no_patch(&guest, pglob, sizeof(guest))) return GlobAborted;
    std::string copy;
    if (CopyGuestString_nid_no_patch(pattern, MaximumPattern, copy) != 0) return GlobAborted;

    Walk walk;
    walk.noescape = (flags & GlobNoescape) != 0;
    walk.mark = (flags & GlobMark) != 0;
    walk.err = (flags & GlobErr) != 0;
    walk.errfunc = errfunc;
    walk.dirOnly = !copy.empty() && copy.back() == '/';
    walk.components = Split(copy);
    bool magic = false;
    for (const Segment& segment : walk.components) magic = magic || HasMagic(segment.text, walk.noescape);

    if (!copy.empty()) Expand(walk, std::string(), 0);
    if (walk.status == 0 && !(flags & GlobNosort)) std::sort(walk.matches.begin(), walk.matches.end());
    if (walk.status == 0 && walk.matches.empty() && ((flags & GlobNocheck) || ((flags & GlobNomagic) && !magic)))
        walk.matches.push_back(copy);

    const bool append = (flags & GlobAppend) != 0;
    const std::size_t offs = (append || (flags & GlobDooffs)) ? guest.offs : 0;
    const int published = Publish(guest, pglob, walk.matches, append, offs, flags, magic, errfunc);
    if (published != 0) return published;
    if (walk.status != 0) return walk.status;
    return walk.matches.empty() ? GlobNomatch : 0;
}

extern "C" void APS5_VABI globfree_nid_postfix(void* pglob) {
    if (pglob == nullptr) throw std::invalid_argument("globfree: null glob");
    GuestGlob guest{};
    if (!GuestCopyIn_nid_no_patch(&guest, pglob, sizeof(guest))) return;
    if (guest.pathv != nullptr) {
        for (std::size_t index = guest.offs; index < guest.offs + guest.pathc; ++index) free_nid_postfix(guest.pathv[index]);
        free_nid_postfix(guest.pathv);
    }
    guest.pathc = 0;
    guest.pathv = nullptr;
    GuestCopyOut_nid_no_patch(pglob, &guest, sizeof(guest));
}
