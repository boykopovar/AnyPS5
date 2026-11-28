#include "prx/libSceSystemService/HostUrl.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"

#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <string_view>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint64_t freeSize = 0;
    std::uint64_t totalSize = 0;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, &totalSize) == 0);
    errno = 71;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(reinterpret_cast<std::uint64_t*>(0x10), &totalSize) == static_cast<int>(0x80e30005u));
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, reinterpret_cast<std::uint64_t*>(0x10)) == static_cast<int>(0x80e30005u));
    Require(errno == 71);
    Require(freeSize != 0 && totalSize != 0 && freeSize <= totalSize);
    Require(sceShellCoreUtilGetFreeSizeOfAvContentsTmp() != 0);

    const std::string_view invalidIpv6Urls[] = {
        "http://[abc]/",
        "http://[:::]/",
        "http://[1::2::3]/",
        "http://[12345::1]/",
        "http://[1:2:3:4:5:6:7:8:9]/",
        "http://[1:2:3]/",
        "http://[:1]/",
        "http://[1:]/",
        "http://[::ffff:999.0.0.1]/",
        "http://[::ffff:01.0.0.1]/",
        "http://[1:2:3:4:5:6:7:1.2.3.4]/",
        "http://[1:2:3:4::5:6:7:8]/",
        "http://[1.2.3.4::1]/",
        "http://[::ffff:192.0.2]/",
        "http://[::ffff:1.2.3.4.5]/",
    };
    for (const std::string_view url : invalidIpv6Urls)
        Require(ValidateHostUrl(url) == HostUrlVerdict::BadAuthority);

    const std::string_view validIpv6Urls[] = {
        "http://[::1]/",
        "http://[::1]:8080/path",
        "https://[2001:db8::1]/",
        "http://[::ffff:192.0.2.1]/",
        "http://[1:2:3:4:5:6:192.0.2.1]/",
        "http://[1:2:3::5:6:7:8]/",
        "http://[::192.0.2.1]/",
        "http://[0:0:0:0:0:ffff:0.0.0.0]/",
        "http://[2001:0db8:85a3:0000:0000:8a2e:0370:7334]/",
    };
    for (const std::string_view url : validIpv6Urls)
        Require(ValidateHostUrl(url) == HostUrlVerdict::Ok);

    return 0;
}
