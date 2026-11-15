#include "prx/libcairo/include/Matrix.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Check(bool ok, int line) {
    if (!ok) {
        std::fprintf(stderr, "Cairo matrix guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TCall>
void Rejects(TCall callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
        Require(data != nullptr);
#ifndef _WIN32
        Require(data != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    template <typename T>
    T* Set(const T& value) {
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, bytes, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

void CheckMatrixOutputs() {
    auto* bad = reinterpret_cast<CairoMatrix*>(std::uintptr_t{0x10});
    Rejects([&] { cairo_matrix_init(bad, 1, 2, 3, 4, 5, 6); });
    Rejects([&] { cairo_matrix_init_identity(bad); });
    Rejects([&] { cairo_matrix_init_translate(bad, 5, 6); });
    Rejects([&] { cairo_matrix_translate(bad, 5, 6); });
    Rejects([&] { cairo_matrix_scale(bad, 5, 6); });
    Rejects([&] { cairo_matrix_rotate(bad, 0.5); });
    Rejects([&] { cairo_matrix_invert(bad); });

    const CairoMatrix original{2, 0, 0, 3, 5, 7};
    ReadOnlyPage page;
    CairoMatrix* readonly = page.Set(original);
    page.Protect();
    Rejects([&] { cairo_matrix_init(readonly, 1, 2, 3, 4, 5, 6); });
    Rejects([&] { cairo_matrix_init_identity(readonly); });
    Rejects([&] { cairo_matrix_init_translate(readonly, 5, 6); });
    Rejects([&] { cairo_matrix_translate(readonly, 5, 6); });
    Rejects([&] { cairo_matrix_scale(readonly, 5, 6); });
    Rejects([&] { cairo_matrix_rotate(readonly, 0.5); });
    Rejects([&] { cairo_matrix_invert(readonly); });
    Require(std::memcmp(readonly, &original, sizeof(original)) == 0);
}

void CheckMatrixInputs() {
    const CairoMatrix valid{2, 0, 0, 3, 5, 7};
    const CairoMatrix unchanged{8, 9, 10, 11, 12, 13};
    auto* bad = reinterpret_cast<const CairoMatrix*>(std::uintptr_t{0x10});
    CairoMatrix result = unchanged;
    Rejects([&] { cairo_matrix_multiply(&result, bad, &valid); });
    Require(std::memcmp(&result, &unchanged, sizeof(result)) == 0);
    Rejects([&] { cairo_matrix_multiply(&result, &valid, bad); });
    Require(std::memcmp(&result, &unchanged, sizeof(result)) == 0);
    Rejects([&] { cairo_matrix_multiply(const_cast<CairoMatrix*>(bad), &valid, &valid); });

    ReadOnlyPage page;
    CairoMatrix* readonly = page.Set(valid);
    page.Protect();
    cairo_matrix_multiply(&result, readonly, &valid);
    Require(result.xx == 4 && result.yy == 9 && result.x0 == 15 && result.y0 == 28);
    Rejects([&] { cairo_matrix_multiply(readonly, &valid, &valid); });
    Require(std::memcmp(readonly, &valid, sizeof(valid)) == 0);
}

void CheckCoordinateInputsAndOutputs() {
    const CairoMatrix matrix{2, 0, 0, 3, 5, 7};
    auto* badMatrix = reinterpret_cast<const CairoMatrix*>(std::uintptr_t{0x10});
    auto* badDouble = reinterpret_cast<double*>(std::uintptr_t{0x10});
    double x = 2, y = 4;
    Rejects([&] { cairo_matrix_transform_point(badMatrix, &x, &y); });
    Require(x == 2 && y == 4);
    Rejects([&] { cairo_matrix_transform_distance(badMatrix, &x, &y); });
    Require(x == 2 && y == 4);

    Rejects([&] { cairo_matrix_transform_point(&matrix, badDouble, &y); });
    Require(y == 4);
    Rejects([&] { cairo_matrix_transform_point(&matrix, &x, badDouble); });
    Require(x == 2);
    Rejects([&] { cairo_matrix_transform_distance(&matrix, badDouble, &y); });
    Require(y == 4);
    Rejects([&] { cairo_matrix_transform_distance(&matrix, &x, badDouble); });
    Require(x == 2);

    ReadOnlyPage page;
    double* readonly = page.Set(7.0);
    page.Protect();
    Rejects([&] { cairo_matrix_transform_point(&matrix, readonly, &y); });
    Require(y == 4 && *readonly == 7);
    Rejects([&] { cairo_matrix_transform_point(&matrix, &x, readonly); });
    Require(x == 2 && *readonly == 7);
    Rejects([&] { cairo_matrix_transform_distance(&matrix, readonly, &y); });
    Require(y == 4 && *readonly == 7);
    Rejects([&] { cairo_matrix_transform_distance(&matrix, &x, readonly); });
    Require(x == 2 && *readonly == 7);

    ReadOnlyPage readonlyMatrixPage;
    CairoMatrix* readOnlyMatrix = readonlyMatrixPage.Set(matrix);
    readonlyMatrixPage.Protect();
    cairo_matrix_transform_point(readOnlyMatrix, &x, &y);
    Require(x == 9 && y == 19);

    double aliased = 2;
    cairo_matrix_transform_point(&matrix, &aliased, &aliased);
    Require(aliased == 13);
    aliased = 2;
    cairo_matrix_transform_distance(&matrix, &aliased, &aliased);
    Require(aliased == 6);
}

}

int main() {
    CheckMatrixOutputs();
    CheckMatrixInputs();
    CheckCoordinateInputsAndOutputs();
    std::puts("guest Cairo matrix pointer checks passed");
}
