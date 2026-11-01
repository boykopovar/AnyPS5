#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>
#include <new>

namespace {

constexpr std::int32_t Magic = 0x55434600;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t InvalidFormatError = 3;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t CategoryUndefined = 0;

enum class Constraint : std::int8_t {
    None,
    Category,
    Field
};

struct Position {
    std::int32_t magic = Magic;
    std::int32_t field = 0;
    std::int32_t start = 0;
    std::int32_t limit = 0;
    std::int32_t category = CategoryUndefined;
    Constraint constraint = Constraint::None;
};

Position* Validate(const UConstrainedFieldPosition* handle, std::int32_t& error) {
    if (error > 0) return nullptr;
    if (!handle) {
        error = IllegalArgumentError;
        return nullptr;
    }
    std::int32_t magic = 0;
    std::memcpy(&magic, handle, sizeof(magic));
    if (magic != Magic) {
        error = InvalidFormatError;
        return nullptr;
    }
    return reinterpret_cast<Position*>(const_cast<UConstrainedFieldPosition*>(handle));
}

Position* Validate(const UConstrainedFieldPosition* handle, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return Validate(handle, *error);
}

}

extern "C" {

UConstrainedFieldPosition* APS5_VABI ucfpos_open_nid_postfix(std::int32_t* error) {
    auto* position = new (std::nothrow) Position();
    if (!position) {
        if (!error) APS5_INVALID_ARG_EX;
        *error = MemoryAllocationError;
        return nullptr;
    }
    return reinterpret_cast<UConstrainedFieldPosition*>(position);
}

void APS5_VABI ucfpos_close_nid_postfix(UConstrainedFieldPosition* handle) {
    std::int32_t error = 0;
    auto* position = Validate(handle, error);
    if (!position) return;
    position->magic = 0;
    delete position;
}

void APS5_VABI ucfpos_constrainCategory_nid_postfix(UConstrainedFieldPosition* handle, std::int32_t category, std::int32_t* error) {
    auto* position = Validate(handle, error);
    if (!position) return;
    position->constraint = Constraint::Category;
    position->category = category;
}

void APS5_VABI ucfpos_constrainField_nid_postfix(UConstrainedFieldPosition* handle, std::int32_t category, std::int32_t field, std::int32_t* error) {
    auto* position = Validate(handle, error);
    if (!position) return;
    position->constraint = Constraint::Field;
    position->category = category;
    position->field = field;
}

std::int32_t APS5_VABI ucfpos_getCategory_nid_postfix(const UConstrainedFieldPosition* handle, std::int32_t* error) {
    const auto* position = Validate(handle, error);
    return position ? position->category : CategoryUndefined;
}

std::int32_t APS5_VABI ucfpos_getField_nid_postfix(const UConstrainedFieldPosition* handle, std::int32_t* error) {
    const auto* position = Validate(handle, error);
    return position ? position->field : 0;
}

void APS5_VABI ucfpos_getIndexes_nid_postfix(const UConstrainedFieldPosition* handle, std::int32_t* start, std::int32_t* limit, std::int32_t* error) {
    const auto* position = Validate(handle, error);
    if (!position) return;
    if (!start || !limit) APS5_INVALID_ARG_EX;
    *start = position->start;
    *limit = position->limit;
}

}
