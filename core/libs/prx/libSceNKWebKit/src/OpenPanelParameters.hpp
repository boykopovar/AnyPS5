#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <utility>
#include <vector>

namespace NKWebKit {

class OpenPanelParameters final : public ObjectOf<ObjectType::OpenPanelParameters> {
public:
    OpenPanelParameters() = default;
    OpenPanelParameters(bool allowsDirectories, bool allowsMultipleFiles, std::vector<Retained<String>> acceptMIMETypes, std::vector<Retained<String>> acceptFileExtensions, std::vector<Retained<String>> selectedFiles)
        : acceptMIMETypes(std::move(acceptMIMETypes)), acceptFileExtensions(std::move(acceptFileExtensions)), selectedFiles(std::move(selectedFiles)), allowsDirectories(allowsDirectories), allowsMultipleFiles(allowsMultipleFiles) {}

    std::vector<Retained<String>> acceptMIMETypes;
    std::vector<Retained<String>> acceptFileExtensions;
    std::vector<Retained<String>> selectedFiles;
    bool allowsDirectories = false;
    bool allowsMultipleFiles = false;
};

}  // namespace NKWebKit
