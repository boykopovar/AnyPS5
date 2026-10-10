#include <io/FileWriter.hpp>
#include <domain/Types.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template<typename TValue>
void requireWriteFailure(const TValue& data) {
    Io::FileWriter writer;
    writer.Write("/dev/null", data);
    try {
        writer.Write("/dev/full", data);
    } catch (const Domain::RelinkerException& e) {
        if (std::string(e.what()) != "Failed to write file: /dev/full")
            throw std::runtime_error("Unexpected write failure: " + std::string(e.what()));
        return;
    }
    throw std::runtime_error("Write to /dev/full reported success");
}

}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::runtime_error("Expected binary or text");
        const std::string mode = argv[1];
        for (const std::size_t size : {std::size_t{3}, std::size_t{65536}}) {
            if (mode == "binary")
                requireWriteFailure(std::vector<std::uint8_t>(size, 0x41));
            else if (mode == "text")
                requireWriteFailure(std::string(size, 'A'));
            else
                throw std::runtime_error("Unknown test mode: " + mode);
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
