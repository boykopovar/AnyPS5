#include <relinker/parsing/RelrRelocations.hpp>
#include <io/BufferUtils.hpp>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void Require(const bool condition) {
    if (!condition) throw std::runtime_error("RELR bounds validation failed");
}

struct Fixture {
    std::vector<std::uint8_t> Bytes = std::vector<std::uint8_t>(0x2000);
    std::vector<Domain::ProgramHeader> Headers{{1, 7, 0, 0, 0, 0x2000, 0x3000, 0x1000}};
    std::vector<Domain::DynamicTag> Tags{{36, 0}, {35, 8}, {37, 8}};
};

void Reject(const std::function<void(Fixture&)>& change, const std::string_view diagnostic) {
    Fixture fixture;
    change(fixture);
    try {
        Relinker::ReadRelrRelocations(fixture.Bytes, fixture.Headers, fixture.Tags);
    } catch (const Domain::RelinkerException& error) {
        Require(std::string_view(error.what()).find(diagnostic) != std::string_view::npos);
        return;
    }
    throw std::runtime_error("Malformed RELR input accepted");
}

}

int main() try {
    Reject([](auto& input) { input.Tags[0].Value = 0; input.Tags[1].Value = 8; input.Tags[2].Value = 8; }, "Incomplete DT_RELR metadata");
    return 0;
} catch (const std::exception& error) {
    return 1;
}
