#include "prx/libcairo/include/FontFace.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"

#include <CairoBackend.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <map>
#include <mutex>
#include <new>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kFileNotFound = 18;

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
    CairoFtFontFaceKey key{};
    FcPattern* pattern{};
    cairo_font_face_t* backend{};
    std::vector<UserDataSlot> userData;
};

namespace {

std::mutex& CacheMutex() {
    static auto* mutex = new std::mutex;
    return *mutex;
}

std::map<CairoFtFontFaceKey, CairoFontFaceState*>& Cache() {
    static auto* cache = new std::map<CairoFtFontFaceKey, CairoFontFaceState*>;
    return *cache;
}

CairoFontFaceState* State(CairoFontFace* face) {
    return reinterpret_cast<CairoFontFaceState*>(face);
}

CairoFontFace* Handle(CairoFontFaceState* state) {
    return reinterpret_cast<CairoFontFace*>(state);
}

std::mutex& BackendMutex() {
    static auto* mutex = new std::mutex;
    return *mutex;
}

const cairo_user_data_key_t backendKey{};

void Finalize(CairoFontFaceState* state) {
    if (state->pattern) FcPatternDestroy_nid_postfix(state->pattern);
    for (std::size_t slot = state->userData.size(); slot-- > 0;) {
        const auto& entry = state->userData[slot];
        if (entry.data && entry.destroy) entry.destroy(entry.data);
    }
    delete state;
}

void FinalizeBackend(void* data) {
    auto* state = static_cast<CairoFontFaceState*>(data);
    state->finalizing = true;
    Finalize(state);
}

}

CairoFontFace* CairoFontFaceFromBackend_nid_no_patch(cairo_font_face_t* face) {
    if (!face) return nullptr;
    std::lock_guard lock(BackendMutex());
    if (aps5_cairo_font_face_get_reference_count(face) == 0) {
        static auto* faces = new std::map<cairo_font_face_t*, CairoFontFaceState*>;
        auto& state = (*faces)[face];
        if (!state) state = new CairoFontFaceState{.immortal = true, .status = aps5_cairo_font_face_status(face), .backend = face};
        return Handle(state);
    }
    if (auto* found = aps5_cairo_font_face_get_user_data(face, &backendKey))
        return Handle(static_cast<CairoFontFaceState*>(found));
    auto* state = new CairoFontFaceState{.backend = face};
    if (aps5_cairo_font_face_set_user_data(face, &backendKey, state, FinalizeBackend) != CAIRO_STATUS_SUCCESS) {
        delete state;
        throw std::bad_alloc();
    }
    return Handle(state);
}

cairo_font_face_t* CairoFontFaceBackend_nid_no_patch(CairoFontFace* face, const char* function) {
    if (!face) return nullptr;
    auto* backend = State(face)->backend;
    if (!backend) NotImplemented_nid_no_patch(function);
    return backend;
}

CairoFontFace* CairoFontFaceNilNoMemory() {
    static CairoFontFaceState nil{.immortal = true, .status = kNoMemory};
    return Handle(&nil);
}

CairoFontFace* CairoFontFaceNilFileNotFound() {
    static CairoFontFaceState nil{.immortal = true, .status = kFileNotFound};
    return Handle(&nil);
}

CairoFontFace* CairoFtFontFaceFind(const CairoFtFontFaceKey& key) {
    std::lock_guard lock(CacheMutex());
    auto& cache = Cache();
    if (const auto found = cache.find(key); found != cache.end()) {
        found->second->references.fetch_add(1, std::memory_order_relaxed);
        return Handle(found->second);
    }
    auto* state = new (std::nothrow) CairoFontFaceState;
    if (!state) return CairoFontFaceNilNoMemory();
    try {
        state->key = key;
        cache.emplace(state->key, state);
    } catch (const std::bad_alloc&) {
        delete state;
        return CairoFontFaceNilNoMemory();
    }
    return Handle(state);
}

CairoFontFace* CairoFtFontFaceForPattern(FcPattern* duplicate) {
    auto* state = new (std::nothrow) CairoFontFaceState;
    if (!state) {
        FcPatternDestroy_nid_postfix(duplicate);
        return CairoFontFaceNilNoMemory();
    }
    state->pattern = duplicate;
    return Handle(state);
}

extern "C" {

CairoFontFace* APS5_VABI cairo_ft_font_face_create_for_ft_face(FreeType::GuestFaceRec* face, int loadFlags) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    std::int64_t faceIndex{};
    const bool readable = GuestCopyIn_nid_no_patch(&faceIndex, &face->face_index, sizeof(faceIndex));
    if (!readable) NotImplemented_nid_no_patch(__func__);
    CairoFtFontFaceKey key;
    key.fromFace = true;
    key.face = face;
    key.id = faceIndex;
    key.loadFlags = loadFlags;
    return CairoFtFontFaceFind(key);
}

CairoFontFace* APS5_VABI cairo_font_face_reference(CairoFontFace* fontFace) {
    if (fontFace && State(fontFace)->backend) {
        if (State(fontFace)->finalizing) NotImplemented_nid_no_patch(__func__);
        aps5_cairo_font_face_reference(State(fontFace)->backend);
        return fontFace;
    }
    if (fontFace && !State(fontFace)->immortal)
        State(fontFace)->references.fetch_add(1, std::memory_order_relaxed);
    return fontFace;
}

void APS5_VABI cairo_font_face_destroy(CairoFontFace* fontFace) {
    if (!fontFace || State(fontFace)->immortal) return;
    auto* state = State(fontFace);
    if (state->backend) {
        if (state->finalizing) NotImplemented_nid_no_patch(__func__);
        aps5_cairo_font_face_destroy(state->backend);
        return;
    }
    {
        std::lock_guard lock(CacheMutex());
        auto count = state->references.load(std::memory_order_acquire);
        while (count != 1 && !state->references.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel)) {}
        if (count != 1) return;
        if (state->finalizing) NotImplemented_nid_no_patch(__func__);
        state->finalizing = true;
        auto& cache = Cache();
        if (!state->pattern)
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
