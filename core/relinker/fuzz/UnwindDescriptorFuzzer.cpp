#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 8) {
        return 0;
    }

    const std::vector<std::uint8_t> payload(data + 1, data + size);
    std::vector<Domain::ProgramHeader> headers;

    bool extracted = false;
    if (payload.size() >= 64) {
        try {
            const Relinker::ElfReader reader(payload);
            headers = reader.ReadProgramHeaders();
            extracted = !headers.empty();
        } catch (const std::exception&) {
        }
    }

    if (!extracted) {
        const std::size_t payloadSize = payload.size();
        Domain::ProgramHeader load{};
        load.Type = 1;
        load.MappedAddress = 0x1000;
        load.FileSize = payloadSize;
        load.MemorySize = payloadSize;

        Domain::ProgramHeader eh{};
        eh.Type = 0x6474E550;
        eh.MappedAddress = 0x1000;
        eh.FileSize = payloadSize > 64 ? 64 : payloadSize;
        eh.MemorySize = eh.FileSize;

        headers = {load, eh};
    }

    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(payload, headers, {}, {});
    } catch (const Domain::RelinkerException&) {
    } catch (const std::exception&) {
    }

    return 0;
}
