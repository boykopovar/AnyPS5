#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kInvalidRestore = 2;
constexpr int kInvalidMatrix = 5;
constexpr int kNullPointer = 7;
constexpr int kInvalidDash = 19;
constexpr int kInvalidPathData = 9;

constexpr CairoMatrix kIdentity{1, 0, 0, 1, 0, 0};

struct GraphicsStyle {
    int compositionOperator{2};
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

struct PathEntry {
    int type{};
    double points[6]{};
};

struct ContextStorage {
    std::atomic<unsigned> references{1};
    int status{kSuccess};
    int initialStatus{kSuccess};
    bool immortal{false};
    CairoSurface* target{};
    CairoMatrix matrix{kIdentity};
    GraphicsStyle style{};
    SavedState* saved{};
    std::vector<PathEntry>* path{};
    bool hasCurrent{};
    double currentX{}, currentY{};
    double startX{}, startY{};

    explicit ContextStorage(int error = kSuccess, bool staticError = false,
                            CairoSurface* initialTarget = nullptr)
        : status(error), initialStatus(error), immortal(staticError), target(initialTarget) {}
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
    delete state->path;
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

bool Record(ContextStorage* state, const PathEntry& entry) {
    if (!state->path) {
        state->path = new (std::nothrow) std::vector<PathEntry>;
        if (!state->path) {
            state->status = kNoMemory;
            return false;
        }
    }
    try {
        state->path->push_back(entry);
        return true;
    } catch (const std::bad_alloc&) {
        state->status = kNoMemory;
        return false;
    }
}

double Fixed(double coordinate) {
    if (std::isnan(coordinate)) return 0;
    constexpr double kMinimum = -8388606.0;
    constexpr double kMaximum = 8388606.0 - 1.0 / 256.0;
    return std::nearbyint(std::clamp(coordinate, kMinimum, kMaximum) * 256.0) / 256.0;
}

void DevicePoint(const ContextStorage* state, double& x, double& y) {
    cairo_matrix_transform_point(&state->matrix, &x, &y);
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(state->target, &sx, &sy);
    x = Fixed(x * sx);
    y = Fixed(y * sy);
}

void UserPoint(const ContextStorage* state, const CairoMatrix& inverse,
               double& x, double& y) {
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(state->target, &sx, &sy);
    x /= sx;
    y /= sy;
    cairo_matrix_transform_point(&inverse, &x, &y);
}

struct PathRange {
    double minimum{std::numeric_limits<double>::infinity()};
    double maximum{-std::numeric_limits<double>::infinity()};

