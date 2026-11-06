#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFont/include/FontWords.hpp"

namespace {

constexpr std::uint16_t WORDS_MAGIC = 0x0F0D;
constexpr std::int32_t PARSER_RESULT_ERROR = -1;
constexpr std::int32_t PARSER_RESULT_TERMINATE = 0;
constexpr std::int32_t PARSER_RESULT_FONT_CODE = 1;
constexpr std::size_t MAX_WORD_DEPTH = 128;
constexpr std::size_t MAX_WORD_NODES = 0xFFFF;
constexpr std::size_t MAX_MATCHES = 16;
constexpr std::uint32_t WORD_END = 0x80000000u;
constexpr std::uint32_t WORD_CODE_MASK = 0x7FFFFFFFu;
constexpr std::uint64_t CHARACTER_SEGMENTED_SCRIPT_MASK = 0xFFull;
constexpr std::uint64_t CHARACTER_BREAK_CLASS_MASK = 0x7ull << 43;
constexpr std::uint64_t CHARACTER_BREAK_CLASS_JOINED = 0x1ull << 43;
constexpr std::uint8_t CLUSTER_KIND_LEADING = 1;

struct WordNode {
    std::uint32_t code;
    std::uint16_t childCount;
    std::uint16_t firstChild;
};

static_assert(sizeof(WordNode) == 8);

struct WordsData {
    std::uint16_t magic = WORDS_MAGIC;
    std::uint16_t reserved = 0;
    std::uint32_t nodeCount = 0;
    std::uint16_t rootCount = 0;
    const FontMemory* memory = nullptr;
    WordNode* nodes = nullptr;
};

struct BuildNode {
    std::uint32_t code = 0;
    bool end = false;
    std::uint16_t childCount = 0;
    std::size_t firstChild = 0;
};

using BuildLevels = std::vector<std::vector<BuildNode>>;

struct WordMatch {
    FontTextCharacter* last = nullptr;
    std::uint32_t length = 0;
};

struct MatchRing {
    std::array<WordMatch, MAX_MATCHES> entries{};
    std::uint32_t count = 0;

