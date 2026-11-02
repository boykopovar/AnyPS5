#include "prx/libcairo/include/Context.hpp"

#include <atomic>
#include <new>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kInvalidRestore = 2;
constexpr int kInvalidMatrix = 5;
constexpr int kNullPointer = 7;

constexpr CairoMatrix kIdentity{1, 0, 0, 1, 0, 0};

struct SavedState {
    CairoMatrix matrix;
    SavedState* previous;
};

struct ContextStorage {
    std::atomic<unsigned> references{1};
    int status{kSuccess};
    bool immortal{false};
    CairoSurface* target{};
    CairoMatrix matrix{kIdentity};
    SavedState* saved{};

    explicit ContextStorage(int error = kSuccess, bool staticError = false)
        : status(error), immortal(staticError) {}
};

ContextStorage* State(CairoContext* context) {
    return reinterpret_cast<ContextStorage*>(context);
}

CairoContext* Handle(ContextStorage* state) {
    return reinterpret_cast<CairoContext*>(state);
}

CairoContext* ErrorContext(int status) {
    static ContextStorage noMemory(kNoMemory, true);
    static ContextStorage invalidMatrix(kInvalidMatrix, true);
    static ContextStorage nullPointer(kNullPointer, true);
    static ContextStorage invalidRestore(kInvalidRestore, true);
    switch (status) {
    case kInvalidMatrix: return Handle(&invalidMatrix);
    case kNullPointer: return Handle(&nullPointer);
    case kInvalidRestore: return Handle(&invalidRestore);
    default: return Handle(&noMemory);
    }
}

ContextStorage* Valid(CairoContext* context) {
    if (!context) return nullptr;
    auto* state = State(context);
    return state->status == kSuccess ? state : nullptr;
}

void ReleaseStorage(ContextStorage* state) {
    while (state->saved) {
        SavedState* previous = state->saved->previous;
        delete state->saved;
        state->saved = previous;
    }
    if (state->target) cairo_surface_destroy(state->target);
    delete state;
}

void ReplaceMatrix(ContextStorage* state, const CairoMatrix& candidate) {
    CairoMatrix inverse = candidate;
    if (cairo_matrix_invert(&inverse) != kSuccess) {
        state->status = kInvalidMatrix;
        return;
    }
    state->matrix = candidate;
}

} // namespace

extern "C" {

CairoContext* APS5_VABI cairo_create(CairoSurface* target) {
    if (!target) return ErrorContext(kNullPointer);
    const int status = cairo_surface_status(target);
    auto* state = new (std::nothrow) ContextStorage(status);
    if (!state) return ErrorContext(kNoMemory);
    state->target = cairo_surface_reference(target);
    return Handle(state);
}

CairoContext* APS5_VABI cairo_reference(CairoContext* context) {
    if (context && !State(context)->immortal)
        State(context)->references.fetch_add(1, std::memory_order_relaxed);
    return context;
}

void APS5_VABI cairo_destroy(CairoContext* context) {
    if (!context) return;
    auto* state = State(context);
    if (!state->immortal && state->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        ReleaseStorage(state);
}

void APS5_VABI cairo_save(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    auto* snapshot = new (std::nothrow) SavedState{state->matrix, state->saved};
    if (!snapshot) {
        state->status = kNoMemory;
        return;
    }
    state->saved = snapshot;
}

void APS5_VABI cairo_restore(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (!state->saved) {
        state->status = kInvalidRestore;
        return;
    }
    SavedState* snapshot = state->saved;
    state->matrix = snapshot->matrix;
    state->saved = snapshot->previous;
    delete snapshot;
}

CairoSurface* APS5_VABI cairo_get_target(CairoContext* context) {
    return context ? State(context)->target : nullptr;
}

void APS5_VABI cairo_translate(CairoContext* context, double tx, double ty) {
    auto* state = Valid(context);
    if (!state) return;
    CairoMatrix matrix = state->matrix;
    cairo_matrix_translate(&matrix, tx, ty);
    ReplaceMatrix(state, matrix);
}

void APS5_VABI cairo_scale(CairoContext* context, double sx, double sy) {
    auto* state = Valid(context);
    if (!state) return;
    CairoMatrix matrix = state->matrix;
    cairo_matrix_scale(&matrix, sx, sy);
    ReplaceMatrix(state, matrix);
}

void APS5_VABI cairo_rotate(CairoContext* context, double radians) {
    auto* state = Valid(context);
    if (!state) return;
    CairoMatrix matrix = state->matrix;
    cairo_matrix_rotate(&matrix, radians);
    ReplaceMatrix(state, matrix);
}

void APS5_VABI cairo_transform(CairoContext* context, const CairoMatrix* matrix) {
    auto* state = Valid(context);
    if (!state || !matrix) return;
    CairoMatrix combined{};
    cairo_matrix_multiply(&combined, matrix, &state->matrix);
    ReplaceMatrix(state, combined);
}

void APS5_VABI cairo_set_matrix(CairoContext* context, const CairoMatrix* matrix) {
    auto* state = Valid(context);
    if (state && matrix) ReplaceMatrix(state, *matrix);
}

void APS5_VABI cairo_get_matrix(CairoContext* context, CairoMatrix* matrix) {
    auto* state = Valid(context);
    if (state && matrix) *matrix = state->matrix;
}

void APS5_VABI cairo_identity_matrix(CairoContext* context) {
    auto* state = Valid(context);
    if (state) state->matrix = kIdentity;
}

void APS5_VABI cairo_user_to_device(CairoContext* context, double* x, double* y) {
    auto* state = Valid(context);
    if (state && x && y) cairo_matrix_transform_point(&state->matrix, x, y);
}

void APS5_VABI cairo_user_to_device_distance(CairoContext* context, double* dx, double* dy) {
    auto* state = Valid(context);
    if (state && dx && dy) cairo_matrix_transform_distance(&state->matrix, dx, dy);
}

void APS5_VABI cairo_device_to_user(CairoContext* context, double* x, double* y) {
    auto* state = Valid(context);
    if (!state || !x || !y) return;
    CairoMatrix inverse = state->matrix;
    if (cairo_matrix_invert(&inverse) != kSuccess) {
        state->status = kInvalidMatrix;
        return;
    }
    cairo_matrix_transform_point(&inverse, x, y);
}

void APS5_VABI cairo_device_to_user_distance(CairoContext* context, double* dx, double* dy) {
    auto* state = Valid(context);
    if (!state || !dx || !dy) return;
    CairoMatrix inverse = state->matrix;
    if (cairo_matrix_invert(&inverse) != kSuccess) {
        state->status = kInvalidMatrix;
        return;
    }
    cairo_matrix_transform_distance(&inverse, dx, dy);
}

} // extern "C"
