#include <lifter/Compile.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/Object/ArchiveWriter.h>
#include <llvm/Support/Error.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace Ps5Lift {

namespace {

constexpr std::size_t InstructionsPerPart = 50000;

std::size_t Weight(const GuestFunction& function) {
    return std::max<std::size_t>(1, function.Code.size());
}

struct Part {
    std::size_t Image;
    std::vector<std::uint64_t> Functions;
};

void Partition(const std::size_t image, const std::map<std::uint64_t, GuestFunction>& functions, std::vector<Part>& parts) {
    std::size_t total = 0;
    for (const auto& [address, function] : functions) total += Weight(function);
    const auto count = std::max<std::size_t>(1, (total + InstructionsPerPart - 1) / InstructionsPerPart);
    const auto target = (total + count - 1) / count;
    const auto first = parts.size();
    parts.push_back({image, {}});
    std::size_t weight = 0;
    for (const auto& [address, function] : functions) {
        if (weight >= target && parts.size() - first < count) {
            parts.push_back({image, {}});
            weight = 0;
        }
        parts.back().Functions.push_back(address);
        weight += Weight(function);
    }
}

void WriteArchive(const std::string& path, const std::vector<llvm::SmallVector<char, 0>>& objects) {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < objects.size(); ++i) names.push_back("part" + std::to_string(i) + ".o");
    std::vector<llvm::NewArchiveMember> members;
    for (std::size_t i = 0; i < objects.size(); ++i) members.emplace_back(llvm::MemoryBufferRef(llvm::StringRef(objects[i].data(), objects[i].size()), names[i]));
    if (auto error = llvm::writeArchive(path, members, llvm::SymtabWritingMode::NormalSymtab, llvm::object::Archive::K_GNU, true, false))
        throw std::runtime_error("Cannot write " + path + ": " + llvm::toString(std::move(error)));
}

}

LiftReport CompileProgram(const GuestProgram& program, const std::vector<std::map<std::uint64_t, GuestFunction>>& functions, const LiftOptions& lift, const Target& target,
                          const CompileOptions& options) {
    std::vector<Part> parts;
    for (std::size_t image = 0; image < functions.size(); ++image) Partition(image, functions[image], parts);
    const auto tasks = parts.size() + 1;
    std::vector<llvm::SmallVector<char, 0>> objects(tasks);
    std::vector<LiftReport> reports(tasks);
    std::atomic<std::size_t> next = 0;
    std::atomic<bool> failed = false;
    std::exception_ptr failure;
    std::mutex failureLock;
    const auto work = [&] {
        for (auto task = next++; task < tasks && !failed; task = next++) {
            try {
                llvm::LLVMContext context;
                auto module = task < parts.size()
                                  ? LiftFunctions(context, program.Images[parts[task].Image], functions[parts[task].Image], parts[task].Functions, lift, target, reports[task])
                                  : LiftTables(context, program, functions, lift, target);
                Optimize(*module, target);
                if (!options.IrPrefix.empty()) WriteIr(*module, options.IrPrefix + "." + std::to_string(task) + ".ll");
                if (!options.Archive.empty()) objects[task] = CompileObject(*module, target);
            } catch (...) {
                const std::lock_guard lock(failureLock);
                if (!failure) failure = std::current_exception();
                failed = true;
            }
        }
    };
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < std::min<std::size_t>(options.Jobs, tasks); ++i) threads.emplace_back(work);
    for (auto& thread : threads) thread.join();
    if (failure) std::rethrow_exception(failure);
    if (!options.Archive.empty()) WriteArchive(options.Archive, objects);

    LiftReport report;
    for (const auto& part : reports) {
        report.Functions += part.Functions;
        report.Instructions += part.Instructions;
        for (const auto& [mnemonic, count] : part.Unsupported) report.Unsupported[mnemonic] += count;
    }
    return report;
}

}
