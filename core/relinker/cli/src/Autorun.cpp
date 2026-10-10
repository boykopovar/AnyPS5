#include <Cli.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Cli {

int Autorun(const std::string& absPath, bool toWindows) {
    if (!toWindows) {
        std::filesystem::permissions(Io::NativePath(absPath),
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add);
    }

    int rawCode = 0;
    int exitCode = 0;

#ifdef _WIN32
    std::wstring command = L"\"" + Io::NativePath(absPath).wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
        throw Domain::RelinkerException("Cannot run the output executable: " + absPath);
    if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0) {
        const unsigned long waitError = GetLastError();
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        throw Domain::RelinkerException("Cannot wait for the output executable: " + absPath + " (error " + std::to_string(waitError) + ")");
    }
    unsigned long rawExitCode = 0;
    if (!GetExitCodeProcess(process.hProcess, &rawExitCode)) {
        const unsigned long exitError = GetLastError();
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        throw Domain::RelinkerException("Cannot read the output executable exit code: " + absPath + " (error " + std::to_string(exitError) + ")");
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    rawCode = static_cast<int>(rawExitCode);
    exitCode = rawCode;
    std::cout << "\nExit code: " << rawCode << '\n';
#else
    std::string cmd = "'";
    for (const char character : absPath) {
        if (character == '\'') cmd += "'\\''";
        else cmd += character;
    }
    cmd += '\'';
    rawCode = std::system(cmd.c_str());

    if (rawCode != -1) {
        const int signal = rawCode & 0x7F;
        exitCode = signal ? 128 + signal : (rawCode >> 8) & 0xFF;
    }
    std::cout << "\nRaw exit code: " << rawCode << "; Unpacked: " << exitCode << '\n';
#endif

    std::cout << "\nPress Enter to exit...\n";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    return exitCode;
}

}
