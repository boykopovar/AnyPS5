#include "prx/libSceFont/libSceFont.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font words check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct AllocationState {
    std::size_t live = 0;
    bool fail = false;
};

static void* APS5_VABI Allocate(void* object, std::uint32_t size) {
    auto& state = *static_cast<AllocationState*>(object);
    if (state.fail) return nullptr;
    void* allocation = std::malloc(size == 0 ? 1 : size);
    if (allocation) ++state.live;
    return allocation;
}

static void APS5_VABI Deallocate(void* object, void* allocation) {
    auto& state = *static_cast<AllocationState*>(object);
    Require(allocation && state.live > 0);
    --state.live;
    std::free(allocation);
}

struct WordText {
    std::vector<std::uint32_t> codes;
    std::size_t position = 0;
    std::size_t parses = 0;
    std::int32_t errorCode = 0;
};

static std::int32_t APS5_VABI ParseWords(FontTextSource* source, void** order, FontTextParseResult* result) {
    auto& text = *static_cast<WordText*>(source->textObject);
    ++text.parses;
    if (text.errorCode != 0 && text.position == 1) {
        result->Error.errorCode = text.errorCode;
        return -1;
    }
    const std::uint32_t code = text.position < text.codes.size() ? text.codes[text.position] : 0;
    *order = reinterpret_cast<void*>(text.position);
    ++text.position;
    if (code == 0 || code == ' ') {
        result->Terminate.terminateCode = code;
        return 0;
    }
    result->FontCode.code = code;
    return 1;
}

static int APS5_VABI RewindWords(FontTextSource* source) {
    static_cast<WordText*>(source->textObject)->position = 0;
    return SCE_FONT_OK;
}

static WordText Text(const std::string& value) {
    WordText text;
    for (const char c : value) text.codes.push_back(static_cast<unsigned char>(c));
    return text;
}

struct Fixture {
    AllocationState allocations;
    FontMemoryInterface iface{Allocate, Deallocate, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};

    Fixture() { Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, &allocations, nullptr, nullptr) == SCE_FONT_OK); }

    int Create(WordText& text, FontWords** words) {
        FontTextSource source{};
        Require(sceFontTextSourceInit(&source, nullptr, 0, ParseWords, &text) == SCE_FONT_OK);
        text.position = 0;
        const int rc = sceFontCreateWords(&memory, &source, 0, words);
        RewindWords(&source);
        return rc;
    }
};

struct Characters {
    std::vector<FontTextCharacter> items;

    explicit Characters(const std::string& value, std::uint64_t flags = 0) : items(value.size()) {
        for (std::size_t i = 0; i < value.size(); ++i) {
            items[i].characterCode = static_cast<unsigned char>(value[i]);
            items[i].clusterSpan = 1;
            items[i].flags = flags;
        }
        Link();
    }

    void Link() {
        for (std::size_t i = 0; i < items.size(); ++i) {
            items[i].prev = i == 0 ? nullptr : &items[i - 1];
            items[i].next = i + 1 < items.size() ? &items[i + 1] : nullptr;
        }
    }

    FontTextCharacter* operator[](std::size_t index) { return &items[index]; }
};

static FontTextCharacter* Find(FontWords* words, FontTextCharacter* first, FontTextCharacter** next = nullptr, FontTextCharacter* end = nullptr) {
    FontTextCharacter* last = reinterpret_cast<FontTextCharacter*>(1);
    FontTextCharacter* following = reinterpret_cast<FontTextCharacter*>(1);
    Require(sceFontWordsFindWordCharacters(words, first, end, &last, &following) == SCE_FONT_OK);
    if (next) *next = following;
    return last;
}

