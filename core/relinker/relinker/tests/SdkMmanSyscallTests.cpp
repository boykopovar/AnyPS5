#include "TestProgress.hpp"
#include <relinker/analysis/SyscallScanner.hpp>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;
struct Fixture { Bytes code; std::size_t syscall; };

const std::vector<Fixture> Fixtures = {
    {{0x55, 0x48, 0x89, 0xE5, 0x53, 0x50, 0x89, 0xD1, 0x49, 0x89, 0xF0, 0x48, 0x89, 0xFE,
      0xF6, 0xC1, 0x04, 0x75, 0x31, 0x48, 0x63, 0xD1, 0xB8, 0x4A, 0x00, 0x00, 0x00,
      0x48, 0x89, 0xF7, 0x4C, 0x89, 0xC6, 0x45, 0x31, 0xD2, 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9,
      0x0F, 0x05, 0x0F, 0x93, 0xC1, 0x48, 0x89, 0xC3, 0x48, 0xF7, 0xDB, 0x84, 0xC9,
      0x48, 0x0F, 0x45, 0xD8, 0x48, 0x85, 0xDB}, 42},
    {{0x49, 0x63, 0xD6, 0x4C, 0x63, 0xD1, 0x4D, 0x63, 0xC0, 0xB8, 0xDD, 0x01, 0x00, 0x00,
      0x48, 0x89, 0xDE, 0x0F, 0x05, 0x48, 0x89, 0xC1, 0x0F, 0x93, 0xC2, 0x48, 0xF7, 0xD8,
      0x84, 0xD2, 0x48, 0x0F, 0x45, 0xC1, 0x48, 0x85, 0xC0}, 17},
    {{0x44, 0x89, 0xF0, 0x83, 0xE0, 0xFB, 0x48, 0x63, 0xD0, 0x4C, 0x63, 0xD1, 0x4D, 0x63, 0xC0,
      0xB8, 0xDD, 0x01, 0x00, 0x00, 0x48, 0x89, 0xDE, 0x0F, 0x05, 0x0F, 0x93, 0xC1,
      0x49, 0x89, 0xC7, 0x49, 0xF7, 0xDF, 0x84, 0xC9, 0x4C, 0x0F, 0x45, 0xF8, 0x4D, 0x85, 0xFF}, 23},
};

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

bool Rejected(const Bytes& code, bool trap, std::size_t size) {
    try {
        Relinker::MakeSyscallScanner(testProgress, trap)->ScanCodeSectionForSyscalls(code, 0, size);
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void CheckFixtures() {
    std::size_t rejectedMutations = 0;
    for (const auto& fixture : Fixtures) {
        Require(Rejected(fixture.code, false, fixture.code.size()), "default mode accepted an SDK inline syscall");
        for (const auto padding : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{64}}) {
            Bytes code(padding, 0x90);
            code.insert(code.end(), fixture.code.begin(), fixture.code.end());
            const auto trapped = Relinker::MakeSyscallScanner(testProgress, true)->ScanCodeSectionForSyscalls(code, 0x123400, code.size());
            Require(trapped == std::vector<std::size_t>{padding + fixture.syscall}, "SDK inline syscall returned the wrong patch offset");
        }
        for (std::size_t length = fixture.syscall + 2; length < fixture.code.size(); ++length)
            Require(Rejected(fixture.code, true, length), "SDK matching read its suffix beyond the code section");
        for (std::size_t removed = 1; removed <= fixture.syscall; ++removed) {
            const Bytes fragment(fixture.code.begin() + static_cast<std::ptrdiff_t>(removed), fixture.code.end());
            if (Rejected(fragment, false, fragment.size()))
                Require(Rejected(fragment, true, fragment.size()), "SDK matching accepted a truncated prefix");
        }
        for (std::size_t offset = 0; offset < fixture.code.size(); ++offset) {
            if (offset == fixture.syscall || offset == fixture.syscall + 1) continue;
            for (unsigned bit = 0; bit < 8; ++bit) {
                auto code = fixture.code;
                code[offset] ^= static_cast<std::uint8_t>(1u << bit);
                if (!Rejected(code, false, code.size())) continue;
                Require(Rejected(code, true, code.size()), "SDK matching accepted a changed instruction sequence");
                ++rejectedMutations;
            }
        }
    }
    Require(rejectedMutations > 1000, "too few near-match rejection cases ran");
    Bytes embedded = {0x48, 0xB8};
    embedded.insert(embedded.end(), Fixtures.front().code.begin(), Fixtures.front().code.end());
    Require(Rejected(embedded, false, embedded.size()), "embedded fixture lost its syscall instruction");
    Require(Rejected(embedded, true, embedded.size()), "SDK matching accepted a prefix that starts inside an immediate");
    for (const auto& code : std::vector<Bytes>{{0xB8, 0xDD, 0x01, 0x00, 0x00, 0x0F, 0x05, 0xC3}, {0xCD, 0x80}, {0x0F, 0x34}, {0x0F, 0x07}})
        Require(Rejected(code, true, code.size()), "SDK trap mode accepted an unrelated privileged instruction");
    bool overflowRejected = false;
    try {
        Relinker::MakeSyscallScanner(testProgress, true)->ScanCodeSectionForSyscalls(Fixtures.front().code, std::numeric_limits<Domain::FileByteOffset>::max(), 1);
    } catch (const Domain::RelinkerException& error) {
        overflowRejected = std::string(error.what()) == "Code section offset overflow";
    }
    Require(overflowRejected, "Scanner accepted code section offset overflow");
    bool sizeExceededRejected = false;
    try {
        Relinker::MakeSyscallScanner(testProgress, true)->ScanCodeSectionForSyscalls(Fixtures.front().code, 0, Fixtures.front().code.size() + 1);
    } catch (const Domain::RelinkerException& error) {
        sizeExceededRejected = std::string(error.what()) == "Code section exceeds the input buffer";
    }
    Require(sizeExceededRejected, "Scanner accepted code section size exceeding input");
    std::cout << "SDK inline syscall checks passed, " << rejectedMutations << " near-match rejections\n";
}

}

int main() {
    try {
        CheckFixtures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
