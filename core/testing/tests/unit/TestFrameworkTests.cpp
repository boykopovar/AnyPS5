#include <Testing/Test.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

using namespace Testing;

const Case requirePassesOnTrueCondition{"Require_TrueCondition_DoesNotThrow", [] {
    Require(true, "a true condition");
}};

const Case requireFailsWithMessageAndLocation{"Require_FalseCondition_ThrowsFailureWithMessageAndLine", [] {
    const auto failure = RequireThrows<Failure>([] { Require(false, "the reason"); }, "Require(false)");
    RequireEqual(std::string(failure.what()), std::string("the reason"), "failure message");
    Require(failure.Location().line() > 0, "failure records the line");
    Require(std::string(failure.Location().file_name()).ends_with("TestFrameworkTests.cpp"), "failure records the calling file");
}};

const Case requireEqualPassesOnEqualValues{"RequireEqual_EqualValues_DoesNotThrow", [] {
    RequireEqual(42, 42, "equal integers");
    RequireEqual(std::string("abc"), std::string("abc"), "equal strings");
}};

const Case requireEqualDescribesBothValues{"RequireEqual_DifferentValues_ReportsExpectedAndActual", [] {
    const auto failure = RequireThrows<Failure>([] { RequireEqual(7, 9, "count"); }, "RequireEqual(7, 9)");
    RequireEqual(std::string(failure.what()), std::string("count: expected 9, got 7"), "failure message");
}};

const Case requireEqualPrintsBytesAsNumbers{"RequireEqual_ByteValues_PrintsNumbers", [] {
    const auto failure = RequireThrows<Failure>(
        [] { RequireEqual(std::uint8_t{0x41}, std::uint8_t{0x42}, "byte"); }, "RequireEqual(bytes)");
    RequireEqual(std::string(failure.what()), std::string("byte: expected 66, got 65"), "failure message");
}};

const Case requireThrowsReturnsTheError{"RequireThrows_MatchingException_ReturnsIt", [] {
    const auto error = RequireThrows<std::out_of_range>([] { throw std::out_of_range("past the end"); }, "throwing operation");
    RequireEqual(std::string(error.what()), std::string("past the end"), "returned error");
}};

const Case requireThrowsFailsWithoutException{"RequireThrows_NoException_Fails", [] {
    const auto failure = RequireThrows<Failure>(
        [] { RequireThrows<std::runtime_error>([] {}, "silent operation"); }, "nested RequireThrows");
    RequireEqual(std::string(failure.what()), std::string("silent operation: did not throw"), "failure message");
}};

const Case requireThrowsFailsOnOtherException{"RequireThrows_DifferentException_Fails", [] {
    const auto failure = RequireThrows<Failure>(
        [] { RequireThrows<std::out_of_range>([] { throw std::invalid_argument("bad"); }, "wrong type"); },
        "nested RequireThrows");
    RequireEqual(std::string(failure.what()), std::string("wrong type: threw an unexpected exception: bad"), "failure message");
}};

const Case requireThrowsWithMessageChecksText{"RequireThrowsWithMessage_DifferentText_Fails", [] {
    RequireThrowsWithMessage<std::runtime_error>([] { throw std::runtime_error("exact"); }, "exact", "matching text");
    RequireThrows<Failure>([] {
        RequireThrowsWithMessage<std::runtime_error>([] { throw std::runtime_error("other"); }, "exact", "text");
    }, "mismatching text");
}};

const Case requireThrowsBaseTypeKeepsFailures{"RequireThrows_BaseTypeAndFailingAssertion_ReportsTheAssertion", [] {
    const auto failure = RequireThrows<Failure>(
        [] { RequireThrows<std::exception>([] { Require(false, "inner assertion"); }, "base type"); }, "nested RequireThrows");
    RequireEqual(std::string(failure.what()), std::string("inner assertion"), "inner failure is not swallowed");
}};

const Case requireThrowsMessageKeepsDerivedText{"RequireThrowsMessage_BaseType_ReturnsDerivedMessage", [] {
    const auto text = RequireThrowsMessage<std::exception>([] { throw std::runtime_error("derived text"); }, "base type");
    RequireEqual(text, std::string("derived text"), "message of the derived exception");
}};

const Case requireThrowsContaining{"RequireThrowsContaining_MissingText_Fails", [] {
    RequireThrowsContaining<std::exception>([] { throw std::runtime_error("one two three"); }, "two", "contained text");
    RequireThrows<Failure>([] {
        RequireThrowsContaining<std::exception>([] { throw std::runtime_error("one"); }, "two", "text");
    }, "missing text");
}};

const Case skipThrowsSkippedWithReason{"Skip_Called_ThrowsSkippedWithReason", [] {
    const auto skipped = RequireThrows<Skipped>([] { Skip("no device"); }, "Skip");
    RequireEqual(std::string(skipped.what()), std::string("no device"), "skip reason");
}};

const Case temporaryDirectoryIsRemoved{"TemporaryDirectory_Destroyed_RemovesItsContents", [] {
    std::filesystem::path path;
    {
        const TemporaryDirectory directory;
        path = directory.Path();
        Require(std::filesystem::is_directory(path), "directory exists while alive");
        std::ofstream(path / "file.bin") << "data";
    }
    Require(!std::filesystem::exists(path), "directory removed on destruction");
}};

const Case temporaryDirectoriesAreDistinct{"TemporaryDirectory_TwoInstances_UseDifferentPaths", [] {
    const TemporaryDirectory first;
    const TemporaryDirectory second;
    Require(first.Path() != second.Path(), "paths differ");
}};

} // namespace