    void Add(double value) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
};

void AddCubicRange(PathRange& range, double start, double first, double second, double end) {
    range.Add(start);
    range.Add(end);
    const double a = -start + 3 * first - 3 * second + end;
    const double b = 2 * (start - 2 * first + second);
    const double c = first - start;
    const auto evaluate = [&](double t) {
        if (t <= 0 || t >= 1) return;
        const double u = 1 - t;
        range.Add(Fixed(u * u * u * start + 3 * u * u * t * first +
                        3 * u * t * t * second + t * t * t * end));
    };
    if (a == 0) {
        if (b != 0) evaluate(-c / b);
        return;
    }
    const double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return;
    const double root = std::sqrt(discriminant);
    evaluate((-b + root) / (2 * a));
    evaluate((-b - root) / (2 * a));
}

bool AppendMove(ContextStorage* state, double x, double y) {
    PathEntry command{};
    command.type = 0;
    command.points[0] = x;
    command.points[1] = y;
    if (state->path && !state->path->empty() && state->path->back().type == 0)
        state->path->back() = command;
    else if (!Record(state, command)) return false;
    state->hasCurrent = true;
    state->currentX = state->startX = x;
    state->currentY = state->startY = y;
    return true;
}

void AppendLine(ContextStorage* state, double x, double y) {
    if (!state->hasCurrent) {
        AppendMove(state, x, y);
        return;
    }
    if (state->path && !state->path->empty()) {
        auto& previous = state->path->back();
        const bool matchingLine = previous.type == 1 &&
                                  previous.points[0] == x && previous.points[1] == y;
        const bool matchingCurve = previous.type == 2 &&
                                   previous.points[4] == x && previous.points[5] == y;
        if (matchingLine || matchingCurve)
            return;
        if (previous.type == 1 && state->path->size() >= 2) {
            const auto& earlier = (*state->path)[state->path->size() - 2];
            const int endpoint = earlier.type == 2 ? 4 : 0;
            const long double dx1 = previous.points[0] - earlier.points[endpoint];
            const long double dy1 = previous.points[1] - earlier.points[endpoint + 1];
            const long double dx2 = x - previous.points[0];
            const long double dy2 = y - previous.points[1];
            if (dx1 * dy2 == dy1 * dx2 && dx1 * dx2 + dy1 * dy2 >= 0) {
                previous.points[0] = state->currentX = x;
                previous.points[1] = state->currentY = y;
                return;
            }
        }
    }
    PathEntry command{};
    command.type = 1;
    command.points[0] = x;
    command.points[1] = y;
    if (Record(state, command)) {
        state->currentX = x;
        state->currentY = y;
    }
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
    if (!context) return nullptr;
    const auto* state = State(context);
    if (state->status == kSuccess) return state->target;
    if (state->initialStatus != kSuccess && state->target) return state->target;
    return CairoSurfaceError(kNoMemory);
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
        if (dashes[i] < 0) {
            state->status = kInvalidDash;
            return;
        }
        total += dashes[i];
    }
    if (numDashes > 0 && total == 0) {
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
        for (int i = 1; i + 1 < numDashes;) {
            if (copy[i] == 0) {
                copy[i - 1] += copy[i + 1];
                std::move(copy + i + 2, copy + numDashes, copy + i);
                numDashes -= 2;
                if (i > 1) --i;
            } else {
                ++i;
            }
        }
    }
    double offTotal = 0;
    for (int i = 1; i < numDashes; i += 2) offTotal += copy[i];
    const double period = (numDashes % 2 == 0 ? total : total * 2);
    if (numDashes > 0 && ((numDashes % 2 != 0 && period < 1.0 / 256.0) ||
                          (numDashes % 2 == 0 && offTotal < 1.0 / 512.0))) {
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

void APS5_VABI cairo_set_operator(CairoContext* context, int operation) {
    auto* state = Valid(context);
    if (state) state->style.compositionOperator = operation;
}

int APS5_VABI cairo_get_operator(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.compositionOperator : 2;
}

void APS5_VABI cairo_rectangle(CairoContext* context, double x, double y,
                               double width, double height) {
    auto* state = Valid(context);
    if (!state) return;
    double wx = width, wy = 0;
    double hx = 0, hy = height;
    cairo_matrix_transform_distance(&state->matrix, &wx, &wy);
    cairo_matrix_transform_distance(&state->matrix, &hx, &hy);
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(state->target, &sx, &sy);
    wx = Fixed(wx * sx);
    wy = Fixed(wy * sy);
    hx = Fixed(hx * sx);
    hy = Fixed(hy * sy);
    DevicePoint(state, x, y);
    if (!AppendMove(state, x, y)) return;
    AppendLine(state, x + wx, y + wy);
    if (state->status != kSuccess) return;
    AppendLine(state, x + wx + hx, y + wy + hy);
    if (state->status != kSuccess) return;
    AppendLine(state, x + hx, y + hy);
    cairo_close_path(context);
}

void APS5_VABI cairo_new_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (state->path) state->path->clear();
    state->hasCurrent = false;
}

void APS5_VABI cairo_new_sub_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (state->path && !state->path->empty() && state->path->back().type == 0)
        state->path->pop_back();
    state->hasCurrent = false;
}

void APS5_VABI cairo_move_to(CairoContext* context, double x, double y) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x, y);
    AppendMove(state, x, y);
}

void APS5_VABI cairo_line_to(CairoContext* context, double x, double y) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x, y);
    AppendLine(state, x, y);
}

void APS5_VABI cairo_curve_to(CairoContext* context, double x1, double y1,
                              double x2, double y2, double x3, double y3) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x1, y1);
    DevicePoint(state, x2, y2);
    DevicePoint(state, x3, y3);
    if (!state->hasCurrent && !AppendMove(state, x1, y1)) return;
    if (state->currentX == x1 && state->currentY == y1 &&
        x1 == x2 && y1 == y2 && x2 == x3 && y2 == y3) {
        AppendLine(state, x3, y3);
        return;
    }
    PathEntry command{};
    command.type = 2;
    command.points[0] = x1;
    command.points[1] = y1;
    command.points[2] = x2;
    command.points[3] = y2;
    command.points[4] = x3;
    command.points[5] = y3;
    if (Record(state, command)) {
        state->currentX = x3;
        state->currentY = y3;
    }
}

void APS5_VABI cairo_close_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state || !state->hasCurrent) return;
    AppendLine(state, state->startX, state->startY);
    if (state->status != kSuccess) return;
    if (state->path && !state->path->empty() && state->path->back().type == 1)
        state->path->pop_back();
    PathEntry close{};
    close.type = 3;
    if (!Record(state, close)) return;
    AppendMove(state, state->startX, state->startY);
}

int APS5_VABI cairo_has_current_point(CairoContext* context) {
    auto* state = Valid(context);
    return state && state->hasCurrent;
}

void APS5_VABI cairo_get_current_point(CairoContext* context, double* x, double* y) {
    auto* state = Valid(context);
    if (!x || !y) return;
    if (!state || !state->hasCurrent) {
        *x = *y = 0;
        return;
    }
    CairoMatrix inverse = state->matrix;
    if (cairo_matrix_invert(&inverse) != kSuccess) return;
    *x = state->currentX;
    *y = state->currentY;
    UserPoint(state, inverse, *x, *y);
}

