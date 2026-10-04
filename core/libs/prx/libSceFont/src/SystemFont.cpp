#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

#include "prx/libSceFont/include/FontInternal.hpp"

namespace {

constexpr std::array<std::uint32_t, 120> SYSTEM_FONT_SET_TYPES = {
    0x18070043u, 0x18070044u, 0x18070045u, 0x18070047u, 0x18070053u, 0x18070054u,
    0x18070055u, 0x18070057u, 0x180700C3u, 0x180700C4u, 0x180700C5u, 0x180700C7u,
    0x18070444u, 0x18070447u, 0x18070454u, 0x18070457u, 0x180704C4u, 0x180704C7u,
    0x18071053u, 0x18071054u, 0x18071055u, 0x18071057u, 0x18071454u, 0x18071457u,
    0x18072444u, 0x18072447u, 0x180724C4u, 0x180724C7u, 0x18073454u, 0x18073457u,
    0x180734D4u, 0x180734D7u, 0x18078044u, 0x180780C4u, 0x18079054u, 0x1807A044u,
    0x1807A0C4u, 0x1807A444u, 0x1807A4C4u, 0x1807AC44u, 0x1807ACC4u, 0x1807B054u,
    0x1807B0D4u, 0x1807B454u, 0x1807B4D4u, 0x1807BC54u, 0x1807BCD4u, 0x18080444u,
    0x18080447u, 0x18080454u, 0x18080457u, 0x180804C4u, 0x180804C7u, 0x18081454u,
    0x18081457u, 0x18082444u, 0x18082447u, 0x180824C4u, 0x180824C7u, 0x18083454u,
    0x18083457u, 0x180834D4u, 0x180834D7u, 0x1808A444u, 0x1808A4C4u, 0x1808B454u,
    0x1808B4D4u, 0x180C8044u, 0x180C80C4u, 0x180C9054u, 0x180CA044u, 0x180CA0C4u,
    0x180CAC44u, 0x180CACC4u, 0x180CB054u, 0x180CB0D4u, 0x180CBC54u, 0x180CBCD4u,
    0x18170043u, 0x18170044u, 0x18170045u, 0x18170047u, 0x18170444u, 0x18170447u,
    0x18370044u, 0x18370047u, 0x18370444u, 0x18370447u, 0x1A070543u, 0x1A070547u,
    0x1A070553u, 0x1A070557u, 0x1A0705C3u, 0x1A0705C7u, 0x1A071553u, 0x1A071557u,
    0x1A072543u, 0x1A072547u, 0x1A0725C3u, 0x1A0725C7u, 0x1A073553u, 0x1A073557u,
    0x1A0735D3u, 0x1A0735D7u, 0x1A080543u, 0x1A080547u, 0x1A080553u, 0x1A080557u,
    0x1A0805C3u, 0x1A0805C7u, 0x1A081553u, 0x1A081557u, 0x1A082543u, 0x1A082547u,
    0x1A0825C3u, 0x1A0825C7u, 0x1A083553u, 0x1A083557u, 0x1A0835D3u, 0x1A0835D7u,
};

enum class Family { Latin, LatinItalic, Typewriter, Vietnamese, Thai, Japanese, ChineseGb };

struct FamilyFiles {
    std::array<std::string_view, 4> system;
    std::array<std::string_view, 4> substitute;
    std::uint32_t substituteSubFont;
};

constexpr std::array<FamilyFiles, 7> FAMILY_FILES = {{
    {{"SST-Light.otf", "SST-Roman.otf", "SST-Medium.otf", "SST-Bold.otf"},
     {"NotoSans-Light.ttf", "NotoSans-Regular.ttf", "NotoSans-Medium.ttf", "NotoSans-Bold.ttf"}, 0},
    {{"SST-LightItalic.otf", "SST-Italic.otf", "SST-MediumItalic.otf", "SST-BoldItalic.otf"},
     {"NotoSans-LightItalic.ttf", "NotoSans-Italic.ttf", "NotoSans-MediumItalic.ttf", "NotoSans-BoldItalic.ttf"}, 0},
    {{"SSTTypewriter-Roman.otf", "SSTTypewriter-Roman.otf", "SSTTypewriter-Roman.otf", "SSTTypewriter-Bd.otf"},
     {"NotoSansMono-Light.ttf", "NotoSansMono-Regular.ttf", "NotoSansMono-Medium.ttf", "NotoSansMono-Bold.ttf"}, 0},
    {{"SSTVietnamese-Light.otf", "SSTVietnamese-Roman.otf", "SSTVietnamese-Medium.otf", "SSTVietnamese-Bold.otf"},
     {"NotoSans-Light.ttf", "NotoSans-Regular.ttf", "NotoSans-Medium.ttf", "NotoSans-Bold.ttf"}, 0},
    {{"SSTThai-Light.otf", "SSTThai-Roman.otf", "SSTThai-Medium.otf", "SSTThai-Bold.otf"},
     {"NotoSansThai-Light.ttf", "NotoSansThai-Regular.ttf", "NotoSansThai-Medium.ttf", "NotoSansThai-Bold.ttf"}, 0},
    {{"SSTJpPro-Regular.otf", "SSTJpPro-Regular.otf", "SSTJpPro-Regular.otf", "SSTJpPro-Bold.otf"},
     {"NotoSansCJK-Light.ttc", "NotoSansCJK-Regular.ttc", "NotoSansCJK-Medium.ttc", "NotoSansCJK-Bold.ttc"}, 0},
    {{"DFHEI5-SONY.ttf", "DFHEI5-SONY.ttf", "DFHEI5-SONY.ttf", "DFHEI5-SONY.ttf"},
     {"NotoSansCJK-Light.ttc", "NotoSansCJK-Regular.ttc", "NotoSansCJK-Medium.ttc", "NotoSansCJK-Bold.ttc"}, 2},
}};

Family FamilyOf(std::uint32_t fontSetType) {
    const std::uint32_t region = (fontSetType >> 16) & 0xFFu;
    const std::uint32_t scripts = (fontSetType >> 8) & 0xFEu;
    const bool vietnamese = (fontSetType & 0x10u) != 0;
    switch (region) {
        case 0x17: return Family::LatinItalic;
        case 0x37: return Family::Typewriter;
        case 0x08: return scripts == 0x04 && vietnamese ? Family::Vietnamese : Family::Japanese;
        case 0x0C: return Family::ChineseGb;
        default:
            if (scripts == 0x10) return Family::Thai;
            if (scripts == 0 && vietnamese) return Family::Vietnamese;
            return Family::Latin;
    }
}

std::size_t WeightOf(std::uint32_t fontSetType) {
    switch (fontSetType & 0x0Fu) {
        case 0x3: return 0;
        case 0x5: return 2;
        case 0x7: return 3;
        default: return 1;
    }
}

std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    wchar_t module[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (length == 0 || length == MAX_PATH) throw std::runtime_error("libSceFont: cannot locate the executable");
    return std::filesystem::path(module).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

}

bool Font::IsSystemFontSet(std::uint32_t fontSetType) {
    return std::find(SYSTEM_FONT_SET_TYPES.begin(), SYSTEM_FONT_SET_TYPES.end(), fontSetType) != SYSTEM_FONT_SET_TYPES.end();
}

std::vector<Font::SystemFontFile> Font::SystemFontCandidates(std::uint32_t fontSetType) {
    if (!IsSystemFontSet(fontSetType)) return {};
    const FamilyFiles& files = FAMILY_FILES[static_cast<std::size_t>(FamilyOf(fontSetType))];
    const std::size_t weight = WeightOf(fontSetType);
    return {{std::filesystem::path(files.system[weight]), 0}, {std::filesystem::path(files.substitute[weight]), files.substituteSubFont}};
}

std::filesystem::path Font::SystemFontDirectory() {
    const char* configured = std::getenv("ANYPS5_SYSTEM_FONTS");
    if (!configured || configured[0] == '\0') return ExecutableDirectory() / "anyps5-fonts";
    const std::filesystem::path directory(configured);
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) throw std::runtime_error("libSceFont: ANYPS5_SYSTEM_FONTS is not a directory: " + directory.string());
    return directory;
}

std::optional<Font::SystemFontFile> Font::FindSystemFontFile(std::uint32_t fontSetType) {
    const std::vector<SystemFontFile> candidates = SystemFontCandidates(fontSetType);
    if (candidates.empty()) return std::nullopt;
    const std::filesystem::path directory = SystemFontDirectory();
    for (const SystemFontFile& candidate : candidates) {
        std::error_code error;
        const std::filesystem::path path = directory / candidate.path;
        if (std::filesystem::is_regular_file(path, error)) return SystemFontFile{path, candidate.subFontIndex};
    }
    return std::nullopt;
}
