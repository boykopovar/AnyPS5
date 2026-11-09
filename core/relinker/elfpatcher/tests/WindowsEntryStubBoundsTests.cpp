#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace WindowsEntryStubAllocation {
extern std::size_t largest;
}

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void checkNormalAllocation(const Elfpatcher::Windows::WindowsImports& imports, const std::uint32_t dataRva) {
    WindowsEntryStubAllocation::largest = 0;
    const auto stub = Elfpatcher::Windows::WindowsEntryStubBuilder().Build(
        dataRva, 0x1000, imports, {}, {}, ".", false, false);
    require(!stub.Data.Data.empty(), "Normal entry-stub allocation was empty");
    require(WindowsEntryStubAllocation::largest > 4096, "The allocation tracker missed the startup buffers");
}

void checkBoundaryBeforeAllocation(const Elfpatcher::Windows::WindowsImports& imports) {
    constexpr auto limit = std::numeric_limits<std::int32_t>::max();
    constexpr std::uint32_t remaining = 4096;
    WindowsEntryStubAllocation::largest = 0;
    try {
        Elfpatcher::Windows::WindowsEntryStubBuilder().Build(
            limit - remaining, 0x1000, imports, {}, {}, ".", false, false);
    } catch (const Domain::RelinkerException& error) {
        const auto largest = WindowsEntryStubAllocation::largest;
        require(std::string(error.what()) == "PE image exceeds the supported 2 GiB address range",
            "The entry-stub boundary failed with another error");
        require(error.FailureOffset > limit, "The rejected endpoint was within the supported range");
        require(largest <= remaining, "The entry stub allocated past the RVA limit before rejecting it");
        return;
    }
    throw std::runtime_error("An entry-stub allocation crossing the 2 GiB limit was accepted");
}

}

int main() {
    try {
        const auto imports = Elfpatcher::Windows::WindowsImportBuilder().Build(0x1000);
        checkNormalAllocation(imports, 0x10000);
        checkNormalAllocation(imports, std::numeric_limits<std::int32_t>::max() - 1024 * 1024);
        checkBoundaryBeforeAllocation(imports);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