CairoPath* APS5_VABI cairo_copy_path(CairoContext* context) {
    auto* state = Valid(context);
    auto* result = new (std::nothrow) CairoPath{context ? State(context)->status : kNullPointer,
                                               nullptr, 0};
    if (!result) return nullptr;
    if (!state || !state->path) return result;

    std::size_t total = 0;
    for (const auto& entry : *state->path) total += entry.type == 2 ? 4 : entry.type == 3 ? 1 : 2;
    if (total > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result->status = kNoMemory;
        return result;
    }
    if (total == 0) return result;
    result->data = new (std::nothrow) CairoPathData[total]();
    if (!result->data) {
        result->status = kNoMemory;
        return result;
    }
    CairoMatrix inverse = state->matrix;
    if (cairo_matrix_invert(&inverse) != kSuccess) {
        result->status = kInvalidMatrix;
        delete[] result->data;
        result->data = nullptr;
        return result;
    }
    std::size_t offset = 0;
    for (const auto& entry : *state->path) {
        const int length = entry.type == 2 ? 4 : entry.type == 3 ? 1 : 2;
        result->data[offset].header = {entry.type, length};
        for (int i = 1; i < length; ++i) {
            double x = entry.points[(i - 1) * 2];
            double y = entry.points[(i - 1) * 2 + 1];
            UserPoint(state, inverse, x, y);
            result->data[offset + i].point = {x, y};
        }
        offset += length;
    }
    result->num_data = static_cast<int>(total);
    return result;
}

void APS5_VABI cairo_path_extents(CairoContext* context, double* x1, double* y1,
                                 double* x2, double* y2) {
    double px1 = 0, py1 = 0, px2 = 0, py2 = 0;
    const auto store = [&] {
        if (x1) *x1 = px1;
        if (y1) *y1 = py1;
        if (x2) *x2 = px2;
        if (y2) *y2 = py2;
    };
    auto* state = Valid(context);
    if (!state || !state->path) return store();

    PathRange xRange, yRange;
    double currentX = 0, currentY = 0, startX = 0, startY = 0;
    for (const auto& entry : *state->path) {
        if (entry.type == 0) {
            currentX = startX = entry.points[0];
            currentY = startY = entry.points[1];
        } else if (entry.type == 1) {
            xRange.Add(currentX);
            yRange.Add(currentY);
            xRange.Add(entry.points[0]);
            yRange.Add(entry.points[1]);
            currentX = entry.points[0];
            currentY = entry.points[1];
        } else if (entry.type == 2) {
            AddCubicRange(xRange, currentX, entry.points[0], entry.points[2], entry.points[4]);
            AddCubicRange(yRange, currentY, entry.points[1], entry.points[3], entry.points[5]);
            currentX = entry.points[4];
            currentY = entry.points[5];
        } else if (entry.type == 3) {
            xRange.Add(startX);
            yRange.Add(startY);
            currentX = startX;
            currentY = startY;
        }
    }
    if (!std::isfinite(xRange.minimum) || !std::isfinite(yRange.minimum)) return store();
    CairoMatrix inverse = state->matrix;
    if (cairo_matrix_invert(&inverse) != kSuccess) return store();
    PathRange usersX, usersY;
    for (double deviceX : {xRange.minimum, xRange.maximum}) {
        for (double deviceY : {yRange.minimum, yRange.maximum}) {
            double x = deviceX, y = deviceY;
            UserPoint(state, inverse, x, y);
            usersX.Add(x);
            usersY.Add(y);
        }
    }
    px1 = usersX.minimum;
    py1 = usersY.minimum;
    px2 = usersX.maximum;
    py2 = usersY.maximum;
    store();
}

void APS5_VABI cairo_append_path(CairoContext* context, const CairoPath* path) {
    auto* state = Valid(context);
    if (!state) return;
    if (path && path->status != kSuccess) {
        state->status = path->status;
        return;
    }
    if (!path || path->num_data < 0 || (path->num_data > 0 && !path->data)) {
        state->status = kNullPointer;
        return;
    }
    for (int i = 0; i < path->num_data && state->status == kSuccess;) {
        const auto header = path->data[i].header;
        const int expected = header.type == 2 ? 4 : header.type == 3 ? 1 :
                             (header.type == 0 || header.type == 1) ? 2 : 0;
        if (!expected || header.length < expected || header.length > path->num_data - i) {
            state->status = kInvalidPathData;
            return;
        }
        const CairoPathData* element = path->data + i;
        if (header.type == 0)
            cairo_move_to(context, element[1].point.x, element[1].point.y);
        else if (header.type == 1)
            cairo_line_to(context, element[1].point.x, element[1].point.y);
        else if (header.type == 2)
            cairo_curve_to(context, element[1].point.x, element[1].point.y,
                           element[2].point.x, element[2].point.y,
                           element[3].point.x, element[3].point.y);
        else
            cairo_close_path(context);
        i += header.length;
    }
}

void APS5_VABI cairo_path_destroy(CairoPath* path) {
    if (!path) return;
    delete[] path->data;
    delete path;
}

}
