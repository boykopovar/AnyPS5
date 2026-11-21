#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace NKWebKit {

class BundleBackForwardListItem final : public ObjectOf<ObjectType::BundleBackForwardListItem> {
public:
    BundleBackForwardListItem() = default;
    BundleBackForwardListItem(
        std::optional<std::string> url,
        Retained<String> title,
        std::optional<std::string> originalURL = std::nullopt,
        Retained<String> target = nullptr,
        bool isTargetItem = false,
        std::vector<Retained<BundleBackForwardListItem>> children = {},
        bool inBackForwardCache = false,
        bool hasCachedPageExpired = false,
        std::uint64_t identifier = 0)
        : url(std::move(url)),
          title(std::move(title)),
          originalURL(std::move(originalURL)),
          target(std::move(target)),
          isTargetItem(isTargetItem),
          children(std::move(children)),
          inBackForwardCache(inBackForwardCache),
          hasCachedPageExpired(hasCachedPageExpired),
          identifier(identifier) {}

    std::optional<std::string> url;
    Retained<String> title;
    std::optional<std::string> originalURL;
    Retained<String> target;
    bool isTargetItem = false;
    std::vector<Retained<BundleBackForwardListItem>> children;
    bool inBackForwardCache = false;
    bool hasCachedPageExpired = false;
    std::uint64_t identifier = 0;
};

class BundleBackForwardList final : public ObjectOf<ObjectType::BundleBackForwardList> {
public:
    BundleBackForwardList() = default;
    BundleBackForwardList(std::vector<Retained<BundleBackForwardListItem>> entries, std::optional<std::size_t> currentIndex)
        : entries(std::move(entries)), currentIndex(currentIndex) {}

    std::vector<Retained<BundleBackForwardListItem>> entries;
    std::optional<std::size_t> currentIndex;
    bool attached = true;
};

}
