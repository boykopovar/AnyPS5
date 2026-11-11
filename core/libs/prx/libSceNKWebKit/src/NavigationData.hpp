#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <optional>
#include <string>
#include <utility>

namespace NKWebKit {

class NavigationData final : public ObjectOf<ObjectType::NavigationData> {
public:
    NavigationData() = default;
    NavigationData(Retained<String> title, std::optional<std::string> url, std::optional<std::string> originalRequestURL)
        : title(std::move(title)), url(std::move(url)), originalRequestURL(std::move(originalRequestURL)) {}

    Retained<String> title;
    std::optional<std::string> url;
    std::optional<std::string> originalRequestURL;
};

}  // namespace NKWebKit
