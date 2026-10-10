#include <Cli.hpp>
#include <Testing/Test.hpp>

#include <csignal>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>

namespace {

using namespace Testing;

constexpr std::string_view ChildPrefix = "anyps5-autorun-child-";
constexpr int ChildExitCode = 42;

std::filesystem::path& ExecutablePath() {
    static std::filesystem::path path;
    return path;
}

class InputRedirect {
public:
    InputRedirect() : input("\n\n\n"), saved(std::cin.rdbuf(input.rdbuf())) {}
    ~InputRedirect() {
        std::cin.rdbuf(saved);
        std::cin.clear();
    }
    InputRedirect(const InputRedirect&) = delete;
    InputRedirect& operator=(const InputRedirect&) = delete;

private:
    std::istringstream input;
    std::streambuf* saved;
};

int AutorunCopy(const std::string& fileName, bool toWindows) {
    const TemporaryDirectory directory;
    const auto child = directory.Path() / (fileName + ExecutablePath().extension().string());
    std::filesystem::copy_file(ExecutablePath(), child);
    const InputRedirect input;
    return Cli::Autorun(child.string(), toWindows);
}

std::string ExitingChild(std::string_view suffix) {
    return std::string(ChildPrefix) + "exit with spaces" + std::string(suffix);
}

const Case posixModeKeepsExitCode{"Autorun_PosixModeChildExits42_Returns42", [] {
    const int code = AutorunCopy(ExitingChild(""), false);

    RequireEqual(code, ChildExitCode, "Autorun exit code");
}};

const Case windowsModeKeepsExitCode{"Autorun_WindowsModeChildExits42_Returns42", [] {
    const int code = AutorunCopy(ExitingChild(""), true);

    RequireEqual(code, ChildExitCode, "Autorun exit code");
}};

#ifndef _WIN32
void RequireExitCodeInBothModes(std::string_view suffix) {
    RequireEqual(AutorunCopy(ExitingChild(suffix), false), ChildExitCode, "Autorun exit code in posix mode");
    RequireEqual(AutorunCopy(ExitingChild(suffix), true), ChildExitCode, "Autorun exit code in windows mode");
}

const Case shellVariablePath{"Autorun_PathWithShellVariable_Returns42", [] {
    RequireExitCodeInBothModes(" $HOME");
}};

const Case commandSubstitutionPath{"Autorun_PathWithCommandSubstitution_Returns42", [] {
    RequireExitCodeInBothModes(" $(printf substituted)");
}};

const Case backtickPath{"Autorun_PathWithBackticks_Returns42", [] {
    RequireExitCodeInBothModes(" `printf substituted`");
}};

const Case singleQuotePath{"Autorun_PathWithSingleQuotes_Returns42", [] {
    RequireExitCodeInBothModes(" 'single'");
}};

const Case doubleQuotePath{"Autorun_PathWithDoubleQuotes_Returns42", [] {
    RequireExitCodeInBothModes(" \"double\"");
}};

const Case backslashPath{"Autorun_PathWithBackslash_Returns42", [] {
    RequireExitCodeInBothModes(" \\backslash");
}};

const Case lineBreakPath{"Autorun_PathWithLineBreak_Returns42", [] {
    RequireExitCodeInBothModes(" line\nbreak");
}};

const Case signaledChild{"Autorun_ChildKilledBySigill_Returns128PlusSignal", [] {
    const int code = AutorunCopy(std::string(ChildPrefix) + "signal with spaces", false);

    RequireEqual(code, 128 + SIGILL, "Autorun exit code");
}};
#endif

} // namespace

int main(int argc, char** argv) {
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto name = executable.stem().string();
    if (name.starts_with(ChildPrefix)) {
        if (name.find("signal") != std::string::npos) std::raise(SIGILL);
        return ChildExitCode;
    }
    ExecutablePath() = executable;
    return Testing::Run(argc, argv);
}
