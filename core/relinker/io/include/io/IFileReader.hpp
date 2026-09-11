#ifndef IO_IFILEREADER_HPP
#define IO_IFILEREADER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Io {

class IFileReader {
public:
    virtual ~IFileReader() = default;

    virtual std::vector<std::uint8_t> Read(const std::string& path) = 0;
};

}

#endif
