#pragma once

#include <Testing/Test.hpp>

#include <filesystem>
#include <system_error>

class SaveDataWorkingDirectory {
public:
    SaveDataWorkingDirectory() : previous(std::filesystem::current_path()) {
        std::filesystem::current_path(directory.Path());
    }

    ~SaveDataWorkingDirectory() {
        std::error_code ignored;
        std::filesystem::current_path(previous, ignored);
    }

    SaveDataWorkingDirectory(const SaveDataWorkingDirectory&) = delete;
    SaveDataWorkingDirectory& operator=(const SaveDataWorkingDirectory&) = delete;

    const std::filesystem::path& Path() const noexcept {
        return directory.Path();
    }

private:
    const Testing::TemporaryDirectory directory;
    const std::filesystem::path previous;
};
