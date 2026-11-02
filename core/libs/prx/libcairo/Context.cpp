#include "prx/libcairo/include/Context.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <new>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kInvalidRestore = 2;
constexpr int kInvalidMatrix = 5;
constexpr int kNullPointer = 7;
constexpr int kInvalidDash = 19;

constexpr CairoMatrix kIdentity{1, 0, 0, 1, 0, 0};

struct GraphicsStyle {
    int antialias{};
    int fillRule{};
    int lineCap{};
    int lineJoin{};
    double lineWidth{2};
    double miterLimit{10};
    double* dashes{};
    int dashCount{};
    double dashOffset{};
};

struct SavedState {
    CairoMatrix matrix;
    GraphicsStyle style;
    SavedState* previous;
};

struct ContextStorage {
    std::atomic<unsigned> references{1};
    int status{kSuccess};
    bool immortal{false};
    CairoSurface* target{};
    CairoMatrix matrix{kIdentity};
    GraphicsStyle style{};
    SavedState* saved{};

    explicit ContextStorage(int error = kSuccess, bool staticError = false,
                            CairoSurface* initialTarget = nullptr)
        : status(error), immortal(staticError), target(initialTarget) {}
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
    static ContextStorage nullPointer(kNullPointer, true,
                                     CairoSurfaceError(kNoMemory));
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
    delete[] state->style.dashes;
    while (state->saved) {
        SavedState* previous = state->saved->previous;
        delete[] state->saved->style.dashes;
        delete state->saved;
        state->saved = previous;
    }
    if (state->target) cairo_surface_destroy(state->target);
    delete state;
}

void ReplaceMatrix(ContextStorage* state, const CairoMatrix& candidate) {
    const double determinant = candidate.xx * candidate.yy - candidate.yx * candidate.xy;
    if (determinant == 0 || !std::isfinite(determinant)) {
        state->status = kInvalidMatrix;
        return;
    }
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
    auto* snapshot = new (std::nothrow) SavedState{state->matrix, state->style, state->saved};
    if (!snapshot) {
        state->status = kNoMemory;
        return;
    }
    if (snapshot->style.dashCount > 0) {
        snapshot->style.dashes = new (std::nothrow) double[snapshot->style.dashCount];
        if (!snapshot->style.dashes) {
            delete snapshot;
            state->status = kNoMemory;
            return;
        }
        std::copy_n(state->style.dashes, snapshot->style.dashCount, snapshot->style.dashes);
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
    delete[] state->style.dashes;
    state->style = snapshot->style;
    state->saved = snapshot->previous;
    delete snapshot;
}

CairoSurface* APS5_VABI cairo_get_target(CairoContext* context) {
    return context ? State(context)->target : nullptr;
}

void APS5_VABI cairo_translate(CairoContext* context, double tx, double ty) {
    auto* state = Valid(context);
    if (!state) return;
    if (!std::isfinite(tx) || !std::isfinite(ty)) {
        state->status = kInvalidMatrix;
        return;
    }
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
    if (!matrix) return;
    auto* state = Valid(context);
    *matrix = state ? state->matrix : kIdentity;
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

void APS5_VABI cairo_set_antialias(CairoContext* context, int antialias) {
    auto* state = Valid(context);
    if (state) state->style.antialias = antialias;
}

int APS5_VABI cairo_get_antialias(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.antialias : 0;
}

void APS5_VABI cairo_set_fill_rule(CairoContext* context, int fillRule) {
    auto* state = Valid(context);
    if (state) state->style.fillRule = fillRule;
}

int APS5_VABI cairo_get_fill_rule(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.fillRule : 0;
}

void APS5_VABI cairo_set_line_cap(CairoContext* context, int cap) {
    auto* state = Valid(context);
    if (state) state->style.lineCap = cap;
}

int APS5_VABI cairo_get_line_cap(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.lineCap : 0;
}

void APS5_VABI cairo_set_line_join(CairoContext* context, int join) {
    auto* state = Valid(context);
    if (state) state->style.lineJoin = join;
}

int APS5_VABI cairo_get_line_join(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.lineJoin : 0;
}

void APS5_VABI cairo_set_line_width(CairoContext* context, double width) {
    auto* state = Valid(context);
    if (state) state->style.lineWidth = width < 0 ? 0 : width;
}

double APS5_VABI cairo_get_line_width(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.lineWidth : 2;
}

void APS5_VABI cairo_set_miter_limit(CairoContext* context, double limit) {
    auto* state = Valid(context);
    if (state) state->style.miterLimit = limit;
}

double APS5_VABI cairo_get_miter_limit(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.miterLimit : 10;
}

void APS5_VABI cairo_set_dash(CairoContext* context, const double* dashes,
                             int numDashes, double offset) {
    auto* state = Valid(context);
    if (!state) return;
    if (numDashes < 0 || (numDashes > 0 && !dashes)) {
        state->status = kInvalidDash;
        return;
    }
    double total = 0;
    for (int i = 0; i < numDashes; ++i) {
        if (!(dashes[i] >= 0)) {
            state->status = kInvalidDash;
            return;
        }
        total += dashes[i];
    }
    if (numDashes > 0 && !(total > 0)) {
        state->status = kInvalidDash;
        return;
    }

    double* copy = nullptr;
    if (numDashes > 0) {
        copy = new (std::nothrow) double[numDashes];
        if (!copy) {
            state->status = kNoMemory;
            return;
        }
        std::copy_n(dashes, numDashes, copy);
    }
    const double period = (numDashes % 2 == 0 ? total : total * 2);
    // Cairo's raster dash cycle cannot represent intervals below 1/256 pixel.
    // The native API turns such an otherwise valid dash array into solid strokes.
    if (numDashes > 0 && period < 1.0 / 256.0) {
        delete[] copy;
        copy = nullptr;
        numDashes = 0;
    }
    if (numDashes > 0) {
        offset = std::fmod(offset, period);
        if (offset < 0) offset += period;
    } else {
        offset = 0;
    }
    delete[] state->style.dashes;
    state->style.dashes = copy;
    state->style.dashCount = numDashes;
    state->style.dashOffset = offset;
}

void APS5_VABI cairo_get_dash(CairoContext* context, double* dashes, double* offset) {
    auto* state = Valid(context);
    if (!state) return;
    if (dashes) std::copy_n(state->style.dashes, state->style.dashCount, dashes);
    if (offset) *offset = state->style.dashOffset;
}

int APS5_VABI cairo_get_dash_count(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.dashCount : 0;
}

} // extern "C"
