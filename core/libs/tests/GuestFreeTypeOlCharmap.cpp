#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "FreeTypeFontFixture.hpp"
#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {

using namespace FreeType;

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "character map check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(value) Require((value), __LINE__)

struct State {
    GuestFaceRec* face;
    int starts;
    int finishes;
    bool fail;
    bool checkRemoval = false;
    GuestCMap* nested = nullptr;
    bool throwDuringDone = false;
};

struct Map {
    GuestCMap base;
    State* state;
};

int APS5_VABI Init(GuestCMap* map, void* data) {
    auto* state = static_cast<State*>(data);
    REQUIRE(map->charmap.face == state->face);
    REQUIRE(map->charmap.platform_id == 7);
    REQUIRE(reinterpret_cast<Map*>(map)->state == nullptr);
    reinterpret_cast<Map*>(map)->state = state;
    ++state->starts;
    return state->fail ? 0x42 : 0;
}

struct DoneError {};

void APS5_VABI Done(GuestCMap* map) {
    auto& state = *reinterpret_cast<Map*>(map)->state;
    if (state.checkRemoval) {
        REQUIRE(state.face->charmap != &map->charmap);
        for (int i = 0; i < state.face->num_charmaps; ++i) REQUIRE(state.face->charmaps[i] != &map->charmap);
    }
    ++state.finishes;
    if (auto* nested = std::exchange(state.nested, nullptr)) sceFreeTypeOlCMapDone_nid_no_patch(nested);
    if (state.throwDuringDone) throw DoneError{};
}

std::uint32_t APS5_VABI Index(GuestCMap* map, std::uint32_t code) {
    REQUIRE(reinterpret_cast<Map*>(map)->state->starts > 0);
    return code == 'Z' ? 1 : code == 0xff ? 1000 : 0;
}

std::uint32_t APS5_VABI Next(GuestCMap* map, std::uint32_t* code) {
    REQUIRE(reinterpret_cast<Map*>(map)->state->starts > 0);
    if (*code < 'Z') {
        *code = 'Z';
        return 1;
    }
    return 0;
}

struct SfntInfo {
    std::uint64_t language;
    std::int64_t format;
};

struct SfntClass {
    GuestCMapClass base;
    std::uint32_t format;
    void* validate;
    int (APS5_VABI *info)(GuestCharMapRec*, SfntInfo*);
};

int APS5_VABI ReadSfntInfo(GuestCharMapRec* map, SfntInfo* info) {
    const auto* cls = reinterpret_cast<const SfntClass*>(reinterpret_cast<GuestCMap*>(map)->clazz);
    *info = {13, cls->format};
    return 0;
}

std::uint32_t APS5_VABI VariantIndex(GuestCMap* map, GuestCMap* unicode, std::uint32_t code, std::uint32_t selector) {
    auto* face = static_cast<GuestFaceRec*>(map->charmap.face);
    REQUIRE(&unicode->charmap == face->charmap);
    return selector == 0xfe0f ? unicode->clazz->index(unicode, code) : 0;
}

int APS5_VABI VariantDefault(GuestCMap*, std::uint32_t code, std::uint32_t selector) {
    return code == 'A' && selector == 0xfe0f ? 1 : -1;
}

std::uint32_t* APS5_VABI Variants(GuestCMap* map, GuestMemoryRec* memory) {
    auto* face = static_cast<GuestFaceRec*>(map->charmap.face);
    REQUIRE(memory && face->memory == memory);
    auto* block = memory->alloc(memory, 12);
    REQUIRE(block != nullptr);
    block = memory->realloc(memory, 12, 24, block);
    REQUIRE(block != nullptr);
    memory->free(memory, block);
    static std::uint32_t result[]{0xfe0f, 0};
    return result;
}

std::uint32_t* APS5_VABI CharVariants(GuestCMap* map, GuestMemoryRec* memory, std::uint32_t code) {
    return code == 'A' ? Variants(map, memory) : nullptr;
}

std::uint32_t* APS5_VABI VariantChars(GuestCMap*, GuestMemoryRec*, std::uint32_t selector) {
    static std::uint32_t result[]{'A', 0};
    return selector == 0xfe0f ? result : nullptr;
}

FT_Realloc_Func savedRealloc;
int rejectedShrinks = 0;

void* RejectShrink(FT_Memory memory, long oldSize, long newSize, void* block) {
    if (newSize < oldSize) {
        ++rejectedShrinks;
        return nullptr;
    }
    return savedRealloc(memory, oldSize, newSize, block);
}

