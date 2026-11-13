#include "../src/WindowsFileOutput.hpp"
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(file.is_open(), "Cannot read failure-test file: " + Io::Utf8Path(path));
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct Probe {
    std::filesystem::path Target;
    std::filesystem::path Backup;
    std::filesystem::path Replacement;
    HANDLE BackupHandle = INVALID_HANDLE_VALUE;
    std::size_t Writes = 0;
    std::size_t Bytes = 0;
    std::size_t Replaces = 0;
    std::size_t Moves = 0;
};

Probe probe;
const std::string previous = "previous output must survive";

std::vector<std::uint8_t> daclBytes(const std::filesystem::path& path) {
    DWORD length = 0;
    GetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &length);
    require(length != 0, "Cannot size the security fixture");
    std::vector<std::uint8_t> descriptor(length);
    require(GetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION, descriptor.data(), length, &length) != 0, "Cannot read the security fixture");
    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    require(GetSecurityDescriptorDacl(descriptor.data(), &present, &dacl, &defaulted) && present && dacl, "The fixture has no DACL");
    const auto bytes = reinterpret_cast<const std::uint8_t*>(dacl);
    return {bytes, bytes + dacl->AclSize};
}

void makePrivate(const std::filesystem::path& path, DWORD rights = FILE_ALL_ACCESS) {
    HANDLE token = nullptr;
    require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != 0, "Cannot open the test process token");
    const std::unique_ptr<void, decltype(&CloseHandle)> held(token, CloseHandle);
    DWORD length = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &length);
    std::vector<std::uint8_t> information(length);
    require(GetTokenInformation(token, TokenUser, information.data(), length, &length) != 0, "Cannot read the test user");
    const auto sid = reinterpret_cast<const TOKEN_USER*>(information.data())->User.Sid;
    std::vector<std::uint8_t> bytes(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(sid));
    const auto dacl = reinterpret_cast<PACL>(bytes.data());
    require(InitializeAcl(dacl, static_cast<DWORD>(bytes.size()), ACL_REVISION) != 0, "Cannot initialize the private DACL");
    require(AddAccessAllowedAce(dacl, ACL_REVISION, rights, sid) != 0, "Cannot grant the test user access");
    SECURITY_DESCRIPTOR descriptor;
    require(InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) != 0, "Cannot initialize fixture security");
    require(SetSecurityDescriptorDacl(&descriptor, TRUE, dacl, FALSE) != 0, "Cannot set the private DACL");
    require(SetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, &descriptor) != 0, "Cannot protect the fixture");
}

BOOL WINAPI privateReplacement(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
    const auto original = daclBytes(target);
    require(daclBytes(replacement) == original, "The staged data has broader or different access than the target");
    return ReplaceFileW(target, replacement, backup, flags, exclude, reserved);
}

BOOL WINAPI shortWrite(HANDLE handle, LPCVOID data, DWORD size, LPDWORD written, LPOVERLAPPED overlapped) {
    require(readFile(probe.Target) == previous, "Writing a stage changed the original output before replacement");
    ++probe.Writes;
    const auto result = WriteFile(handle, data, std::min<DWORD>(size, 2), written, overlapped);
    if (result) probe.Bytes += *written;
    return result;
}

BOOL WINAPI zeroWrite(HANDLE, LPCVOID, DWORD, LPDWORD written, LPOVERLAPPED) {
    ++probe.Writes;
    *written = 0;
    return TRUE;
}

BOOL WINAPI partialWrite(HANDLE handle, LPCVOID data, DWORD size, LPDWORD written, LPOVERLAPPED overlapped) {
    if (probe.Writes == 0) return shortWrite(handle, data, size, written, overlapped);
    ++probe.Writes;
    *written = 0;
    SetLastError(ERROR_DISK_FULL);
    return FALSE;
}

BOOL WINAPI failFlush(HANDLE) {
    SetLastError(ERROR_DISK_FULL);
    return FALSE;
}

BOOL WINAPI failClose(HANDLE) {
    SetLastError(ERROR_WRITE_FAULT);
    return FALSE;
}

BOOL WINAPI observeReplace(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
    ++probe.Replaces;
    require(readFile(target) == previous, "The original changed before its replacement was committed");
    return ReplaceFileW(target, replacement, backup, flags, exclude, reserved);
}

BOOL WINAPI failReplacement(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    ++probe.Replaces;
    probe.Backup = backup;
    probe.Replacement = replacement;
    require(readFile(target) == previous, "The original changed before the replacement-failure probe");
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT);
    return FALSE;
}

BOOL WINAPI moveOriginalThenFail(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    ++probe.Replaces;
    probe.Backup = backup;
    probe.Replacement = replacement;
    require(MoveFileExW(target, backup, 0) != 0, "Cannot move the original into its recovery backup");
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2);
    return FALSE;
}

