#ifndef IO_IFILEWRITER_HPP
#define IO_IFILEWRITER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Io {

class IFileWriter {
public:
    virtual ~IFileWriter() = default;

    virtual void Write(const std::string& path, const std::vector<std::uint8_t>& data) = 0;
    virtual void Write(const std::string& path, const std::string& content) = 0;
};

}

#endif
