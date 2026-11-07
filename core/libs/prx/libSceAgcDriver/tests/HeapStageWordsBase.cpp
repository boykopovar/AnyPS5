#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "Recompiler.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using ShaderRecompiler::DescriptorRole;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

ShaderRecompiler::DescriptorBinding Binding(DescriptorRole role, std::vector<std::uint32_t> words) {
    ShaderRecompiler::DescriptorBinding binding{};
    binding.role = role;
    binding.count = 1;
    binding.guestDescriptor = std::move(words);
    return binding;
}

void TestTemplateWords() {
    ShaderRecompiler::RecompileResult program;
    program.bindings = {Binding(DescriptorRole::ShaderData, {1, 2, 3, 4}), Binding(DescriptorRole::GuestImages, {10, 11, 12, 13, 14, 15, 16, 17}),
                        Binding(DescriptorRole::FlattenedSrt, {20, 21}), Binding(DescriptorRole::GuestSamplers, {30, 31, 32, 33})};
    auto shared = std::make_shared<ShaderRecompiler::HeapStageWords>();
    shared->bindings = {{9, 9, 9, 9}, {40, 41, 42, 43, 44, 45, 46, 47}, {50, 51}, {}};
    ShaderRecompiler::HeapStageWords words;
    words.base = shared;
    words.userData = {7};
    words.bindings.resize(program.bindings.size());
    const auto& b = program.bindings;
    Check(AgcDriver::Graphics::DrawGuestWords(program, &words, b[1]) == shared->bindings[1], "a resource binding without per-draw words does not read its template's");
    Check(AgcDriver::Graphics::DrawGuestWords(program, &words, b[3]) == b[3].guestDescriptor, "a binding the template does not hold does not read the program's words");
    Check(AgcDriver::Graphics::DrawGuestWords(program, &words, b[0]) == b[0].guestDescriptor, "shader data fell back to the template");
    Check(AgcDriver::Graphics::DrawGuestWords(program, &words, b[2]) == b[2].guestDescriptor, "the flattened SRT fell back to the template");
    words.bindings[1] = {60, 61, 62, 63, 64, 65, 66, 67};
    Check(AgcDriver::Graphics::DrawGuestWords(program, &words, b[1]) == words.bindings[1], "per-draw words do not override the template's");
    words.bindings[1].clear();
    const auto patched = AgcDriver::DriverDetail::PatchedHeapResult(program, words);
    Check(patched->bindings[0].guestDescriptor == std::vector<std::uint32_t>({7, 2, 3, 4}), "the patched shader data does not take the draw's user data over the program's");
    Check(patched->bindings[1].guestDescriptor == shared->bindings[1], "the patched result does not take the template's resource words");
    Check(patched->bindings[2].guestDescriptor == b[2].guestDescriptor, "the patched flattened SRT took the template's words");
    Check(patched->bindings[3].guestDescriptor == b[3].guestDescriptor, "the patched result lost the program's words");
    shared->bindings[3] = {1};
    bool refused = false;
    try {
        static_cast<void>(AgcDriver::DriverDetail::PatchedHeapResult(program, words));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Check(refused, "template words of another size than the program's were taken");
}

}

int main() {
    try {
        TestTemplateWords();
        std::cout << "heap stage template word tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "heap stage template word tests failed: " << error.what() << '\n';
        return 1;
    }
}
