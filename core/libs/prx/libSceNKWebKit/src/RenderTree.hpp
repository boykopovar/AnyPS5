#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <utility>

namespace NKWebKit {

class RenderObject final : public ObjectOf<ObjectType::RenderObject> {
public:
    Retained<String> elementID;
    Retained<String> elementTagName;
    Retained<String> name;
    Retained<String> textSnippet;
    Retained<Array> elementClassNames;
    Retained<Array> children;
    RectValue frameRect{{0.0, 0.0}, {0.0, 0.0}};
    PointValue absolutePosition{0.0, 0.0};
    std::uint32_t textLength = 0;
};

class RenderLayer final : public ObjectOf<ObjectType::RenderLayer> {
public:
    Retained<RenderObject> renderer;
    Retained<RenderLayer> frameContentsLayer;
    Retained<String> rendererName;
    Retained<String> elementTagName;
    Retained<String> elementID;
    Retained<Array> elementClassNames;
    Retained<Array> negativeZOrderList;
    Retained<Array> positiveZOrderList;
    Retained<Array> normalFlowList;
    bool isClipping = false;
    bool isClipped = false;
    bool isReflection = false;
    std::uint32_t compositingLayerType = 0;
    std::uint32_t backingStoreMemoryEstimate = 0;
    RectValue absoluteBounds{{0.0, 0.0}, {0.0, 0.0}};
};

}
