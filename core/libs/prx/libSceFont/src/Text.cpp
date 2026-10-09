// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libSceFont/include/FontInternal.hpp"

namespace {

using namespace Font;

constexpr std::uint16_t TEXT_SOURCE_MAGIC = 0x0F04;
constexpr std::uint16_t FONT_STRING_MAGIC = 0x0F05;
constexpr std::uint16_t FONT_WRITING_MAGIC = 0x0F06;
constexpr std::uint16_t CREATE_STRING_DETAIL_ID = 0x0FD4;
constexpr std::uint32_t WRITING_FORM_HORIZONTAL = 0x10;
constexpr std::uint32_t WRITING_FORM_HORIZONTAL_LTR = 0x12;
constexpr std::int32_t PARSER_RESULT_ERROR = -1;
constexpr std::int32_t PARSER_RESULT_TERMINATE = 0;
constexpr std::int32_t PARSER_RESULT_FONT_CODE = 1;
constexpr std::size_t MAX_PARSED_CHARACTERS = 4096;
constexpr std::int32_t WRITING_MASK_NONE = 0;
constexpr std::int32_t WRITING_MASK_FORMAT_CHARACTERS = 1;
constexpr std::uint64_t CHARACTER_FLAG_FORMAT = 1ull << 42;
constexpr std::uint64_t TEXT_CODES_TAG = 0x5345444F43545854ull;
constexpr int SYLLABLE_STRING_NON_DETECTION = 0;

struct TextCodesState {
    std::uint64_t tag;
    FontTextCharacter* current;
    const FontTextCharacter* term;
};

static_assert(sizeof(TextCodesState) <= sizeof(FontTextCodes::systemUse));

[[noreturn]] void Unsupported(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

std::string CodeName(std::uint32_t code) {
    char name[16];
    std::snprintf(name, sizeof(name), "U+%04X", static_cast<unsigned>(code));
    return name;
}

FontTextCodes* SetTextCodesStep(FontTextCodes* textCodes, FontTextCharacter* character, const FontTextCharacter* term) {
    textCodes->textOrder = character->textOrder;
    textCodes->textCode = character->characterCode;
    const TextCodesState state{TEXT_CODES_TAG, character, term};
    std::memcpy(textCodes->systemUse, &state, sizeof(state));
    return textCodes;
}

TextCodesState GetTextCodesStep(const char* function, const FontTextCodes* textCodesStep) {
    TextCodesState state{};
    std::memcpy(&state, textCodesStep->systemUse, sizeof(state));
    if (state.tag != TEXT_CODES_TAG || !state.current) Unsupported(function, "text codes step not set up by sceFontCharactersRefersTextCodes");
    return state;
}

bool IsSyllableFreeCode(std::uint32_t code) {
    struct Range {
        std::uint32_t first;
        std::uint32_t last;
    };
    static constexpr Range ranges[] = {
        {0x0000, 0x00AC}, {0x00AE, 0x02FF}, {0x0370, 0x03FF}, {0x0400, 0x0482}, {0x048A, 0x052F},
        {0x1E00, 0x1FFF}, {0x2000, 0x200A}, {0x2010, 0x2027}, {0x202F, 0x205F}, {0x2070, 0x20CF},
        {0x2100, 0x2BFF}, {0x3000, 0x3029}, {0x3030, 0x303F}, {0x3041, 0x3096}, {0x309B, 0x30FF},
        {0x31F0, 0x31FF}, {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xF900, 0xFAFF}, {0xFF01, 0xFF9D},
        {0xFFE0, 0xFFEE}, {0x20000, 0x3FFFF},
    };
    return std::any_of(std::begin(ranges), std::end(ranges), [code](const Range& range) { return code >= range.first && code <= range.last; });
}

struct CharacterStorage {
    std::atomic<std::uint32_t> refCount{1};
    std::vector<FontTextCharacter> characters;
};

struct FontStringData {
    std::uint16_t magic = FONT_STRING_MAGIC;
    std::uint8_t flags = 0;
    std::uint8_t reserved0 = 0;
    std::uint32_t writingForm = WRITING_FORM_HORIZONTAL;
    const FontMemory* memory = nullptr;
    FontTextSource* source = nullptr;
    FontHandle defaultFont = nullptr;
    std::uint32_t terminateCode = 0;
    void* terminateOrder = nullptr;
    std::uint32_t textCount = 0;
    std::uint32_t renderCount = 0;
    CharacterStorage* storage = nullptr;
};

struct FontWritingData {
    std::uint16_t magic = FONT_WRITING_MAGIC;
    std::uint16_t reserved0 = 0;
    std::uint32_t writingForm = WRITING_FORM_HORIZONTAL;
    FontString string = nullptr;
    CharacterStorage* storage = nullptr;
    FontHandle defaultFont = nullptr;
    std::size_t nextIndex = 0;
    std::size_t currentIndex = 0;
    float penX = 0.0f;
    float penY = 0.0f;
    bool hasAggregateMetrics = false;
    bool hasCurrentStep = false;
    bool maskFormatCharacters = true;
    bool ownsStorage = false;
    FontWritingStep step{};
    FontWritingMetrics metrics{};
    FontGlyphMetrics rawGlyphMetrics{};
    bool hasRawGlyphMetrics = false;
};

static_assert(sizeof(FontWritingData) <= sizeof(FontWriting));

FontStringData* GetStringData(FontString fontString) {
    auto* data = reinterpret_cast<FontStringData*>(fontString);
    return data && data->magic == FONT_STRING_MAGIC ? data : nullptr;
}

FontWritingData* GetWritingData(FontWriting* fontWriting) {
    if (!fontWriting) return nullptr;
    auto* data = reinterpret_cast<FontWritingData*>(fontWriting->systemUse);
    return data->magic == FONT_WRITING_MAGIC ? data : nullptr;
}

void ReleaseStorage(CharacterStorage*& storage) {
    if (!storage) return;
    if (storage->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) delete storage;
    storage = nullptr;
}

void ReleaseWritingStorage(FontWritingData& writing) {
    if (!writing.ownsStorage) return;
    ReleaseStorage(writing.storage);
    writing.ownsStorage = false;
}

std::uint16_t TextSourceMagic(const FontTextSource* source) {
    return static_cast<std::uint16_t>(source->systemUse0 & 0xFFFFu);
}

bool IsWhitespaceCode(std::uint32_t code) {
    switch (code) {
    case 0x0009: case 0x000A: case 0x000B: case 0x000C: case 0x000D: case 0x0020: case 0x0085:
    case 0x00A0: case 0x1680: case 0x2028: case 0x2029: case 0x202F: case 0x205F: case 0x3000:
        return true;
    default:
        return code >= 0x2000 && code <= 0x200A;
    }
}

bool IsFormatCode(std::uint32_t code) {
    switch (code) {
    case 0x00AD: case 0x061C: case 0x200B: case 0x200C: case 0x200D: case 0x200E: case 0x200F:
    case 0x2060: case 0x2061: case 0x2062: case 0x2063: case 0x2064: case 0x2066: case 0x2067:
    case 0x2068: case 0x2069: case 0xFE0E: case 0xFE0F: case 0xFEFF:
        return true;
    default:
        return false;
    }
}

FontTextCharacter MakeCharacter(FontHandle font, std::uint32_t code, void* textOrder) {
    FontTextCharacter character{};
    character.textOrder = textOrder;
    character.font = font;
    character.characterCode = code;
    character.clusterSpan = 1;
    character.flags = static_cast<std::uint64_t>(IsWhitespaceCode(code) ? 0x0E : 0) << 8;
    if (IsFormatCode(code)) character.flags |= CHARACTER_FLAG_FORMAT;
    return character;
}

void LinkCharacters(std::vector<FontTextCharacter>& characters) {
    for (std::size_t i = 0; i < characters.size(); ++i) {
        characters[i].prev = i == 0 ? nullptr : &characters[i - 1];
        characters[i].next = i + 1 < characters.size() ? &characters[i + 1] : nullptr;
    }
}

FontTextCharacter* TextCharacters(FontStringData* data) {
    return data && data->storage && !data->storage->characters.empty() ? data->storage->characters.data() : nullptr;
}

std::uint32_t LevelOf(const FontTextCharacter* character) {
    return static_cast<std::uint32_t>((character->flags >> 24) & 0xFFu);
}

void FillWritingMetricsFromGlyph(FontWritingData& writing, const FontGlyphMetrics& glyphMetrics) {
    writing.rawGlyphMetrics = glyphMetrics;
    writing.hasRawGlyphMetrics = true;
    writing.step.advanceX = glyphMetrics.Horizontal.advance;
    writing.step.advanceY = glyphMetrics.Vertical.advance;
    writing.step.GlyphMetrics = glyphMetrics;
    writing.step.Positioning.x = writing.step.x;
    writing.step.Positioning.y = writing.step.y;
    writing.step.GlyphMetrics.width = writing.step.x;
    writing.step.GlyphMetrics.height = writing.step.y;
}

bool PopulateWritingStep(FontWritingData& writing, std::size_t index) {
    if (!writing.storage || index >= writing.storage->characters.size()) return false;
    const FontTextCharacter& character = writing.storage->characters[index];
    const FontHandle font = character.font ? character.font : writing.defaultFont;
    writing.step.x = writing.penX;
    writing.step.y = writing.penY;
    writing.step.advanceX = 0.0f;
    writing.step.advanceY = 0.0f;
    writing.step.font = font;
    writing.step.Profile = {};
    writing.step.Profile.characterCount = 1;
    writing.step.glyphCode = character.characterCode;
    writing.step.Positioning.x = 0.0f;
    writing.step.Positioning.y = 0.0f;
    writing.step.GlyphMetrics = {};
    writing.rawGlyphMetrics = {};
    writing.hasRawGlyphMetrics = false;
    if ((character.flags & CHARACTER_FLAG_FORMAT) != 0 && writing.maskFormatCharacters) writing.step.Profile.invisibleGlyph = 1;
    if (font && character.characterCode != 0) {
        FontGlyphMetrics glyphMetrics{};
        int rc = GetCharGlyphMetrics(font, character.characterCode, &glyphMetrics, true);
        if (rc != SCE_FONT_OK) rc = GetCharGlyphMetrics(font, character.characterCode, &glyphMetrics, false);
        if (rc == SCE_FONT_OK) FillWritingMetricsFromGlyph(writing, glyphMetrics);
    }
    writing.penX += writing.step.advanceX;
    writing.penY += writing.step.advanceY;
    return true;
}

void AccumulateWritingMetrics(FontWritingData& writing) {
    const FontWritingMetrics previous = writing.metrics;
    const FontGlyphMetrics& glyphMetrics = writing.hasRawGlyphMetrics ? writing.rawGlyphMetrics : writing.step.GlyphMetrics;
    const float left = writing.step.x + glyphMetrics.Horizontal.bearingX;
    const float right = left + glyphMetrics.width;
    const float top = writing.step.y - glyphMetrics.Horizontal.bearingY;
    const float bottom = top + glyphMetrics.height;
    writing.metrics.advanceX = writing.step.x + writing.step.advanceX;
    writing.metrics.advanceY = writing.step.y + writing.step.advanceY;
    if (!writing.hasAggregateMetrics) {
        writing.metrics.Extent = {top, bottom, left, right};
        writing.hasAggregateMetrics = true;
        return;
    }
    writing.metrics.Extent.left = std::min(previous.Extent.left, left);
    writing.metrics.Extent.right = std::max(previous.Extent.right, right);
    writing.metrics.Extent.top = std::min(previous.Extent.top, top);
    writing.metrics.Extent.bottom = std::max(previous.Extent.bottom, bottom);
}

}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontTextSourceInit(FontTextSource* textSource, const void* textAddress, std::uint32_t textSizeByte, FontTextParseFunction textParser, void* textObject) {
    if (!textSource) return SCE_FONT_ERROR_INVALID_PARAMETER;
    std::memset(textSource, 0, sizeof(*textSource));
    if (!textParser) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const void* end = textSizeByte != 0 ? static_cast<const std::uint8_t*>(textAddress) + textSizeByte : nullptr;
    textSource->systemUse0 = (static_cast<std::uint64_t>(WRITING_FORM_HORIZONTAL) << 32) | TEXT_SOURCE_MAGIC;
    textSource->start = textAddress;
    textSource->end = end;
    textSource->current = textAddress;
    textSource->textParser = textParser;
    textSource->textObject = textObject;
    textSource->defaultFont = nullptr;
    textSource->systemUse[0] = const_cast<void*>(textAddress);
    textSource->systemUse[1] = const_cast<void*>(end);
    textSource->systemUse[2] = reinterpret_cast<void*>(textParser);
    textSource->systemUse[3] = textObject;
    textSource->systemUse[4] = nullptr;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontTextSourceRewind(FontTextSource* textSource) {
    if (!textSource || TextSourceMagic(textSource) != TEXT_SOURCE_MAGIC) return SCE_FONT_ERROR_INVALID_PARAMETER;
    textSource->start = textSource->systemUse[0];
    textSource->end = textSource->systemUse[1];
    textSource->current = textSource->systemUse[0];
    textSource->textParser = reinterpret_cast<FontTextParseFunction>(textSource->systemUse[2]);
    textSource->textObject = textSource->systemUse[3];
    textSource->defaultFont = static_cast<FontHandle>(textSource->systemUse[4]);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontTextSourceSetDefaultFont(FontTextSource* textSource, FontHandle defaultFont) {
    if (!textSource || TextSourceMagic(textSource) != TEXT_SOURCE_MAGIC) return SCE_FONT_ERROR_INVALID_PARAMETER;
    textSource->defaultFont = defaultFont;
    textSource->systemUse[4] = defaultFont;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontTextSourceSetWritingForm(FontTextSource* textSource, std::int32_t writingForm) {
    if (!textSource || TextSourceMagic(textSource) != TEXT_SOURCE_MAGIC) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (writingForm < static_cast<std::int32_t>(WRITING_FORM_HORIZONTAL) || writingForm > static_cast<std::int32_t>(WRITING_FORM_HORIZONTAL_LTR)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    textSource->systemUse0 = (textSource->systemUse0 & 0xFFFFFFFFull) | (static_cast<std::uint64_t>(writingForm) << 32);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontCreateString(const FontMemory* fontMemory, FontTextSource* textSource, const FontCreateStringDetail* detail, FontString* pFontString) {
    if (!fontMemory || !textSource || !pFontString || !textSource->textParser) {
        if (pFontString) *pFontString = nullptr;
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    *pFontString = nullptr;
    if (TextSourceMagic(textSource) != TEXT_SOURCE_MAGIC) return SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
    if (fontMemory->mem_kind != MEMORY_MAGIC || !fontMemory->iface || !fontMemory->iface->alloc || !fontMemory->iface->dealloc) return SCE_FONT_ERROR_INVALID_MEMORY;
    if (detail) {
        constexpr std::uint32_t validOrders = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 31);
        if (detail->detailId != CREATE_STRING_DETAIL_ID || detail->detailType > 1 || (detail->detections & ~0x07u) != 0 || (detail->ordersOption & ~validOrders) != 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    void* raw = fontMemory->iface->alloc(fontMemory->mspace_handle, sizeof(FontStringData));
    if (!raw) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    auto* data = new (raw) FontStringData{};
    data->memory = fontMemory;
    data->source = textSource;
    data->defaultFont = textSource->defaultFont;
    data->storage = new CharacterStorage{};
    if (!data->defaultFont && detail) data->defaultFont = detail->defaultFont;
    data->writingForm = static_cast<std::uint32_t>(textSource->systemUse0 >> 32);
    std::int32_t state = SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
    void* textOrder = nullptr;
    for (std::size_t parsed = 0; parsed < MAX_PARSED_CHARACTERS; ++parsed) {
        FontTextParseResult result{};
        state = textSource->textParser(textSource, &textOrder, &result);
        if (state == PARSER_RESULT_FONT_CODE) {
            const FontHandle font = result.FontCode.font ? result.FontCode.font : data->defaultFont;
            if (!font) {
                state = SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
                break;
            }
            data->storage->characters.push_back(MakeCharacter(font, result.FontCode.code, textOrder));
            continue;
        }
        if (state == PARSER_RESULT_TERMINATE) {
            data->terminateOrder = textOrder;
            data->terminateCode = result.Terminate.terminateCode;
            state = SCE_FONT_OK;
        } else if (state == PARSER_RESULT_ERROR) {
            state = result.Error.errorCode < 0 ? result.Error.errorCode : SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
        } else {
            state = SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
        }
        break;
    }
    if (state == PARSER_RESULT_FONT_CODE) state = SCE_FONT_ERROR_INVALID_TEXT_SOURCE;
    if (state != SCE_FONT_OK) {
        ReleaseStorage(data->storage);
        data->~FontStringData();
        fontMemory->iface->dealloc(fontMemory->mspace_handle, raw);
        return state;
    }
    LinkCharacters(data->storage->characters);
    data->textCount = static_cast<std::uint32_t>(data->storage->characters.size());
    data->renderCount = data->textCount;
    *pFontString = reinterpret_cast<FontString>(data);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontDestroyString(FontString* pFontString) {
    if (!pFontString) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* data = GetStringData(*pFontString);
    if (!data) {
        *pFontString = nullptr;
        return SCE_FONT_ERROR_INVALID_STRING;
    }
    const FontMemory* memory = data->memory;
    ReleaseStorage(data->storage);
    data->~FontStringData();
    memory->iface->dealloc(memory->mspace_handle, data);
    *pFontString = nullptr;
    return SCE_FONT_OK;
}

std::uint32_t APS5_VABI sceFontStringGetTerminateCode(FontString fontString) {
    const auto* data = GetStringData(fontString);
    return data ? data->terminateCode : 0;
}

void* APS5_VABI sceFontStringGetTerminateOrder(FontString fontString) {
    const auto* data = GetStringData(fontString);
    return data ? data->terminateOrder : nullptr;
}

int APS5_VABI sceFontStringGetWritingForm(FontString fontString) {
    const auto* data = GetStringData(fontString);
    return data ? static_cast<int>(data->writingForm) : 0;
}

FontTextCharacter* APS5_VABI sceFontStringRefersTextCharacters(FontString fontString, std::uint32_t* characterCount) {
    auto* data = GetStringData(fontString);
    if (!data || data->textCount == 0) {
        if (characterCount) *characterCount = 0;
        return nullptr;
    }
    if (characterCount) *characterCount = data->textCount;
    return TextCharacters(data);
}

const FontTextCharacter* APS5_VABI sceFontStringRefersRenderCharacters(FontString fontString, FontTextCharacter* startCharacter, FontTextCharacter* lastCharacter, std::uint32_t* characterCount) {
    auto* data = GetStringData(fontString);
    if (!data || !characterCount || data->renderCount == 0) {
        if (characterCount) *characterCount = 0;
        return nullptr;
    }
    FontTextCharacter* characters = TextCharacters(data);
    if (!characters) {
        *characterCount = 0;
        return nullptr;
    }
    const FontTextCharacter* begin = characters;
    const FontTextCharacter* end = characters + data->renderCount;
    if (!startCharacter) startCharacter = characters;
    if (startCharacter < begin || startCharacter >= end || (lastCharacter && (lastCharacter < begin || lastCharacter >= end))) {
        *characterCount = 0;
        return nullptr;
    }
    FontTextCharacter* probe = startCharacter;
    if (startCharacter->clusterKind > 1) {
        do {
            startCharacter = probe;
            probe = startCharacter->prev;
            if (!probe) break;
        } while (probe->synthetic < 0);
    }
    auto startFlags = static_cast<std::uint32_t>(startCharacter->flags);
    const std::uint32_t paragraphLevel = (data->writingForm >> 8) & 1u;
    if ((startFlags & 0xFF00u) == 0x100u && startCharacter->prev) {
        if (startCharacter->prev->clusterKind == 1) startCharacter = startCharacter->prev;
        startFlags = static_cast<std::uint32_t>(startCharacter->flags);
    }
    std::uint32_t startLevel = startFlags >> 24;
    std::uint32_t lastLevel = startLevel;
    std::uint32_t minLevel = startLevel;
    std::uint32_t count = startCharacter->clusterIndex == 0 ? 1u : 0u;
    probe = startCharacter;
    while (probe != lastCharacter) {
        probe = probe->next;
        if (!probe) {
            if (lastCharacter) {
                *characterCount = 0;
                return nullptr;
            }
            break;
        }
        if (probe->clusterIndex == 0) {
            lastLevel = LevelOf(probe);
            if (lastLevel < minLevel && ((lastLevel ^ paragraphLevel) & 1u) == 0) minLevel = lastLevel;
            ++count;
        }
    }
    if (startLevel != lastLevel || startLevel != minLevel) {
        probe = startCharacter->prev;
        while (probe) {
            if (probe->clusterIndex == 0) {
                if (LevelOf(probe) <= minLevel) break;
                ++count;
            }
            startCharacter = probe;
            probe = probe->prev;
        }
        if (lastCharacter) {
            probe = lastCharacter->next;
            while (probe) {
                if (probe->clusterIndex == 0) {
                    if (LevelOf(probe) <= minLevel) break;
                    ++count;
                    lastCharacter = probe;
                }
                probe = probe->next;
            }
        }
    }
    if (lastCharacter) {
        probe = lastCharacter->next;
        std::uint32_t clusterTrim = 0;
        while (probe) {
            std::uint32_t nextTrim = clusterTrim;
            if (probe->clusterIndex == 0) {
                if (probe->synthetic == 0) break;
                nextTrim = 0;
                FontTextCharacter* nextLast = probe;
                if (probe->synthetic < 1) {
                    if (probe->clusterKind == 1) break;
                    nextTrim = clusterTrim + 1;
                    nextLast = lastCharacter;
                }
                lastCharacter = nextLast;
                ++count;
            }
            probe = probe->next;
            count -= nextTrim;
            clusterTrim = nextTrim;
        }
    }
    startLevel = LevelOf(startCharacter);
    if (startLevel != paragraphLevel) {
        probe = startCharacter;
        if (!lastCharacter || LevelOf(lastCharacter) != startLevel) {
            do {
                probe = probe->next;
                if (!probe) {
                    if (lastCharacter) startCharacter = nullptr;
                    break;
                }
                FontTextCharacter* best = startCharacter;
                std::uint32_t bestLevel = startLevel;
                if (probe->clusterIndex == 0) {
                    const std::uint32_t probeLevel = LevelOf(probe);
                    if (probeLevel <= paragraphLevel) break;
                    if ((startLevel & 1u) == paragraphLevel && probeLevel < startLevel && (probeLevel & 1u) != paragraphLevel) {
                        best = probe;
                        bestLevel = probeLevel;
                    }
                }
                startLevel = bestLevel;
                startCharacter = best;
            } while (probe != lastCharacter);
        } else if ((LevelOf(startCharacter) & 1u) != paragraphLevel) {
            startCharacter = lastCharacter;
        }
    }
    if (!startCharacter) count = 0;
    *characterCount = count;
    return startCharacter;
}

int APS5_VABI sceFontCharacterGetBidiLevel(const FontTextCharacter* textCharacter, int* bidiLevel) {
    if (!textCharacter || !bidiLevel) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *bidiLevel = static_cast<int>(LevelOf(textCharacter));
    return SCE_FONT_OK;
}

int APS5_VABI sceFontCharacterGetTextFontCode(const FontTextCharacter* textCharacter, FontHandle* pFontHandle, std::uint32_t* textCode) {
    if (!textCharacter) {
        if (pFontHandle) *pFontHandle = nullptr;
        if (textCode) *textCode = 0;
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    if (!pFontHandle || !textCode) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *pFontHandle = textCharacter->font;
    *textCode = textCharacter->characterCode;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontCharacterGetTextOrder(const FontTextCharacter* textCharacter, void** pTextOrder) {
    if (!pTextOrder) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!textCharacter) {
        *pTextOrder = nullptr;
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    *pTextOrder = textCharacter->textOrder;
    return SCE_FONT_OK;
}

std::uint32_t APS5_VABI sceFontCharacterLooksFormatCharacters(const FontTextCharacter* textCharacter) {
    if (!textCharacter) return 0;
    return (textCharacter->flags & CHARACTER_FLAG_FORMAT) != 0 ? textCharacter->characterCode : 0;
}

std::uint32_t APS5_VABI sceFontCharacterLooksWhiteSpace(const FontTextCharacter* textCharacter) {
    if (!textCharacter) return 0;
    return ((textCharacter->flags >> 8) & 0xFFu) == 0x0E ? textCharacter->characterCode : 0;
}

FontTextCharacter* APS5_VABI sceFontCharacterRefersTextBack(const FontTextCharacter* textCharacter) {
    if (!textCharacter) return nullptr;
    for (FontTextCharacter* current = textCharacter->prev; current; current = current->prev) {
        if (current->synthetic == 0 && current->clusterIndex == 0) return current;
    }
    return nullptr;
}

FontTextCharacter* APS5_VABI sceFontCharacterRefersTextNext(const FontTextCharacter* textCharacter) {
    if (!textCharacter) return nullptr;
    for (FontTextCharacter* current = textCharacter->next; current; current = current->next) {
        if (current->synthetic == 0 && current->clusterIndex == 0) return current;
    }
    return nullptr;
}

int APS5_VABI sceFontCharacterGetSyllableStringState(const FontTextCharacter* textCharacter, int* syllableStringState) {
    if (!textCharacter || !syllableStringState) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const FontTextCharacter* const neighbourhood[] = {textCharacter->prev, textCharacter, textCharacter->next};
    for (const FontTextCharacter* character : neighbourhood) {
        if (character && !IsSyllableFreeCode(character->characterCode)) Unsupported(__func__, "syllable detection next to " + CodeName(character->characterCode) + " is not modelled");
    }
    *syllableStringState = SYLLABLE_STRING_NON_DETECTION;
    return SCE_FONT_OK;
}

FontTextCodes* APS5_VABI sceFontCharactersRefersTextCodes(const FontTextCharacter* textCharacter, const FontTextCharacter* termCharacter, FontTextCodes* textCodes) {
    if (!textCodes || !textCharacter) return nullptr;
    if (textCharacter == termCharacter) Unsupported(__func__, "a start character equal to the term character is not modelled");
    std::memset(textCodes, 0, sizeof(*textCodes));
    return SetTextCodesStep(textCodes, const_cast<FontTextCharacter*>(textCharacter), termCharacter);
}

FontTextCodes* APS5_VABI sceFontTextCodesStepNext(FontTextCodes* textCodesStep) {
    if (!textCodesStep) return nullptr;
    const TextCodesState state = GetTextCodesStep(__func__, textCodesStep);
    FontTextCharacter* next = state.current->next;
    if (!next || next == state.term) return nullptr;
    return SetTextCodesStep(textCodesStep, next, state.term);
}

FontTextCodes* APS5_VABI sceFontTextCodesStepBack(FontTextCodes* textCodesStep) {
    if (!textCodesStep) return nullptr;
    const TextCodesState state = GetTextCodesStep(__func__, textCodesStep);
    FontTextCharacter* previous = state.current->prev;
    if (!previous) return nullptr;
    return SetTextCodesStep(textCodesStep, previous, state.term);
}

int APS5_VABI sceFontWritingInit(FontWriting* fontWriting, FontString fontString, const FontTextCharacter* fontCharacter) {
    if (!fontWriting || !fontString || !fontCharacter) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* data = GetStringData(fontString);
    if (!data) return SCE_FONT_ERROR_INVALID_STRING;
    if (auto* previous = GetWritingData(fontWriting)) ReleaseWritingStorage(*previous);
    std::memset(fontWriting, 0, sizeof(*fontWriting));
    auto* writing = new (fontWriting->systemUse) FontWritingData{};
    writing->string = fontString;
    writing->storage = data->storage;
    writing->defaultFont = data->defaultFont;
    if (writing->storage) writing->storage->refCount.fetch_add(1, std::memory_order_relaxed);
    writing->ownsStorage = true;
    writing->writingForm = data->writingForm;
    const FontTextCharacter* characters = TextCharacters(data);
    if (!characters || fontCharacter < characters || fontCharacter >= characters + data->storage->characters.size()) {
        ReleaseWritingStorage(*writing);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    writing->nextIndex = static_cast<std::size_t>(fontCharacter - characters);
    writing->currentIndex = writing->nextIndex;
    writing->hasCurrentStep = false;
    if (!PopulateWritingStep(*writing, writing->nextIndex)) {
        ReleaseWritingStorage(*writing);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    writing->penX = 0.0f;
    writing->penY = 0.0f;
    writing->hasAggregateMetrics = false;
    writing->metrics = {};
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingGetRenderMetrics(FontWriting* fontWriting, FontWritingMetrics* writingMetrics) {
    if (!fontWriting || !writingMetrics) {
        if (writingMetrics) *writingMetrics = {};
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    const auto* writing = GetWritingData(fontWriting);
    if (!writing) {
        *writingMetrics = {};
        return SCE_FONT_ERROR_INVALID_WRITING;
    }
    *writingMetrics = writing->metrics;
    return SCE_FONT_OK;
}

const FontWritingStep* APS5_VABI sceFontWritingRefersRenderStep(FontWriting* fontWriting) {
    auto* writing = GetWritingData(fontWriting);
    if (!writing || !writing->storage || writing->nextIndex >= writing->storage->characters.size()) return nullptr;
    if (!PopulateWritingStep(*writing, writing->nextIndex)) return nullptr;
    writing->currentIndex = writing->nextIndex;
    writing->hasCurrentStep = true;
    AccumulateWritingMetrics(*writing);
    ++writing->nextIndex;
    return &writing->step;
}

FontTextCharacter* APS5_VABI sceFontWritingRefersRenderStepCharacter(FontWriting* fontWriting, FontWritingLetterStep* letterStep) {
    if (letterStep) std::memset(letterStep, 0, sizeof(*letterStep));
    auto* writing = GetWritingData(fontWriting);
    if (!writing || !writing->hasCurrentStep || !writing->storage || writing->currentIndex >= writing->storage->characters.size()) return nullptr;
    if (letterStep) {
        letterStep->x = writing->step.x;
        letterStep->y = writing->step.y;
        letterStep->advanceX = writing->step.advanceX;
        letterStep->advanceY = writing->step.advanceY;
        letterStep->Components.textsCount = 1;
        letterStep->Components.textsIndex = static_cast<std::uint32_t>(writing->currentIndex);
        letterStep->Components.glyphsCount = 1;
        letterStep->Components.glyphsIndex = static_cast<std::uint32_t>(writing->currentIndex);
        letterStep->Components.characterTextCount = static_cast<std::uint8_t>(std::max<std::uint32_t>(writing->step.Profile.characterCount, 1));
    }
    return &writing->storage->characters[writing->currentIndex];
}

int APS5_VABI sceFontWritingSetMaskInvisible(FontWriting* fontWriting, std::int32_t mask) {
    auto* writing = GetWritingData(fontWriting);
    if (!writing) return SCE_FONT_ERROR_INVALID_WRITING;
    if (mask == WRITING_MASK_FORMAT_CHARACTERS) {
        writing->maskFormatCharacters = false;
        return SCE_FONT_OK;
    }
    if (mask == WRITING_MASK_NONE) {
        writing->maskFormatCharacters = true;
        return SCE_FONT_OK;
    }
    return SCE_FONT_ERROR_INVALID_PARAMETER;
}

}

#pragma GCC visibility pop
