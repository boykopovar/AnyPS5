#include "OracleDispatch.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr const char* Usage = "usage: agc_oracle_runner code.co threads in.bin out.bin out_dwords\n";

std::vector<std::byte> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error(std::string("cannot open ") + path);
    const auto size = file.tellg();
    if (size < 0) throw std::runtime_error(std::string("cannot size ") + path);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error(std::string("cannot read ") + path);
    return bytes;
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << Usage;
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--target") {
        std::cerr << "oracle runner: --target is not supported\n";
        return 2;
    }
    if (argc != 6) {
        std::cerr << Usage;
        return 2;
    }
    try {
        const auto threads = ParseOracleCount(argv[2], "threads");
        const auto outDwords = ParseOracleCount(argv[5], "out_dwords");
        if (outDwords != 16) throw std::runtime_error("out_dwords must be 16, the template stores 16 dwords per lane");
        const auto kernel = ParseOracleCodeObject(ReadFile(argv[1]));
        const auto input = ReadFile(argv[3]);
        AgcDriver::VulkanDevice device;
        const auto output = RunOracleKernel(device, kernel, threads, input);
        std::ofstream file(argv[4], std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()));
        file.close();
        if (!file) throw std::runtime_error(std::string("cannot write ") + argv[4]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "oracle runner: " << error.what() << '\n';
        return 1;
    }
}
