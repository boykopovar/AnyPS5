#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
struct Iovec {
    const void* base;
    std::uint64_t length;
};
struct Hdtr {
    const Iovec* headers;
    int headerCount;
    const Iovec* trailers;
    int trailerCount;
};
extern "C" {
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI ioctl_nid_postfix(int, std::uint64_t, void*);
int APS5_VABI sendfile_nid_postfix(int, int, std::int64_t, std::uint64_t, const Hdtr*, std::int64_t*, int);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
static bool Fails(int result, int error) { return result == -1 && *__error_nid_postfix() == error; }
static std::string Receive(int descriptor, std::size_t length) {
    std::string data(length, 0);
    std::size_t done = 0;
    while (done < length) {
        const auto result = recv_nid_postfix(descriptor, data.data() + done, length - done, 0);
        Require(result > 0);
        done += static_cast<std::size_t>(result);
    }
    return data;
}
static std::string Content(std::size_t size) {
    std::string content(size, 0);
    for (std::size_t i = 0; i < size; ++i) content[i] = static_cast<char>(i * 7 % 251);
    return content;
}
static int OpenWritten(const char* path, const std::string& content) {
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    const int file = open_nid_postfix(path, 0, 0);
    Require(file >= 0);
    return file;
}
int main() {
    const std::string content = Content(200000);
    const auto size = static_cast<std::int64_t>(content.size());
    const int file = OpenWritten("sendfile_probe.bin", content);
    int pair[2];
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    const Iovec header[] = {{"GET", 3}, {" ", 1}};
    const Iovec trailer[] = {{"END", 3}};
    const Hdtr parts{header, 2, trailer, 1};
    std::int64_t sent = -1;
    Require(sendfile_nid_postfix(file, pair[0], 5, 10, &parts, &sent, 0) == 0 && sent == 17);
    Require(Receive(pair[1], 17) == "GET " + content.substr(5, 10) + "END");
    Require(lseek_nid_postfix(file, 0, 1) == 0);
    std::string rest;
    std::thread reader([&] { rest = Receive(pair[1], content.size() - 100); });
    Require(sendfile_nid_postfix(file, pair[0], 100, 0, nullptr, &sent, 0) == 0 && sent == size - 100);
    reader.join();
    Require(rest == content.substr(100));
    Require(sendfile_nid_postfix(file, pair[0], size - 4, 1000, nullptr, &sent, 0) == 0 && sent == 4);
    Require(Receive(pair[1], 4) == content.substr(content.size() - 4));
    Require(sendfile_nid_postfix(file, pair[0], size + 10, 0, &parts, &sent, 0) == 0 && sent == 7);
    Require(Receive(pair[1], 7) == "GET END");
    const Hdtr nullHeaders{nullptr, 5, trailer, 1};
    Require(sendfile_nid_postfix(file, pair[0], 0, 3, &nullHeaders, nullptr, 0) == 0);
    Require(Receive(pair[1], 6) == content.substr(0, 3) + "END");
    Require(lseek_nid_postfix(file, 0, 1) == 0);

    sent = 77;
    Require(Fails(sendfile_nid_postfix(file, pair[0], -1, 0, nullptr, &sent, 0), 22) && sent == 77);
    const std::vector<Iovec> many(1025, Iovec{"x", 1});
    const Hdtr tooMany{many.data(), 1025, nullptr, 0};
    Require(Fails(sendfile_nid_postfix(file, pair[0], 0, 0, &tooMany, &sent, 0), 22) && sent == 77);
    const Hdtr negative{nullptr, 0, trailer, -1};
    Require(Fails(sendfile_nid_postfix(file, pair[0], 0, 0, &negative, &sent, 0), 22) && sent == 77);
    Require(Fails(sendfile_nid_postfix(pair[1], pair[0], 0, 0, nullptr, &sent, 0), 22) && sent == 0);
    sent = 77;
    Require(Fails(sendfile_nid_postfix(file, file, 0, 0, nullptr, &sent, 0), 38) && sent == 0);
    const int datagram = socket_nid_postfix(2, 2, 0);
    Require(datagram >= 0);
    sent = 77;
    Require(Fails(sendfile_nid_postfix(file, datagram, 0, 0, nullptr, &sent, 0), 22) && sent == 0);
    const int unconnected = socket_nid_postfix(2, 1, 0);
    Require(unconnected >= 0);
    sent = 77;
    Require(Fails(sendfile_nid_postfix(file, unconnected, size + 10, 0, nullptr, &sent, 0), 57) && sent == 0);

    const std::string large = Content(32 << 20);
    const int largeFile = OpenWritten("sendfile_large.bin", large);
    int on = 1;
    Require(ioctl_nid_postfix(pair[0], 0x8004667e, &on) == 0);
    sent = -1;
    Require(Fails(sendfile_nid_postfix(largeFile, pair[0], 0, 0, nullptr, &sent, 0), 35));
    Require(sent > 0 && sent < static_cast<std::int64_t>(large.size()));
    Require(Receive(pair[1], static_cast<std::size_t>(sent)) == large.substr(0, static_cast<std::size_t>(sent)));

    Require(close_nid_postfix(unconnected) == 0);
    sent = 77;
    Require(Fails(sendfile_nid_postfix(file, unconnected, 0, 0, nullptr, &sent, 0), 9) && sent == 0);
    Require(close_nid_postfix(largeFile) == 0 && close_nid_postfix(file) == 0);
    sent = 77;
    Require(Fails(sendfile_nid_postfix(file, pair[0], 0, 0, nullptr, &sent, 0), 9) && sent == 77);
    Require(close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    std::filesystem::remove("sendfile_probe.bin");
    std::filesystem::remove("sendfile_large.bin");
}
