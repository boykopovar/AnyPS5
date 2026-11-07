#include "prx/libcairo/include/InventoryExports.hpp"
#include "prx/libSceFreeType/include/Api.hpp"

#include <atomic>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kNullPointer = 7;
constexpr int kInvalidStatus = 12;
constexpr int kUserFontError = 51;

struct CairoFontFaceState {
    struct UserDataSlot {
        const cairo_user_data_key_t* key{};
        void* data{};
        cairo_destroy_func_t destroy{};
    };

    std::atomic<unsigned> references{1};
    int status{kSuccess};
    FreeType::GuestFaceRec* face{};
    int loadFlags{};
    bool immortal{};
    std::vector<UserDataSlot> userData;

    CairoFontFaceState() = default;
    CairoFontFaceState(int error, bool permanent) : status(error), immortal(permanent) {}
};

CairoFontFaceState& NullPointerFace() {
    static CairoFontFaceState face{kNullPointer, true};
    return face;
}

CairoFontFaceState& NoMemoryFace() {
    static CairoFontFaceState face{kNoMemory, true};
    return face;
}

CairoFontFaceState& UserFontErrorFace() {
    static CairoFontFaceState face{kUserFontError, true};
    return face;
}

CairoFontFaceState* State(cairo_font_face_t* face) {
    return reinterpret_cast<CairoFontFaceState*>(face);
}

cairo_font_face_t* Handle(CairoFontFaceState* state) {
    return reinterpret_cast<cairo_font_face_t*>(state);
}

}

extern "C" {

cairo_font_face_t* APS5_VABI cairo_ft_font_face_create_for_ft_face(FreeType::GuestFaceRec* face, int loadFlags) {
    if (!face) return Handle(&NullPointerFace());
    if (FT_Reference_Face_nid_postfix(face) != 0) return Handle(&UserFontErrorFace());
    auto* state = new (std::nothrow) CairoFontFaceState;
    if (!state) {
        FT_Done_Face_nid_postfix(face);
        return Handle(&NoMemoryFace());
    }
    state->face = face;
    state->loadFlags = loadFlags;
    return Handle(state);
}

void APS5_VABI cairo_font_face_destroy(cairo_font_face_t* face) {
    if (!face) return;
    auto* state = State(face);
    if (state->immortal) return;
    auto references = state->references.load(std::memory_order_relaxed);
    while (references != 0 && !state->references.compare_exchange_weak(
                                  references, references - 1, std::memory_order_acq_rel,
                                  std::memory_order_relaxed)) {}
    if (references != 1) return;
    for (auto slot = state->userData.rbegin(); slot != state->userData.rend(); ++slot)
        if (slot->data && slot->destroy) slot->destroy(slot->data);
    if (state->face) FT_Done_Face_nid_postfix(state->face);
    delete state;
}

void* APS5_VABI cairo_font_face_get_user_data(cairo_font_face_t* face, const cairo_user_data_key_t* key) {
    if (!face) return nullptr;
    const auto* state = State(face);
    if (state->status != kSuccess) return nullptr;
    for (const auto& slot : state->userData)
        if (slot.key == key) return slot.data;
    return nullptr;
}

cairo_font_face_t* APS5_VABI cairo_font_face_reference(cairo_font_face_t* face) {
    if (!face) return nullptr;
    auto* state = State(face);
    if (state->immortal) return face;
    auto references = state->references.load(std::memory_order_relaxed);
    while (references != 0 && references != std::numeric_limits<unsigned>::max()) {
        if (state->references.compare_exchange_weak(references, references + 1,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed))
            break;
    }
    return face;
}

int APS5_VABI cairo_font_face_set_user_data(cairo_font_face_t* face, const cairo_user_data_key_t* key,
                                            void* data, cairo_destroy_func_t destroy) {
    if (!face) return kNullPointer;
    auto* state = State(face);
    if (state->status != kSuccess) return state->status;
    if (state->references.load(std::memory_order_relaxed) == 0) return kInvalidStatus;
    CairoFontFaceState::UserDataSlot* selected = nullptr;
    for (auto& slot : state->userData) {
        if (slot.key == key) {
            selected = &slot;
            break;
        }
        if (data && !slot.data) selected = &slot;
    }
    if (selected) {
        if (selected->data && selected->destroy) selected->destroy(selected->data);
        *selected = data ? CairoFontFaceState::UserDataSlot{key, data, destroy}
                         : CairoFontFaceState::UserDataSlot{};
    } else if (data) {
        if (state->userData.size() == state->userData.max_size()) return kNoMemory;
        try {
            state->userData.push_back({key, data, destroy});
        } catch (const std::bad_alloc&) {
            return kNoMemory;
        }
    }
    return kSuccess;
}

}