static void CreateValidation() {
    Fixture fixture;
    WordText text = Text("an");
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, nullptr, 0, ParseWords, &text) == SCE_FONT_OK);
    FontWords* words = reinterpret_cast<FontWords*>(1);
    Require(sceFontCreateWords(nullptr, &source, 0, &words) == SCE_FONT_ERROR_INVALID_PARAMETER && words == nullptr);
    FontMemory badMemory{};
    words = reinterpret_cast<FontWords*>(1);
    Require(sceFontCreateWords(&badMemory, &source, 0, &words) == SCE_FONT_ERROR_INVALID_MEMORY && words == nullptr);
    Require(sceFontCreateWords(&fixture.memory, nullptr, 0, &words) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontCreateWords(&fixture.memory, &source, 0, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    FontTextSource unknown = source;
    unknown.systemUse0 = 0;
    Require(sceFontCreateWords(&fixture.memory, &unknown, 0, &words) == SCE_FONT_ERROR_INVALID_TEXT_SOURCE && words == nullptr);
    Require(sceFontTextSourceRewind(&unknown) == SCE_FONT_ERROR_INVALID_TEXT_SOURCE);
    Require(sceFontTextSourceSetDefaultFont(&unknown, nullptr) == SCE_FONT_ERROR_INVALID_TEXT_SOURCE);
    Require(sceFontTextSourceSetWritingForm(&unknown, 0x10) == SCE_FONT_ERROR_INVALID_TEXT_SOURCE);
    Require(sceFontTextSourceRewind(nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontTextSourceSetWritingForm(&source, 0x13) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(text.parses == 0);
    Require(fixture.allocations.live == 0);
}

static void CreateFailures() {
    Fixture fixture;
    FontWords* words = reinterpret_cast<FontWords*>(1);
    WordText unsorted = Text("be an");
    Require(fixture.Create(unsorted, &words) == SCE_FONT_ERROR_NO_SUPPORT_CODE && words == nullptr);
    WordText sibling = Text("abc abb");
    Require(fixture.Create(sibling, &words) == SCE_FONT_ERROR_NO_SUPPORT_CODE && words == nullptr);
    WordText prefixThenSmaller = Text("abc ab abb");
    Require(fixture.Create(prefixThenSmaller, &words) == SCE_FONT_ERROR_NO_SUPPORT_CODE && words == nullptr);
    WordText negative;
    negative.codes = {'a', 0x80000000u};
    Require(fixture.Create(negative, &words) == SCE_FONT_ERROR_NO_SUPPORT_CODE && words == nullptr);
    WordText failing = Text("an");
    failing.errorCode = static_cast<std::int32_t>(0x80460099);
    Require(fixture.Create(failing, &words) == static_cast<int>(0x80460099) && words == nullptr);
    failing = Text("an");
    failing.errorCode = 5;
    Require(fixture.Create(failing, &words) == SCE_FONT_ERROR_INVALID_TEXT_SOURCE && words == nullptr);
    WordText valid = Text("an");
    fixture.allocations.fail = true;
    Require(fixture.Create(valid, &words) == SCE_FONT_ERROR_ALLOCATION_FAILED && words == nullptr);
    fixture.allocations.fail = false;
    Require(fixture.allocations.live == 0);
}

static void CreateAndDestroy() {
    Fixture fixture;
    WordText text = Text("an and ant be");
    FontWords* words = nullptr;
    Require(fixture.Create(text, &words) == SCE_FONT_OK && words != nullptr);
    Require(fixture.allocations.live == 2);
    FontWords* copy = words;
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK && words == nullptr);
    Require(fixture.allocations.live == 0);
    Require(sceFontDestroyWords(&words) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontDestroyWords(nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    std::uint64_t garbage[8]{};
    copy = reinterpret_cast<FontWords*>(garbage);
    Require(sceFontDestroyWords(&copy) == SCE_FONT_ERROR_INVALID_WORDS && copy != nullptr);
    WordText empty;
    Require(fixture.Create(empty, &words) == SCE_FONT_OK && words != nullptr);
    Characters chars("an");
    Require(Find(words, chars[0]) == nullptr);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
    Require(fixture.allocations.live == 0);
}

static void FindValidation() {
    Fixture fixture;
    WordText text = Text("an");
    FontWords* words = nullptr;
    Require(fixture.Create(text, &words) == SCE_FONT_OK);
    Characters chars("an");
    FontTextCharacter* last = chars[0];
    FontTextCharacter* next = chars[0];
    Require(sceFontWordsFindWordCharacters(nullptr, chars[0], nullptr, &last, &next) == SCE_FONT_ERROR_INVALID_PARAMETER && last == nullptr && next == nullptr);
    next = chars[0];
    Require(sceFontWordsFindWordCharacters(words, nullptr, nullptr, &last, &next) == SCE_FONT_ERROR_INVALID_PARAMETER && next == nullptr);
    Require(sceFontWordsFindWordCharacters(words, chars[0], nullptr, nullptr, &next) == SCE_FONT_ERROR_INVALID_PARAMETER);
    std::uint64_t garbage[8]{};
    last = chars[0];
    Require(sceFontWordsFindWordCharacters(reinterpret_cast<FontWords*>(garbage), chars[0], nullptr, &last, nullptr) == SCE_FONT_ERROR_INVALID_WORDS && last == nullptr);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
}

static void LongestMatch() {
    Fixture fixture;
    WordText text = Text("an and ant be");
    FontWords* words = nullptr;
    Require(fixture.Create(text, &words) == SCE_FONT_OK);
    Characters andy("andy");
    FontTextCharacter* next = nullptr;
    Require(Find(words, andy[0], &next) == andy[2] && next == andy[3]);
    Characters ant("ant");
    Require(Find(words, ant[0], &next) == ant[2] && next == nullptr);
    Characters ab("ab");
    Require(Find(words, ab[0], &next) == nullptr && next == nullptr);
    Characters ba("ba");
    Require(Find(words, ba[0]) == nullptr);
    Characters band("and");
    Require(Find(words, band[0], &next, band[2]) == band[1] && next == band[2]);
    Characters empty("be");
    Require(Find(words, empty[0], &next, empty[0]) == empty[1] && next == nullptr);
    Characters joined("andy");
    joined.items[3].flags = std::uint64_t{1} << 43;
    Require(Find(words, joined[0]) == joined[1]);
    joined.items[3].flags = std::uint64_t{3} << 43;
    Require(Find(words, joined[0]) == joined[2]);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
}

static void Clusters() {
    Fixture fixture;
    WordText text = Text("ab abc");
    FontWords* words = nullptr;
    Require(fixture.Create(text, &words) == SCE_FONT_OK);
    Characters split("abcd");
    split.items[2].clusterIndex = 1;
    FontTextCharacter* next = nullptr;
    Require(Find(words, split[0], &next) == split[1] && next == split[3]);
    Characters marked("abxc");
    marked.items[2].synthetic = 1;
    Require(Find(words, marked[0], &next) == marked[3] && next == nullptr);
    marked.items[2].synthetic = -1;
    marked.items[2].clusterKind = 1;
    Require(Find(words, marked[0]) == marked[3]);
    Characters lead("abxc");
    lead.items[2].synthetic = -1;
    lead.items[2].clusterKind = 2;
    lead.items[3].characterCode = 'z';
    Require(Find(words, lead[0]) == nullptr);
    lead.items[2].clusterKind = 1;
    Require(Find(words, lead[0], &next) == lead[1] && next == lead[3]);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
}

static void SegmentedRuns() {
    Fixture fixture;
    WordText text = Text("an ant be tea");
    FontWords* words = nullptr;
    Require(fixture.Create(text, &words) == SCE_FONT_OK);
    Characters following("antbe", 1);
    Require(Find(words, following[0]) == following[2]);
    Characters shorter("antea", 1);
    Require(Find(words, shorter[0]) == shorter[1]);
    Characters fallback("antx", 1);
    Require(Find(words, fallback[0]) == fallback[2]);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);

    WordText overlapping = Text("ab bcd");
    Require(fixture.Create(overlapping, &words) == SCE_FONT_OK);
    Characters overrun("abcd", 1);
    overrun.items[0].flags = 0;
    FontTextCharacter* next = overrun[0];
    Require(Find(words, overrun[0], &next) == nullptr && next == nullptr);
    Characters contained("abcd", 1);
    Require(Find(words, contained[0]) == contained[1]);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
    Require(fixture.allocations.live == 0);
}

static void DepthLimit() {
    Fixture fixture;
    WordText text = Text(std::string(129, 'a') + " b");
    FontWords* words = nullptr;
    Require(fixture.Create(text, &words) == SCE_FONT_OK);
    Characters longWord(std::string(129, 'a'));
    Require(Find(words, longWord[0]) == nullptr);
    Characters later("b");
    Require(Find(words, later[0]) == nullptr);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
    WordText exact = Text(std::string(128, 'a') + " b");
    Require(fixture.Create(exact, &words) == SCE_FONT_OK);
    Characters fullWord(std::string(128, 'a'));
    Require(Find(words, fullWord[0]) == fullWord[127]);
    Require(Find(words, later[0]) == later[0]);
    Require(sceFontDestroyWords(&words) == SCE_FONT_OK);
}

int main() {
    CreateValidation();
    CreateFailures();
    CreateAndDestroy();
    FindValidation();
    LongestMatch();
    Clusters();
    SegmentedRuns();
    DepthLimit();
    std::puts("Font words tests passed");
    return 0;
}