void CheckRemoval(GuestFaceRec* face, const GuestCMapClass& clazz, const GuestCharMapRec& description) {
    const int initialCount = face->num_charmaps;
    State outer{face, 0, 0, false, true};
    State inner{face, 0, 0, false, true};
    GuestCMap* map = nullptr;
    REQUIRE(!sceFreeTypeOlCMapNew_nid_no_patch(&clazz, &outer, &description, &map));
    REQUIRE(!sceFreeTypeOlCMapNew_nid_no_patch(&clazz, &inner, &description, &outer.nested));
    REQUIRE(!FT_Set_Charmap_nid_postfix(face, &map->charmap));
    auto* owner = static_cast<GuestFace*>(face->internal);
    auto* memory = owner->host->memory;
    savedRealloc = std::exchange(memory->realloc, RejectShrink);
    sceFreeTypeOlCMapDone_nid_no_patch(map);
    memory->realloc = savedRealloc;
    REQUIRE(rejectedShrinks == 1 && !owner->charmapRemoval);
    REQUIRE(outer.finishes == 0 && inner.finishes == 0 && face->num_charmaps == initialCount + 2);
    REQUIRE(face->charmap == &map->charmap && FT_Get_Char_Index_nid_postfix(face, 'Z') == 1);
    inner.throwDuringDone = true;
    bool caught = false;
    try {
        sceFreeTypeOlCMapDone_nid_no_patch(map);
    } catch (const DoneError&) {
        caught = true;
    }
    REQUIRE(caught && !owner->charmapRemoval && face->num_charmaps == initialCount);
    REQUIRE(!face->charmap && outer.finishes == 1 && inner.finishes == 1);
}

void CheckSelector(GuestFaceRec* face) {
    const SfntClass clazz{{sizeof(GuestCMap), nullptr, nullptr, nullptr, nullptr, VariantIndex, VariantDefault,
                               Variants, CharVariants, VariantChars}, 14, nullptr, ReadSfntInfo};
    GuestCharMapRec description{face, 0, 0, 5};
    GuestCMap* map{};
    REQUIRE(!sceFreeTypeOlCMapNew_nid_no_patch(&clazz.base, nullptr, &description, &map));
    REQUIRE(FT_Get_CMap_Format_nid_postfix(&map->charmap) == 14);
    REQUIRE(FT_Get_CMap_Language_ID_nid_postfix(&map->charmap) == 13);
    REQUIRE(FT_Set_Charmap_nid_postfix(face, &map->charmap) != 0);
    REQUIRE(FT_Face_GetCharVariantIndex_nid_postfix(face, 'A', 0xfe0f) == 1);
    REQUIRE(FT_Face_GetCharVariantIndex_nid_postfix(face, 'Z', 0xfe0f) == 0);
    REQUIRE(FT_Face_GetCharVariantIsDefault_nid_postfix(face, 'A', 0xfe0f) == 1);
    REQUIRE(FT_Face_GetCharVariantIsDefault_nid_postfix(face, 'Z', 0xfe0f) == -1);
    REQUIRE(FT_Face_GetVariantSelectors_nid_postfix(face)[0] == 0xfe0f);
    REQUIRE(FT_Face_GetVariantsOfChar_nid_postfix(face, 'A')[0] == 0xfe0f);
    REQUIRE(!FT_Face_GetVariantsOfChar_nid_postfix(face, 'Z'));
    REQUIRE(FT_Face_GetCharsOfVariant_nid_postfix(face, 0xfe0f)[0] == 'A');
    sceFreeTypeOlCMapDone_nid_no_patch(map);
    REQUIRE(!FT_Face_GetVariantSelectors_nid_postfix(face));
}

}

