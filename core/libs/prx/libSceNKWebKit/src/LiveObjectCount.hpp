#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"

#include <cstdint>

extern "C" void AdjustLiveObjectCount_nid_no_patch(NKWebKit::ObjectType kind, std::int32_t delta);

namespace NKWebKit {

template<ObjectType kind>
class LiveObjectCount {
public:
    LiveObjectCount() { AdjustLiveObjectCount_nid_no_patch(kind, 1); }
    LiveObjectCount(const LiveObjectCount&) = delete;
    LiveObjectCount& operator=(const LiveObjectCount&) = delete;
    ~LiveObjectCount() { AdjustLiveObjectCount_nid_no_patch(kind, -1); }
};

}
