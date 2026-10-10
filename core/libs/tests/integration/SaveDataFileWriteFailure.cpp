#include "prx/libSceSaveData/SaveDataFile.hpp"

#include <Testing/Test.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

const std::vector<char> oldData{'o', 'l', 'd'};
const std::vector<char> replacement{'n', 'e', 'w'};

struct ReplaceAttempt {
    int count = 0;
    std::filesystem::path temporary;
    std::filesystem::path destination;
    bool replaced = true;
};

class SaveFileFixture {
public:
    SaveFileFixture() : savePath(directory.Path() / "memory.dat") {
        std::ofstream file(savePath, std::ios::binary);
        Require(file.is_open(), "create the save file");
        file.write(oldData.data(), static_cast<std::streamsize>(oldData.size()));
        Require(static_cast<bool>(file), "write the save file");
    }

    SaveFileFixture(const SaveFileFixture&) = delete;
    SaveFileFixture& operator=(const SaveFileFixture&) = delete;

    std::filesystem::path Temporary() const {
        auto temporary = savePath;
        temporary += ".tmp";
        return temporary;
    }

    ReplaceAttempt ReplaceWithFailingOperation() const {
        ReplaceAttempt attempt;
        attempt.replaced = savedata::replace_file_with(
            savePath, replacement.data(), replacement.size(),
            [&attempt](const std::filesystem::path& temporary, const std::filesystem::path& destination) {
                ++attempt.count;
                attempt.temporary = temporary;
                attempt.destination = destination;
                return false;
            });
        return attempt;
    }

    const std::filesystem::path& SavePath() const { return savePath; }

private:
    Testing::TemporaryDirectory directory;
    const std::filesystem::path savePath;
};

std::vector<char> ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const Case operationArguments{"ReplaceFileWith_FailingReplace_CallsReplaceOnceWithTemporaryAndDestination", [] {
    const SaveFileFixture fixture;
    const auto attempt = fixture.ReplaceWithFailingOperation();
    RequireEqual(attempt.count, 1, "replace attempts");
    Require(attempt.destination == fixture.SavePath(), "destination is the save path");
    Require(attempt.temporary == fixture.Temporary(), "temporary is the save path with .tmp");
}};

const Case returnsFalse{"ReplaceFileWith_FailingReplace_ReturnsFalseAndKeepsOriginal", [] {
    const SaveFileFixture fixture;
    const auto attempt = fixture.ReplaceWithFailingOperation();
    Require(!attempt.replaced, "replace_file_with returns false");
    Require(ReadAll(fixture.SavePath()) == oldData, "save file keeps the old data");
}};

const Case removesTemporary{"ReplaceFileWith_FailingReplace_RemovesTemporary", [] {
    const SaveFileFixture fixture;
    fixture.ReplaceWithFailingOperation();
    Require(!std::filesystem::exists(fixture.Temporary()), "temporary file removed");
}};

} // namespace
