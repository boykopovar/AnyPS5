#include "prx/libSceFont/include/FontTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

extern "C" {
int APS5_VABI sceFontMemoryInit(FontMemory*, void*, std::uint32_t, const FontMemoryInterface*, void*, FontMemoryDestroyFunction, void*);
int APS5_VABI sceFontMemoryTerm(FontMemory*);
int APS5_VABI sceFontTextSourceInit(FontTextSource*, const void*, std::uint32_t, FontTextParseFunction, void*);
int APS5_VABI sceFontTextSourceSetDefaultFont(FontTextSource*, FontHandle);
int APS5_VABI sceFontCreateString(const FontMemory*, FontTextSource*, const FontCreateStringDetail*, FontString*);
int APS5_VABI sceFontDestroyString(FontString*);
FontTextCharacter* APS5_VABI sceFontStringRefersTextCharacters(FontString, std::uint32_t*);
FontTextCodes* APS5_VABI sceFontCharactersRefersTextCodes(const FontTextCharacter*, const FontTextCharacter*, FontTextCodes*);
FontTextCodes* APS5_VABI sceFontTextCodesStepNext(FontTextCodes*);
FontTextCodes* APS5_VABI sceFontTextCodesStepBack(FontTextCodes*);
int APS5_VABI sceFontCharacterGetSyllableStringState(const FontTextCharacter*, int*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font text check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename F>
static bool Throws(F f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

static std::int32_t APS5_VABI ParseCodes(FontTextSource* source, void** order, FontTextParseResult* result) {
    const auto* current = static_cast<const std::uint32_t*>(source->current);
    *order = const_cast<std::uint32_t*>(current);
    if (current == source->end) {
        result->Terminate.terminateCode = 0;
        return 0;
    }
    result->FontCode.font = nullptr;
    result->FontCode.code = *current;
    source->current = current + 1;
    return 1;
}

struct Text {
    FontString string = nullptr;
    FontTextCharacter* characters = nullptr;
    std::uint32_t count = 0;
};

static Text CreateText(const FontMemory& memory, const std::vector<std::uint32_t>& codes, FontHandle font) {
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, codes.data(), static_cast<std::uint32_t>(codes.size() * sizeof(std::uint32_t)), ParseCodes, nullptr) == SCE_FONT_OK);
    Require(sceFontTextSourceSetDefaultFont(&source, font) == SCE_FONT_OK);
    Text text;
    Require(sceFontCreateString(&memory, &source, nullptr, &text.string) == SCE_FONT_OK && text.string != nullptr);
    text.characters = sceFontStringRefersTextCharacters(text.string, &text.count);
    Require(text.characters != nullptr && text.count == codes.size());
    return text;
}

static bool StepIs(const FontTextCodes* step, const FontTextCodes* codes, std::uint32_t code, const std::uint32_t* order) {
    return step == codes && step->textCode == code && step->textOrder == order;
}

static bool StateIs(const FontTextCharacter* character, int expected) {
    int state = -1;
    return sceFontCharacterGetSyllableStringState(character, &state) == SCE_FONT_OK && state == expected;
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontHandleOpaque fontStorage{};
    const FontHandle font = &fontStorage;

    const std::vector<std::uint32_t> latin{'A', 'b', ' ', 0x00E9, 0x4E00};
    Text text = CreateText(memory, latin, font);
    FontTextCharacter* characters = text.characters;

    FontTextCodes codes{};
    Require(StepIs(sceFontCharactersRefersTextCodes(&characters[0], &characters[3], &codes), &codes, 'A', &latin[0]));
    Require(StepIs(sceFontTextCodesStepNext(&codes), &codes, 'b', &latin[1]));
    Require(StepIs(sceFontTextCodesStepNext(&codes), &codes, ' ', &latin[2]));
    Require(sceFontTextCodesStepNext(&codes) == nullptr && codes.textCode == ' ');
    Require(StepIs(sceFontTextCodesStepBack(&codes), &codes, 'b', &latin[1]));
    Require(StepIs(sceFontTextCodesStepBack(&codes), &codes, 'A', &latin[0]));
    Require(sceFontTextCodesStepBack(&codes) == nullptr && codes.textCode == 'A');
    Require(StepIs(sceFontTextCodesStepNext(&codes), &codes, 'b', &latin[1]));

    Require(StepIs(sceFontCharactersRefersTextCodes(&characters[3], nullptr, &codes), &codes, 0x00E9, &latin[3]));
    Require(StepIs(sceFontTextCodesStepNext(&codes), &codes, 0x4E00, &latin[4]));
    Require(sceFontTextCodesStepNext(&codes) == nullptr);
    Require(StepIs(sceFontTextCodesStepBack(&codes), &codes, 0x00E9, &latin[3]));

    Require(Throws([&] { sceFontCharactersRefersTextCodes(&characters[1], &characters[1], &codes); }) && StepIs(&codes, &codes, 0x00E9, &latin[3]));
    Require(sceFontCharactersRefersTextCodes(nullptr, &characters[1], &codes) == nullptr && StepIs(&codes, &codes, 0x00E9, &latin[3]));
    Require(sceFontCharactersRefersTextCodes(&characters[0], nullptr, nullptr) == nullptr);
    Require(sceFontTextCodesStepNext(nullptr) == nullptr);
    Require(sceFontTextCodesStepBack(nullptr) == nullptr);
    FontTextCodes unset{};
    Require(Throws([&] { sceFontTextCodesStepNext(&unset); }));
    Require(Throws([&] { sceFontTextCodesStepBack(&unset); }));

    for (std::uint32_t i = 0; i < text.count; ++i) Require(StateIs(&characters[i], 0));
    int state = 7;
    Require(sceFontCharacterGetSyllableStringState(nullptr, &state) == SCE_FONT_ERROR_INVALID_PARAMETER && state == 7);
    Require(sceFontCharacterGetSyllableStringState(&characters[0], nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontDestroyString(&text.string) == SCE_FONT_OK && text.string == nullptr);

    const std::vector<std::uint32_t> mixed{'x', 'e', 0x0301, 'y', 'z', 0x0E01, 0x0E35};
    Text marks = CreateText(memory, mixed, font);
    Require(StateIs(&marks.characters[0], 0));
    Require(Throws([&] { sceFontCharacterGetSyllableStringState(&marks.characters[1], &state); }) && state == 7);
    Require(Throws([&] { sceFontCharacterGetSyllableStringState(&marks.characters[2], &state); }));
    Require(Throws([&] { sceFontCharacterGetSyllableStringState(&marks.characters[3], &state); }));
    Require(Throws([&] { sceFontCharacterGetSyllableStringState(&marks.characters[4], &state); }));
    Require(Throws([&] { sceFontCharacterGetSyllableStringState(&marks.characters[5], &state); }));
    Require(Throws([&] { sceFontCharacterGetSyllableStringState(&marks.characters[6], &state); }));
    Require(sceFontDestroyString(&marks.string) == SCE_FONT_OK);

    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    std::puts("font text: ok");
}
