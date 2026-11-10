#include "prx/libcairo/include/InventoryExports.hpp"
#include "prx/libSceFreeType/include/Api.hpp"

#include <atomic>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kNullPointer = 7;
constexpr int kInvalidStatus = 6;
constexpr int kFreetypeError = 40;

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
    std::mutex userDataMutex;
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

CairoFontFaceState& FreetypeErrorFace() {
    static CairoFontFaceState face{kFreetypeError, true};
    return face;
}

std::mutex& FaceCacheMutex() {
    static std::mutex mutex;
    return mutex;
}

using FaceKey = std::pair<FreeType::GuestFaceRec*, int>;

std::map<FaceKey, CairoFontFaceState*>& FaceCache() {
    static std::map<FaceKey, CairoFontFaceState*> cache;
    return cache;
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
    std::lock_guard cacheLock(FaceCacheMutex());
    const FaceKey key{face, loadFlags};
    if (const auto existing = FaceCache().find(key); existing != FaceCache().end()) {
        existing->second->references.fetch_add(1, std::memory_order_relaxed);
        return Handle(existing->second);
    }
    if (FT_Reference_Face_nid_postfix(face) != 0) return Handle(&FreetypeErrorFace());
    auto* state = new (std::nothrow) CairoFontFaceState;
    if (!state) {
        FT_Done_Face_nid_postfix(face);
        return Handle(&NoMemoryFace());
    }
    state->face = face;
    state->loadFlags = loadFlags;
    try {
        FaceCache().emplace(key, state);
    } catch (const std::bad_alloc&) {
        FT_Done_Face_nid_postfix(face);
        delete state;
        return Handle(&NoMemoryFace());
    }
    return Handle(state);
}

void APS5_VABI cairo_font_face_destroy(cairo_font_face_t* face) {
    if (!face) return;
    auto* state = State(face);
    if (state->immortal) return;
    {
        std::lock_guard cacheLock(FaceCacheMutex());
        const auto references = state->references.load(std::memory_order_relaxed);
        if (references == 0) return;
        state->references.store(references - 1, std::memory_order_release);
        if (references != 1) return;
        FaceCache().erase({state->face, state->loadFlags});
    }
    std::vector<CairoFontFaceState::UserDataSlot> finalizers;
    {
        std::lock_guard dataLock(state->userDataMutex);
        state->status = kInvalidStatus;
        finalizers = std::move(state->userData);
    }
    for (auto slot = finalizers.rbegin(); slot != finalizers.rend(); ++slot)
        if (slot->data && slot->destroy) slot->destroy(slot->data);
    if (state->face) FT_Done_Face_nid_postfix(state->face);
    delete state;
}

void* APS5_VABI cairo_font_face_get_user_data(cairo_font_face_t* face, const cairo_user_data_key_t* key) {
    if (!face) return nullptr;
    auto* state = State(face);
    std::lock_guard dataLock(state->userDataMutex);
    if (state->status != kSuccess) return nullptr;
    for (const auto& slot : state->userData)
        if (slot.key == key) return slot.data;
    return nullptr;
}

cairo_font_face_t* APS5_VABI cairo_font_face_reference(cairo_font_face_t* face) {
    if (!face) return nullptr;
    auto* state = State(face);
    if (state->immortal) return face;
    std::lock_guard cacheLock(FaceCacheMutex());
    const auto references = state->references.load(std::memory_order_relaxed);
    if (references != 0 && references != std::numeric_limits<unsigned>::max())
        state->references.store(references + 1, std::memory_order_relaxed);
    return face;
}

int APS5_VABI cairo_font_face_set_user_data(cairo_font_face_t* face, const cairo_user_data_key_t* key,
                                            void* data, cairo_destroy_func_t destroy) {
    if (!face) return kNullPointer;
    auto* state = State(face);
    std::unique_lock dataLock(state->userDataMutex);
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
        const auto replaced = *selected;
        *selected = data ? CairoFontFaceState::UserDataSlot{key, data, destroy}
                         : CairoFontFaceState::UserDataSlot{};
        dataLock.unlock();
        if (replaced.data && replaced.destroy) replaced.destroy(replaced.data);
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
