#include "prx/libSceAgcDriver/Execution/include/StackWalk.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::CollapsedStacks;
using AgcDriver::DwarfFrames;
using Byte = DwarfFrames::Byte;
using Word = DwarfFrames::Word;
using Step = DwarfFrames::Step;

constexpr std::size_t FunctionOffset = 0x100;
constexpr std::size_t FunctionSize = 0x40;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Section {
public:
    explicit Section(Byte returnColumn) : bytes(0x200, 0) {
        const std::size_t cie = append32(0);
        append32(0);
        append({1, 'z', 'R', 0, 1, 0x78, returnColumn, 1, 0x1b, 0x0c, 7, 8, static_cast<Byte>(0x80 | returnColumn), 1});
        patch32(cie, static_cast<std::uint32_t>(cursor - cie - 4));
        const std::size_t fde = append32(0);
        append32(static_cast<std::uint32_t>(cursor - cie));
        append32(static_cast<std::uint32_t>(Function() - Address(cursor)));
        append32(FunctionSize);
        append({0, 0x41, 0x0e, 0x10, 0x86, 0x02, 0x43, 0x0d, 0x06, 0, 0});
        patch32(fde, static_cast<std::uint32_t>(cursor - fde - 4));
        end = cursor;
        append32(0);
    }

    Word Function() const {
        return Address(FunctionOffset);
    }

    DwarfFrames Frames() const {
        return DwarfFrames(Address(0), Address(bytes.size()), bytes.data(), bytes.data() + end + 4);
    }

private:
    Word Address(std::size_t offset) const {
        return reinterpret_cast<Word>(bytes.data() + offset);
    }

    std::size_t append32(std::uint32_t value) {
        const auto at = cursor;
        std::memcpy(bytes.data() + cursor, &value, 4);
        cursor += 4;
        return at;
    }

    void append(std::initializer_list<Byte> values) {
        for (const auto value : values) bytes[cursor++] = value;
    }

    void patch32(std::size_t at, std::uint32_t value) {
        std::memcpy(bytes.data() + at, &value, 4);
    }

    std::vector<Byte> bytes;
    std::size_t cursor = 0;
    std::size_t end = 0;
};

struct Stack {
    std::array<Word, 64> words{};

    Word At(std::size_t index) const {
        return reinterpret_cast<Word>(&words[index]);
    }

    auto Reader() const {
        return [this](Word address, Word& value) {
            if (address < At(0) || address > At(words.size() - 1)) return false;
            std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
            return true;
        };
    }
};

DwarfFrames::Registers Registers(Word pc, Word sp, Word fp) {
    DwarfFrames::Registers registers{};
    registers[16] = pc;
    registers[7] = sp;
    registers[6] = fp;
    return registers;
}

void Prologue() {
    const Section section(16);
    const auto frames = section.Frames();
    const auto function = section.Function();
    Stack stack;
    stack.words[32] = 0x5555;
    stack.words[33] = 0x1234;

    auto registers = Registers(function, stack.At(33), 0x9999);
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::Stepped, "the entry of a function did not step");
    Check(registers[16] == 0x1234 && registers[7] == stack.At(34) && registers[6] == 0x9999, "the entry of a function stepped to the wrong frame");

    registers = Registers(function + 2, stack.At(32), 0x9999);
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::Stepped, "the frame after the push did not step");
    Check(registers[16] == 0x1234 && registers[7] == stack.At(34) && registers[6] == 0x5555, "the frame after the push did not restore the frame pointer");

    registers = Registers(function + 0x20, stack.At(8), stack.At(32));
    Check(frames.Unwind(registers, false, stack.Reader()) == Step::Stepped, "the body of a function did not step");
    Check(registers[16] == 0x1234 && registers[7] == stack.At(34) && registers[6] == 0x5555, "the body of a function did not step through the frame pointer");
}

void Outside() {
    const Section section(16);
    const auto frames = section.Frames();
    const auto function = section.Function();
    Stack stack;
    auto registers = Registers(function + FunctionSize, stack.At(8), 0);
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::NoRecord, "a pc after the function had a record");
    registers = Registers(function - 1, stack.At(8), 0);
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::NoRecord, "a pc before the function had a record");
    Check(frames.Covers(function + 0x10) && !frames.Covers(function + FunctionSize), "the record covers the wrong range");
    registers = Registers(function, stack.At(63) + 4, 0);
    const auto before = registers;
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::Failed && registers == before, "a read outside the stack was not refused");
    Check(DwarfFrames(0, 0, nullptr, nullptr).Unwind(registers, true, stack.Reader()) == Step::NoRecord, "a module without records stepped");
}

void ReturnColumn32() {
    const Section section(32);
    const auto frames = section.Frames();
    Stack stack;
    stack.words[10] = 0x4321;
    auto registers = Registers(section.Function(), stack.At(10), 0);
#ifdef _WIN32
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::Stepped && registers[16] == 0x4321 && registers[7] == stack.At(11), "the MinGW return column was not read");
#else
    Check(frames.Unwind(registers, true, stack.Reader()) == Step::NoRecord, "a return column above 16 was accepted");
#endif
}

void Collapsed() {
    CollapsedStacks stacks;
    const std::uint64_t full[] = {0x10, 0x20, 0x30};
    const std::uint64_t cut[] = {0x10, 0x40};
    stacks.Add(full, false);
    stacks.Add(full, false);
    stacks.Add(cut, true);
    const auto text = stacks.Format([](std::uint64_t address) {
        char name[32];
        std::snprintf(name, sizeof(name), "m+0x%llx", static_cast<unsigned long long>(address));
        return std::string(name);
    });
    Check(text == "m+0x30;m+0x20;m+0x10 2\n[truncated];m+0x40;m+0x10 1\n", "collapsed stacks changed format");
    Check(stacks.Truncated() == 1, "the truncated walk was not counted");
}

}

int main() {
    try {
        Prologue();
        Outside();
        ReturnColumn32();
        Collapsed();
        std::cout << "stack walk tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
