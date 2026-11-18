#include "prx/libcairo/include/FontFace.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <new>
#include <tuple>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;

using CacheKey = std::tuple<const FreeType::GuestFaceRec*, std::int64_t, unsigned>;

}

struct CairoFontFaceState {
    struct UserDataSlot {
        const CairoUserDataKey* key{};
        void* data{};
        CairoDestroyFunc destroy{};
    };
    std::atomic<unsigned> references{1};
    bool immortal{};
    bool finalizing{};
    int status{kSuccess};
    CacheKey key{};
    std::vector<UserDataSlot> userData;
};

namespace {

std::mutex& CacheMutex() {
    static auto* mutex = new std::mutex;
    return *mutex;
}

std::map<CacheKey, CairoFontFaceState*>& Cache() {
    static auto* cache = new std::map<CacheKey, CairoFontFaceState*>;
    return *cache;
}

CairoFontFaceState* State(CairoFontFace* face) {
    return reinterpret_cast<CairoFontFaceState*>(face);
}

CairoFontFace* Handle(CairoFontFaceState* state) {
    return reinterpret_cast<CairoFontFace*>(state);
}

CairoFontFace* NilFace() {
    static CairoFontFaceState nil{.immortal = true, .status = kNoMemory};
    return Handle(&nil);
}

void Finalize(CairoFontFaceState* state) {
    for (std::size_t slot = state->userData.size(); slot-- > 0;) {
        const auto& entry = state->userData[slot];
        if (entry.data && entry.destroy) entry.destroy(entry.data);
    }
    delete state;
}

}

extern "C" {

CairoFontFace* APS5_VABI cairo_ft_font_face_create_for_ft_face(FreeType::GuestFaceRec* face, int loadFlags) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    const CacheKey key{face, face->face_index, static_cast<unsigned>(loadFlags)};
    std::lock_guard lock(CacheMutex());
    auto& cache = Cache();
    if (const auto found = cache.find(key); found != cache.end()) {
        found->second->references.fetch_add(1, std::memory_order_relaxed);
        return Handle(found->second);
    }
    auto* state = new (std::nothrow) CairoFontFaceState;
    if (!state) return NilFace();
    state->key = key;
    try {
        cache.emplace(key, state);
    } catch (const std::bad_alloc&) {
        delete state;
        return NilFace();
    }
    return Handle(state);
}

CairoFontFace* APS5_VABI cairo_font_face_reference(CairoFontFace* fontFace) {
    if (fontFace && !State(fontFace)->immortal)
        State(fontFace)->references.fetch_add(1, std::memory_order_relaxed);
    return fontFace;
}

void APS5_VABI cairo_font_face_destroy(CairoFontFace* fontFace) {
    if (!fontFace || State(fontFace)->immortal) return;
    auto* state = State(fontFace);
    {
        std::lock_guard lock(CacheMutex());
        auto count = state->references.load(std::memory_order_acquire);
        while (count != 1 && !state->references.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel)) {}
        if (count != 1) return;
        if (state->finalizing) NotImplemented_nid_no_patch(__func__);
        state->finalizing = true;
        auto& cache = Cache();
        if (const auto found = cache.find(state->key); found != cache.end() && found->second == state) cache.erase(found);
    }
    Finalize(state);
}

void* APS5_VABI cairo_font_face_get_user_data(CairoFontFace* fontFace, const CairoUserDataKey* key) {
    if (!fontFace) NotImplemented_nid_no_patch(__func__);
    for (const auto& slot : State(fontFace)->userData)
        if (slot.key == key) return slot.data;
    return nullptr;
}

int APS5_VABI cairo_font_face_set_user_data(CairoFontFace* fontFace, const CairoUserDataKey* key,
                                            void* data, CairoDestroyFunc destroy) {
    if (!fontFace) NotImplemented_nid_no_patch(__func__);
    auto* state = State(fontFace);
    if (state->immortal) return state->status;
    if (state->finalizing) NotImplemented_nid_no_patch(__func__);
    const CairoFontFaceState::UserDataSlot replacement =
        data ? CairoFontFaceState::UserDataSlot{key, data, destroy} : CairoFontFaceState::UserDataSlot{};
    std::size_t selected = state->userData.size();
    for (std::size_t slot = 0; slot < state->userData.size(); ++slot) {
        if (state->userData[slot].key == key) {
            const auto previous = state->userData[slot];
            if (previous.destroy && previous.data) previous.destroy(previous.data);
            selected = slot;
            break;
        }
        if (data && !state->userData[slot].data) selected = slot;
    }
    if (selected < state->userData.size()) {
        state->userData[selected] = replacement;
        return kSuccess;
    }
    if (!data) return kSuccess;
    try {
        state->userData.push_back(replacement);
    } catch (const std::bad_alloc&) {
        return kNoMemory;
    }
    return kSuccess;
}

}
