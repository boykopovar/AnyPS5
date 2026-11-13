#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace NKWebKit {

class FrameHandle final : public ObjectOf<ObjectType::FrameHandle> {
public:
    FrameHandle() = default;
    explicit FrameHandle(std::uint64_t frameID) : frameID(frameID) {}

    std::uint64_t frameID = 0;
};

}  // namespace NKWebKit