BOOL WINAPI failRecovery(LPCWSTR, LPCWSTR, DWORD flags) {
    ++probe.Moves;
    require((flags & MOVEFILE_REPLACE_EXISTING) == 0, "Recovery would overwrite another output");
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL WINAPI competingOutput(LPCWSTR source, LPCWSTR target, DWORD flags) {
    ++probe.Moves;
    require((flags & MOVEFILE_REPLACE_EXISTING) == 0, "A new output would overwrite a competing file");
    std::ofstream(std::filesystem::path(target), std::ios::binary) << "competing output";
    return MoveFileExW(source, target, flags);
}

BOOL WINAPI lockBackup(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
    probe.Backup = backup;
    if (!ReplaceFileW(target, replacement, backup, flags, exclude, reserved)) return FALSE;
    probe.BackupHandle = CreateFileW(backup, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    return TRUE;
}

BOOL WINAPI reuseMovedPath(LPCWSTR source, LPCWSTR target, DWORD flags) {
    if (!MoveFileExW(source, target, flags)) return FALSE;
    probe.Replacement = source;
    std::ofstream(probe.Replacement, std::ios::binary) << "another writer";
    return TRUE;
}

BOOL WINAPI reuseReplacedPath(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
    if (!ReplaceFileW(target, replacement, backup, flags, exclude, reserved)) return FALSE;
    probe.Replacement = replacement;
    std::ofstream(probe.Replacement, std::ios::binary) << "another writer";
    return TRUE;
}

std::vector<std::filesystem::path> stages(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.path().filename().string().starts_with(".anyps5-")) result.push_back(entry.path());
    return result;
}

template<typename TAction>
std::string failure(TAction action) {
    try {
        action();
    } catch (const Domain::RelinkerException& error) {
        return error.what();
    }
    throw std::runtime_error("The injected output failure reported success");
}

void run(const std::filesystem::path& root, const std::ios::openmode mode, const std::string& data, const std::string& expected) {
    using Io::Detail::ReplaceWindowsFile;
    using Io::Detail::WindowsFileOperations;
    const auto setup = [&](const std::string& name) {
        const auto directory = root / name;
        std::filesystem::create_directory(directory);
        probe = {};
        probe.Target = directory / std::filesystem::path(u8"output-\u65e5\u672c.bin");
        std::ofstream(probe.Target, std::ios::binary) << previous;
        return directory;
    };
    const auto write = [&](const WindowsFileOperations& operations) {
        return ReplaceWindowsFile(probe.Target, Io::Utf8Path(probe.Target), mode, data.data(), data.size(), operations);
    };
    {
        const auto directory = setup("private");
        makePrivate(probe.Target);
        const auto original = daclBytes(probe.Target);
        WindowsFileOperations operations;
        operations.Replace = privateReplacement;
        require(write(operations), "A private output could not be replaced");
        require(readFile(probe.Target) == expected && daclBytes(probe.Target) == original, "The replacement changed private output data or permissions");
        require(stages(directory).empty(), "The private output left a stage behind");
    }
    {
        const auto directory = setup("backup-cleanup");
        makePrivate(probe.Target, FILE_ALL_ACCESS & ~(DELETE | FILE_DELETE_CHILD));
        require(write({}), "A file deletable through its parent could not be replaced");
        require(readFile(probe.Target) == expected, "Backup cleanup changed the completed bytes");
        require(stages(directory).empty(), "Replacement retained a backup without delete access");
    }
    {
        const auto directory = setup("locked-backup");
        WindowsFileOperations operations;
        operations.Replace = lockBackup;
        std::string message;
        try {
            write(operations);
        } catch (const Domain::RelinkerException& error) {
            message = error.what();
        }
        require(probe.BackupHandle != INVALID_HANDLE_VALUE, "Cannot lock the recovery backup");
        require(CloseHandle(probe.BackupHandle) != 0, "Cannot release the recovery backup");
        require(message.find(Io::Utf8Path(probe.Backup)) != std::string::npos, "A backup cleanup failure did not report its path");
        require(readFile(probe.Target) == expected, "A backup cleanup failure changed the completed output");
        require(readFile(probe.Backup) == previous, "A backup cleanup failure lost the previous output");
        require(stages(directory).size() == 1, "A backup cleanup failure retained unrelated temporary files");
    }
    {
        const auto directory = setup("denied-write");
        makePrivate(probe.Target, FILE_ALL_ACCESS & ~FILE_WRITE_DATA);
        WindowsFileOperations operations;
        operations.Replace = observeReplace;
        failure([&] { write(operations); });
        require(probe.Replaces == 0, "Replacement bypassed the destination write permission");
        require(readFile(probe.Target) == previous, "Replacement changed an output without write permission");
        require(stages(directory).empty(), "A denied output left a stage behind");
    }
    {
        const auto directory = setup("short");
        WindowsFileOperations operations;
        operations.Write = shortWrite;
        operations.Replace = observeReplace;
        require(write(operations), "Short writes did not produce an output");
        require(probe.Writes > 1 && probe.Bytes == expected.size() && probe.Replaces == 1, "Short-write progress was lost");
        require(readFile(probe.Target) == expected, "Short writes changed the completed bytes");
        require(stages(directory).empty(), "Successful short writes leaked a stage");
    }
    for (const auto& name : {"zero", "partial", "flush", "close"}) {
        const auto directory = setup(name);
        WindowsFileOperations operations;
        operations.Replace = observeReplace;
        if (name == std::string("zero")) operations.Write = zeroWrite;
        if (name == std::string("partial")) operations.Write = partialWrite;
        if (name == std::string("flush")) operations.Flush = failFlush;
        if (name == std::string("close")) operations.Close = failClose;
        const auto error = failure([&] { write(operations); });
        require(error == "Failed to write file: " + Io::Utf8Path(probe.Target), "A staged write failure has the wrong diagnostic");
        require(readFile(probe.Target) == previous, "A staged write failure changed the original output");
        require(probe.Replaces == 0, "An incomplete stage reached replacement");
        require(stages(directory).empty(), "A staged write failure leaked its temporary output");
        if (name == std::string("partial")) require(probe.Bytes == 2 && probe.Writes == 2, "The partial-write probe did not write before failing");
    }
    for (const auto& name : {"replace1176", "restore1177", "retained1177"}) {
        const auto directory = setup(name);
        WindowsFileOperations operations;
        operations.Replace = name == std::string("replace1176") ? failReplacement : moveOriginalThenFail;
        if (name == std::string("retained1177")) operations.Move = failRecovery;
        const auto error = failure([&] { write(operations); });
        require(probe.Replaces == 1, "The replacement-failure probe was not reached");
        require(!std::filesystem::exists(probe.Replacement), "Replacement failure retained the incomplete new output");
        if (name == std::string("retained1177")) {
            require(probe.Moves == 1, "The recovery-failure probe was not reached");
            require(!std::filesystem::exists(probe.Target), "The recovery-failure fixture did not move the old output");
            require(readFile(probe.Backup) == previous, "Failed recovery deleted the old output backup");
            require(error.find(Io::Utf8Path(probe.Backup)) != std::string::npos, "Failed recovery did not report the backup path");
            require(stages(directory).size() == 1, "Failed recovery did not keep its backup directory");
        } else {
            require(error == "Cannot open output file: " + Io::Utf8Path(probe.Target), "Replacement failure has the wrong diagnostic");
            require(readFile(probe.Target) == previous, "Replacement failure did not restore the original output");
            require(stages(directory).empty(), "A recoverable replacement failure leaked a stage");
        }
    }
    for (const bool existing : {false, true}) {
        const auto directory = setup(existing ? "reuse-replaced" : "reuse-moved");
        if (!existing) std::filesystem::remove(probe.Target);
        WindowsFileOperations operations;
        operations.Move = reuseMovedPath;
        operations.Replace = reuseReplacedPath;
        require(write(operations), "The staged file could not be promoted");
        require(readFile(probe.Target) == expected, "Promotion changed the completed bytes");
        require(readFile(probe.Replacement) == "another writer", "Cleanup deleted the reused staging name");
        std::filesystem::remove(probe.Replacement);
        require(stages(directory).empty(), "Successful promotion leaked a stage");
    }
    {
        const auto directory = setup("race");
        std::filesystem::remove(probe.Target);
        WindowsFileOperations operations;
        operations.Move = competingOutput;
        const auto error = failure([&] { write(operations); });
        require(error == "Cannot open output file: " + Io::Utf8Path(probe.Target), "A competing output has the wrong diagnostic");
        require(probe.Moves == 1 && readFile(probe.Target) == "competing output", "A newly created output overwrote a competing file");
        require(stages(directory).empty(), "A competing output leaked a stage");
    }
}

}

int main(int argc, char** argv) {
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-windows-output-failures-" + std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected binary or text");
        std::filesystem::create_directory(directory);
        const std::string mode(argv[1]);
        const std::string data("first\n\0last\n", 12);
        if (mode == "binary") run(directory, std::ios::binary, data, data);
        else if (mode == "text") run(directory, std::ios::out, data, std::string("first\r\n\0last\r\n", 14));
        else throw std::runtime_error("Expected binary or text");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        return 1;
    }
    std::filesystem::remove_all(directory);
    std::cout << "15 deterministic Windows output cases passed\n";
    return 0;
}
