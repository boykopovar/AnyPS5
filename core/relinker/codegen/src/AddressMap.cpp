#include <codegen/AddressMap.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <limits>
#include <utility>

namespace Codegen {

namespace {

std::int64_t CheckedAdd(const std::int64_t left, const std::int64_t right, const std::int64_t offset) {
    if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right)
        throw CodegenException("Address mapping overflows the offset space", static_cast<Domain::FileByteOffset>(offset));
    if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)
        throw CodegenException("Address mapping underflows the offset space", static_cast<Domain::FileByteOffset>(offset));
    return left + right;
}

Domain::FileByteOffset FailureAt(const std::int64_t offset) {
    return offset < 0 ? 0 : static_cast<Domain::FileByteOffset>(offset);
}

}

AddressMap::AddressMap(std::vector<LayoutShift> shifts) : _shifts(std::move(shifts)) {
    std::sort(_shifts.begin(), _shifts.end(), [](const LayoutShift& left, const LayoutShift& right) {
        return left.Begin < right.Begin;
    });
    for (std::size_t index = 0; index < _shifts.size(); ++index) {
        const auto& shift = _shifts[index];
        if (shift.Begin < 0)
            throw CodegenException("Layout shift begins before the code section");
        if (shift.End <= shift.Begin)
            throw CodegenException("Layout shift does not cover any code", FailureAt(shift.Begin));
        if (shift.Delta == 0)
            throw CodegenException("Layout shift does not change the code length", FailureAt(shift.Begin));
        if (index != 0 && shift.Begin < _shifts[index - 1].End)
            throw CodegenException("Layout shifts overlap", FailureAt(shift.Begin));
        _totalDelta = CheckedAdd(_totalDelta, shift.Delta, shift.Begin);
    }
}

bool AddressMap::IsIdentity() const {
    return _shifts.empty();
}

std::int64_t AddressMap::TotalDelta() const {
    return _totalDelta;
}

std::int64_t AddressMap::MapAddress(const std::int64_t oldOffset) const {
    std::int64_t mapped = oldOffset;
    for (const auto& shift : _shifts) {
        if (shift.End <= oldOffset) {
            mapped = CheckedAdd(mapped, shift.Delta, oldOffset);
            continue;
        }
        if (shift.Begin < oldOffset)
            throw CodegenException("Address lies inside a rewritten instruction", FailureAt(oldOffset));
        break;
    }
    return mapped;
}

AddressRange AddressMap::MapRange(const std::int64_t start, const std::int64_t size) const {
    if (size < 0)
        throw CodegenException("Address range size is negative", FailureAt(start));
    if (size > 0 && start > std::numeric_limits<std::int64_t>::max() - size)
        throw CodegenException("Address range end overflows the offset space", FailureAt(start));
    const auto begin = MapAddress(start);
    const auto end = MapAddress(start + size);
    return {begin, end - begin};
}

const std::vector<LayoutShift>& AddressMap::Shifts() const {
    return _shifts;
}

}