#include <codegen/x86/X64InstructionRewriter.hpp>
#include <iostream>
#include <stdexcept>

static void Require(bool ok) {
    if (!ok)
        throw std::runtime_error("Invalid rewrite boundary accepted");
}

static void RequireTruncatedBranchRejected(bool ok) {
    if (!ok)
        throw std::runtime_error(
            "Length-changing rewrite accepted a section with a truncated relative branch");
}

int main() {
    try {
        const Codegen::X64InstructionRewriter rewriter;

        for (const auto& code : {
                 std::vector<std::uint8_t>{
                     0xB8, 0x90, 0x90, 0x90, 0x90, 0xC3},
                 std::vector<std::uint8_t>{
                     0x48, 0x8B, 0x05, 0x90, 0x90, 0x90, 0x90, 0xC3},
                 std::vector<std::uint8_t>{
                     0xE8, 0x90, 0x90, 0x90, 0x90, 0xC3}
             }) {
            const auto original = code;

            for (std::size_t offset = 1;
                 offset + 1 < code.size();
                 ++offset) {
                bool rejected = false;

                try {
                    (void)rewriter.Rewrite(code, {offset, {0x90}});
                } catch (const std::exception&) {
                    rejected = true;
                }

                Require(rejected && code == original);
            }

            const auto result = rewriter.Rewrite(code, {0, {0x90}});

            Require(result.Bytes == std::vector<std::uint8_t>{
                0x90, 0xC3});
        }

        const auto result = rewriter.Rewrite(
            {0x90, 0x90, 0xC3},
            {1, {0x90, 0x90}});

        Require(result.Bytes == std::vector<std::uint8_t>{
            0x90, 0x90, 0x90, 0xC3});

        bool targetInsideCutRejected = false;
        try {
            (void)rewriter.Rewrite(
                {0xEB, 0x01, 0xB8, 0x11, 0x22, 0x33, 0x44, 0xC3},
                {2, {0x90}});
        } catch (const std::exception&) {
            targetInsideCutRejected = true;
        }
        Require(targetInsideCutRejected);

        for (const auto& replacement : {
                 std::vector<std::uint8_t>{0x90},
                 std::vector<std::uint8_t>{0xB8, 0x55, 0x66, 0x77, 0x88}
             }) {
            bool truncatedTargetInsideCutRejected = false;
            try {
                (void)rewriter.Rewrite(
                    {0x66, 0xEB, 0x01, 0xB8, 0x11, 0x22, 0x33, 0x44, 0xC3},
                    {3, replacement});
            } catch (const std::exception&) {
                truncatedTargetInsideCutRejected = true;
            }
            Require(truncatedTargetInsideCutRejected);
        }

        for (const auto& code : {
                 std::vector<std::uint8_t>{
                     0x66, 0xEB, 0x01, 0x90, 0xC3},
                 std::vector<std::uint8_t>{
                     0x90, 0x66, 0xEB, 0xFC, 0xC3}
             }) {
            bool rejected = false;

            try {
                const auto offset =
                    code[0] == 0x66 ? std::size_t{3} : std::size_t{0};

                (void)rewriter.Rewrite(code, {offset, {0x66, 0x90}});
            } catch (const std::exception&) {
                rejected = true;
            }

            RequireTruncatedBranchRejected(rejected);
        }

        const auto unaffected = rewriter.Rewrite(
            {0x66, 0xEB, 0x00, 0x90, 0x90, 0xC3},
            {4, {0x66, 0x90}});

        Require(unaffected.Bytes == std::vector<std::uint8_t>{
            0x66, 0xEB, 0x00, 0x90, 0x66, 0x90, 0xC3});

        std::vector<std::uint8_t> forwardWraparound(0x10004, 0x90);
        forwardWraparound[0xFFFA] = 0x66;
        forwardWraparound[0xFFFB] = 0xEB;
        forwardWraparound[0xFFFC] = 0x03;

        bool forwardWraparoundRejected = false;
        try {
            (void)rewriter.Rewrite(forwardWraparound, {10, {0x66, 0x90}});
        } catch (const std::exception&) {
            forwardWraparoundRejected = true;
        }
        RequireTruncatedBranchRejected(forwardWraparoundRejected);

        std::vector<std::uint8_t> backwardWraparound(0x10004, 0x90);
        backwardWraparound[0] = 0x66;
        backwardWraparound[1] = 0xEB;
        backwardWraparound[2] = 0xFC;

        bool backwardWraparoundRejected = false;
        try {
            (void)rewriter.Rewrite(backwardWraparound, {0xFFFE, {0x66, 0x90}});
        } catch (const std::exception&) {
            backwardWraparoundRejected = true;
        }
        RequireTruncatedBranchRejected(backwardWraparoundRejected);

        std::cout << "24 rewrite boundary cases passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