int main() {
    using namespace FreeType;
    GuestLibrary* library = nullptr;
    REQUIRE(!FT_Init_FreeType_nid_postfix(&library));
    GuestFaceRec* face = nullptr;
    const auto bytes = SquareGlyphFont('A', 700, true);
    REQUIRE(!FT_New_Memory_Face_nid_postfix(library, bytes.data(), bytes.size(), 0, &face));
    auto* original = face->charmap;
    const auto count = face->num_charmaps;
    const SfntClass sfntClass{{sizeof(Map), Init, Done, Index, Next}, 4, nullptr, ReadSfntInfo};
    const auto& clazz = sfntClass.base;
    GuestCharMapRec description{face, 0x63757374, 7, 2};
    GuestCMap* const sentinel = reinterpret_cast<GuestCMap*>(1);
    GuestCMap* map = sentinel;
    REQUIRE(sceFreeTypeOlCMapNew_nid_no_patch(nullptr, nullptr, &description, &map) == 6 && map == sentinel);
    REQUIRE(sceFreeTypeOlCMapNew_nid_no_patch(&clazz, nullptr, nullptr, &map) == 6 && map == sentinel);
    GuestCharMapRec noFace{};
    REQUIRE(sceFreeTypeOlCMapNew_nid_no_patch(&clazz, nullptr, &noFace, &map) == 6 && map == sentinel);
    CheckRemoval(face, clazz, description);
    REQUIRE(!FT_Set_Charmap_nid_postfix(face, original));
    State state{face, 0, 0, true};
    REQUIRE(sceFreeTypeOlCMapNew_nid_no_patch(&clazz, &state, &description, &map) == 0x42);
    REQUIRE(!map && state.starts == 1 && state.finishes == 1 && face->num_charmaps == count);
    state.fail = false;
    REQUIRE(!sceFreeTypeOlCMapNew_nid_no_patch(&clazz, &state, &description, &map));
    REQUIRE(map && map->clazz == &clazz && face->num_charmaps == count + 1);
    REQUIRE(face->charmaps[count] == &map->charmap && face->charmap == original);
    REQUIRE(!FT_Set_Charmap_nid_postfix(face, &map->charmap));
    REQUIRE(FT_Get_Charmap_Index_nid_postfix(&map->charmap) == count);
    REQUIRE(FT_Get_CMap_Format_nid_postfix(&map->charmap) == 4);
    REQUIRE(FT_Get_Char_Index_nid_postfix(face, 'A') == 0);
    REQUIRE(FT_Get_Char_Index_nid_postfix(face, 'Z') == 1);
    REQUIRE(FT_Get_Char_Index_nid_postfix(face, 0xff) == 0);
    std::uint32_t index = 99;
    REQUIRE(FT_Get_First_Char_nid_postfix(face, &index) == 'Z' && index == 1);
    REQUIRE(FT_Get_Next_Char_nid_postfix(face, 'Z', &index) == 0 && index == 0);
    REQUIRE(!sceFreeTypeOlCMapNew_nid_no_patch(&clazz, &state, &description, nullptr));
    auto* survivor = face->charmaps[count + 1];
    REQUIRE(face->charmap == &map->charmap);
    state.checkRemoval = true;
    sceFreeTypeOlCMapDone_nid_no_patch(map);
    state.checkRemoval = false;
    REQUIRE(face->num_charmaps == count + 1 && face->charmaps[count] == survivor);
    REQUIRE(!face->charmap && state.finishes == 2);
    REQUIRE(!FT_Set_Charmap_nid_postfix(face, original) && FT_Get_Char_Index_nid_postfix(face, 'A') == 1);
    CheckSelector(face);
    sceFreeTypeOlCMapDone_nid_no_patch(reinterpret_cast<GuestCMap*>(original));
    REQUIRE(!face->charmap && face->num_charmaps == count && face->charmaps[count - 1] == survivor);
    sceFreeTypeOlCMapDone_nid_no_patch(nullptr);
    REQUIRE(!FT_Done_Face_nid_postfix(face));
    REQUIRE(state.starts == 3 && state.finishes == 3);
    const auto bitmap = FontFixture::BitmapFont();
    REQUIRE(!FT_New_Memory_Face_nid_postfix(library, bitmap.data(), bitmap.size(), 0, &face));
    const GuestCMapClass basic{sizeof(Map), Init, Done, Index, Next};
    state.face = face;
    description.face = face;
    REQUIRE(!sceFreeTypeOlCMapNew_nid_no_patch(&basic, &state, &description, &map));
    REQUIRE(!FT_Set_Charmap_nid_postfix(face, &map->charmap));
    REQUIRE(FT_Get_CMap_Format_nid_postfix(&map->charmap) == -1);
    REQUIRE(FT_Get_Char_Index_nid_postfix(face, 'Z') == 1);
    sceFreeTypeOlCMapDone_nid_no_patch(map);
    REQUIRE(!FT_Done_Face_nid_postfix(face));
    REQUIRE(!FT_Done_FreeType_nid_postfix(library));
}
