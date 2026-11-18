#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace NKWebKit {

enum class MediaCaptureType : std::uint32_t {
    None = 0,
    User = 1,
    Environment = 2
};

class OpenPanelParameters final : public ObjectOf<ObjectType::OpenPanelParameters> {
public:
    OpenPanelParameters() = default;
    OpenPanelParameters(bool allowsDirectories, bool allowsMultipleFiles, std::vector<Retained<String>> acceptMIMETypes, std::vector<Retained<String>> acceptFileExtensions, std::vector<Retained<String>> selectedFiles, Retained<String> capture = nullptr, MediaCaptureType mediaCaptureType = MediaCaptureType::None)
        : acceptMIMETypes(std::move(acceptMIMETypes)), acceptFileExtensions(std::move(acceptFileExtensions)), selectedFiles(std::move(selectedFiles)), capture(std::move(capture)), mediaCaptureType(mediaCaptureType), allowsDirectories(allowsDirectories), allowsMultipleFiles(allowsMultipleFiles) {}

    std::vector<Retained<String>> acceptMIMETypes;
    std::vector<Retained<String>> acceptFileExtensions;
    std::vector<Retained<String>> selectedFiles;
    Retained<String> capture;
    MediaCaptureType mediaCaptureType = MediaCaptureType::None;
    bool allowsDirectories = false;
    bool allowsMultipleFiles = false;
};

}  // namespace NKWebKit
