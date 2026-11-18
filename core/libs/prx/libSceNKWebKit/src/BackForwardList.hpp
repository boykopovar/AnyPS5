#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace NKWebKit {

class BackForwardListItem final : public ObjectOf<ObjectType::BackForwardListItem> {
public:
    BackForwardListItem() = default;
    BackForwardListItem(std::optional<std::string> url, Retained<String> title, std::optional<std::string> originalURL)
        : title(std::move(title)), url(std::move(url)), originalURL(std::move(originalURL)) {}

    Retained<String> title;
    std::optional<std::string> url;
    std::optional<std::string> originalURL;
};

class BackForwardList final : public ObjectOf<ObjectType::BackForwardList> {
public:
    BackForwardList() = default;
    BackForwardList(std::vector<Retained<BackForwardListItem>> entries, std::optional<std::size_t> currentIndex)
        : entries(std::move(entries)), currentIndex(currentIndex) {}

    std::vector<Retained<BackForwardListItem>> entries;
    std::optional<std::size_t> currentIndex;
    bool attached = true;
};

}  // namespace NKWebKit