    const WordMatch& Longest() const { return entries[(count - 1) % MAX_MATCHES]; }
};

int ParseFailure(std::int32_t state, const FontTextParseResult& result) {
    if (state == PARSER_RESULT_ERROR && result.Error.errorCode < 0) return result.Error.errorCode;
    return SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
}

int AddCode(BuildLevels& levels, std::size_t& depth, std::size_t& pathLength, std::uint32_t code) {
    if (levels.size() <= depth) levels.emplace_back();
    auto& level = levels[depth];
    if (depth < pathLength) {
        const std::uint32_t previous = level.back().code;
        if (code == previous) {
            ++depth;
            return SCE_FONT_OK;
        }
        if (code < previous) return SCE_FONT_ERROR_NO_SUPPORT_CODE;
        level.push_back({code});
        if (depth > 0) ++levels[depth - 1].back().childCount;
    } else {
        level.push_back({code});
        if (depth > 0) {
            BuildNode& parent = levels[depth - 1].back();
            parent.firstChild = level.size() - 1;
            parent.childCount = 1;
        }
    }
    pathLength = ++depth;
    return SCE_FONT_OK;
}

int ReadWords(FontTextSource* source, BuildLevels& levels) {
    std::size_t depth = 0;
    std::size_t pathLength = 0;
    void* order = nullptr;
    for (;;) {
        FontTextParseResult result{};
        const std::int32_t state = source->textParser(source, &order, &result);
        if (state == PARSER_RESULT_TERMINATE) {
            if (depth > 0) levels[depth - 1].back().end = true;
            if (result.Terminate.terminateCode == 0) return SCE_FONT_OK;
            depth = 0;
            continue;
        }
        if (state != PARSER_RESULT_FONT_CODE) return ParseFailure(state, result);
        const std::uint32_t code = result.FontCode.code;
        if (static_cast<std::int32_t>(code) <= 0) return SCE_FONT_ERROR_NO_SUPPORT_CODE;
        if (depth >= MAX_WORD_DEPTH) return SCE_FONT_OK;
        if (const int rc = AddCode(levels, depth, pathLength, code); rc != SCE_FONT_OK) return rc;
    }
}

std::size_t CountNodes(const BuildLevels& levels) {
    std::size_t count = 0;
    for (const auto& level : levels) count += level.size();
    return count;
}

void StoreNodes(const BuildLevels& levels, WordNode* nodes) {
    std::size_t levelStart = 0;
    for (std::size_t depth = 0; depth < levels.size(); ++depth) {
        const std::size_t childStart = levelStart + levels[depth].size();
        for (std::size_t i = 0; i < levels[depth].size(); ++i) {
            const BuildNode& node = levels[depth][i];
            WordNode& stored = nodes[levelStart + i];
            stored.code = node.code | (node.end ? WORD_END : 0u);
            stored.childCount = node.childCount;
            stored.firstChild = node.childCount != 0 ? static_cast<std::uint16_t>(childStart + node.firstChild) : 0;
        }
        levelStart = childStart;
    }
}

int BuildWords(const FontMemory* memory, FontTextSource* source, FontWords** words) {
    if (const int rc = sceFontTextSourceRewind(source); rc != SCE_FONT_OK) return rc;
    BuildLevels levels;
    if (const int rc = ReadWords(source, levels); rc != SCE_FONT_OK) return rc;
    const std::size_t nodeCount = CountNodes(levels);
    if (nodeCount > MAX_WORD_NODES) return SCE_FONT_ERROR_FUNCTIONAL_LIMIT;
    void* raw = memory->iface->alloc(memory->mspace_handle, sizeof(WordsData));
    if (!raw) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    auto* data = new (raw) WordsData{};
    auto* nodes = static_cast<WordNode*>(memory->iface->alloc(memory->mspace_handle, static_cast<std::uint32_t>(nodeCount * sizeof(WordNode))));
    if (!nodes) {
        memory->iface->dealloc(memory->mspace_handle, raw);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    StoreNodes(levels, nodes);
    data->nodeCount = static_cast<std::uint32_t>(nodeCount);
    data->rootCount = levels.empty() ? 0 : static_cast<std::uint16_t>(levels[0].size());
    data->memory = memory;
    data->nodes = nodes;
    *words = reinterpret_cast<FontWords*>(data);
    return SCE_FONT_OK;
}

FontTextCharacter* NextVisible(FontTextCharacter* character) {
    do {
        character = character->next;
    } while (character && character->synthetic != 0);
    return character;
}

FontTextCharacter* NextRendered(FontTextCharacter* character) {
    do {
        character = character->next;
    } while (character && (character->synthetic != 0 || character->clusterIndex != 0));
    return character;
}

bool InSegmentedRun(const FontTextCharacter* character) {
    return (character->flags & CHARACTER_SEGMENTED_SCRIPT_MASK) != 0;
}

bool EndsWordAfter(const FontTextCharacter* character, const FontTextCharacter* endCharacter) {
    const FontTextCharacter* follower = character->next;
    if (!follower) return true;
    if (follower->synthetic > 0) return false;
    if (follower->synthetic < 0 && follower->clusterKind != CLUSTER_KIND_LEADING) return false;
    if (follower->synthetic == 0 && follower->clusterIndex != 0) return false;
    while (follower && (follower->synthetic != 0 || follower->clusterIndex != 0)) follower = follower->next;
    if (!follower || follower == endCharacter) return true;
    return (follower->flags & CHARACTER_BREAK_CLASS_MASK) != CHARACTER_BREAK_CLASS_JOINED;
}

const WordNode* FindCode(const WordNode* group, std::uint32_t size, std::uint32_t code) {
    const WordNode* end = group + size;
    const WordNode* node = std::lower_bound(group, end, code, [](const WordNode& entry, std::uint32_t value) {
        return (entry.code & WORD_CODE_MASK) < value;
    });
    return node != end && (node->code & WORD_CODE_MASK) == code ? node : nullptr;
}

MatchRing MatchWords(const WordsData& data, FontTextCharacter* first, const FontTextCharacter* endCharacter) {
    MatchRing ring;
    const WordNode* group = data.nodes;
    std::uint32_t groupSize = data.rootCount;
    FontTextCharacter* character = first;
    FontTextCharacter* clusterStart = first;
    std::uint32_t length = 1;
    while (groupSize != 0) {
        const WordNode* node = FindCode(group, groupSize, character->characterCode);
        if (!node) break;
        if ((node->code & WORD_END) != 0 && EndsWordAfter(character, endCharacter)) {
            ring.entries[ring.count % MAX_MATCHES] = {clusterStart, length};
            ++ring.count;
        }
        if (node->childCount == 0) break;
        character = NextVisible(character);
        if (!character || character == endCharacter) break;
        if (character->clusterIndex == 0) clusterStart = character;
        ++length;
        group = data.nodes + node->firstChild;
        groupSize = node->childCount;
    }
    return ring;
}

bool OverrunsNextWord(const WordsData& data, FontTextCharacter* first, const FontTextCharacter* endCharacter, const WordMatch& longest) {
    FontTextCharacter* probe = first;
    for (std::uint32_t remaining = longest.length; remaining-- > 0;) {
        probe = NextVisible(probe);
        if (!probe) return false;
        if (!InSegmentedRun(probe)) continue;
        const MatchRing overlap = MatchWords(data, probe, endCharacter);
        return overlap.count != 0 && overlap.Longest().length > remaining;
    }
    return false;
}

FontTextCharacter* SelectWord(const WordsData& data, FontTextCharacter* first, const FontTextCharacter* endCharacter) {
    const MatchRing matches = MatchWords(data, first, endCharacter);
    if (matches.count == 0) return nullptr;
    const WordMatch& longest = matches.Longest();
    if (!InSegmentedRun(first) && InSegmentedRun(longest.last) && OverrunsNextWord(data, first, endCharacter, longest)) return nullptr;
    const std::uint32_t candidates = std::min<std::uint32_t>(matches.count, MAX_MATCHES);
    const WordMatch* fallback = nullptr;
    for (std::uint32_t i = 0; i < candidates; ++i) {
        const WordMatch& candidate = matches.entries[(matches.count - 1 - i) % MAX_MATCHES];
        if (!candidate.last) break;
        FontTextCharacter* follower = NextVisible(candidate.last);
        if (!follower || !InSegmentedRun(follower)) return candidate.last;
        if (MatchWords(data, follower, endCharacter).count != 0) return candidate.last;
        if (!fallback) fallback = &candidate;
    }
    return fallback ? fallback->last : nullptr;
}

}

int Font::CreateWords(const FontMemory* memory, FontTextSource* source, FontWords** words) {
    int rc = SCE_FONT_ERROR_INVALID_PARAMETER;
    if (memory && (memory->mem_kind != MEMORY_MAGIC || !memory->iface || !memory->iface->alloc || !memory->iface->dealloc)) {
        rc = SCE_FONT_ERROR_INVALID_MEMORY;
    } else if (memory && source && source->textParser && words) {
        rc = BuildWords(memory, source, words);
        if (rc == SCE_FONT_OK) {
            sceFontTextSourceRewind(source);
            return rc;
        }
    }
    if (words) *words = nullptr;
    if (source) sceFontTextSourceRewind(source);
    return rc;
}

int Font::DestroyWords(FontWords** words) {
    if (!words || !*words) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* data = reinterpret_cast<WordsData*>(*words);
    if (data->magic != WORDS_MAGIC || !data->memory || !data->memory->iface || !data->memory->iface->dealloc) return SCE_FONT_ERROR_INVALID_WORDS;
    const FontMemory* memory = data->memory;
    data->magic = 0;
    if (data->nodes) memory->iface->dealloc(memory->mspace_handle, data->nodes);
    memory->iface->dealloc(memory->mspace_handle, data);
    *words = nullptr;
    return SCE_FONT_OK;
}

int Font::WordsFindWordCharacters(FontWords* words, FontTextCharacter* firstCharacter, FontTextCharacter* endCharacter, FontTextCharacter** lastCharacter, FontTextCharacter** nextCharacter) {
    int rc = SCE_FONT_OK;
    FontTextCharacter* found = nullptr;
    const auto* data = reinterpret_cast<const WordsData*>(words);
    if (!words || !firstCharacter || !lastCharacter) {
        rc = SCE_FONT_ERROR_INVALID_PARAMETER;
    } else if (data->magic != WORDS_MAGIC || !data->nodes) {
        rc = SCE_FONT_ERROR_INVALID_WORDS;
    } else {
        found = SelectWord(*data, firstCharacter, endCharacter);
    }
    if (lastCharacter) *lastCharacter = found;
    if (nextCharacter) *nextCharacter = found ? NextRendered(found) : nullptr;
    return rc;
}
