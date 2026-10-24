#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <climits>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

using Elfpatcher::Windows::WindowsEntryStubBuilder;
using Elfpatcher::Windows::WindowsImports;

namespace {

WindowsImports makeTestImports() {
    WindowsImports imports{};

    const char* functions[] = {
        "RaiseException",
        "lstrlenA",
        "GetStdHandle",
        "WriteFile",
        "FormatMessageA",
        "GetLastError",
        "WideCharToMultiByte",
        "CreateFileW",
        "SetStdHandle",
        "GetEnvironmentVariableW",
        "WaitForSingleObject",
        "CloseHandle",
        "SetEnvironmentVariableW",
        "GetModuleFileNameW",
        "SetCurrentDirectoryW",
        "GetFileAttributesW",
        "LoadLibraryExW",
        "GetProcAddress",
        "LoadLibraryExA",
        "GetCommandLineW",
        "VirtualAlloc",
        "LocalFree",
        "FreeLibrary",
        "ExitProcess"
    };

    std::uint32_t rva = 0x10000u;

    for (const char* name : functions) {
        imports.Functions.emplace(name, rva);
        rva += 8u;
    }

    return imports;
}
void expectBoundaryRejection(std::uint32_t dataRva) {
    try {
        WindowsEntryStubBuilder{}.Build(
            dataRva,
            0x1000u,
            makeTestImports(),
            {},
            {},
            ".",
            false,
            false
        );
    } catch (const Domain::RelinkerException& error) {
        const std::string message = error.what();

        if (message.find("PE image exceeds the supported 2 GiB address range")
            == std::string::npos) {
            throw std::runtime_error(
                "Unexpected RelinkerException: " + message
            );
        }

        return;
    }

    throw std::runtime_error(
        "Builder accepted an entry-stub allocation crossing the 2 GiB limit"
    );
}

void checkNormalAllocation() {
    const auto stub = WindowsEntryStubBuilder{}.Build(
        0x10000u,
        0x1000u,
        makeTestImports(),
        {},
        {},
        ".",
        false,
        false
    );

    if (stub.Data.Data.empty())
        throw std::runtime_error("Normal entry-stub allocation was empty");
}

}

int main() {
    try {
        checkNormalAllocation();


        expectBoundaryRejection(
            static_cast<std::uint32_t>(INT32_MAX) - 64u
        );


        expectBoundaryRejection(
            static_cast<std::uint32_t>(INT32_MAX) - 16u
        );

        std::cout << "Windows entry-stub RVA boundary tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Windows entry-stub RVA boundary test failed: "
                  << error.what() << '\n';
        return 1;
    }
}