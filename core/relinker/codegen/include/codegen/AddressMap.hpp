#ifndef CODEGEN_ADDRESSMAP_HPP
#define CODEGEN_ADDRESSMAP_HPP

#include <cstdint>
#include <vector>

namespace Codegen {

struct LayoutShift {
    std::int64_t Begin;
    std::int64_t End;
    std::int64_t Delta;

    bool operator==(const LayoutShift&) const = default;
};

struct AddressRange {
    std::int64_t Begin;
    std::int64_t Size;

    bool operator==(const AddressRange&) const = default;
};

class AddressMap {
public:
    AddressMap() = default;

    explicit AddressMap(std::vector<LayoutShift> shifts);

    [[nodiscard]] bool IsIdentity() const;

    [[nodiscard]] std::int64_t TotalDelta() const;

    [[nodiscard]] std::int64_t MapAddress(std::int64_t oldOffset) const;

    [[nodiscard]] AddressRange MapRange(std::int64_t start, std::int64_t size) const;

    [[nodiscard]] const std::vector<LayoutShift>& Shifts() const;

private:
    std::vector<LayoutShift> _shifts;
    std::int64_t _totalDelta = 0;
};

}

#endif