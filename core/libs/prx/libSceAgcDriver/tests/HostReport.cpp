#include "prx/libSceAgcDriver/Execution/include/HostReport.hpp"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    ++failures;
}

AgcDriver::HostFacts SampleFacts() {
    AgcDriver::HostFacts facts;
    facts.commit = "0123456789ab";
    facts.os = "Linux 6.1.0-test";
    facts.cpuModel = "Test CPU 9000";
    facts.cpuThreads = 12;
    facts.memoryMiB = 32000;
    facts.cpuMissing = std::vector<std::string>{};
    facts.sdlLinked = "2.30.0";
    facts.sdlCompiled = "2.30.1";
    AgcDriver::HostGpuFacts gpu;
    gpu.name = "Test GPU";
    gpu.type = "discrete";
    gpu.driverName = "test-driver";
    gpu.driverInfo = "Test 1.2.3";
    gpu.vendorId = 0x10de;
    gpu.deviceId = 0x2504;
    gpu.apiVersion = (1u << 22) | (3u << 12) | 280u;
    gpu.driverVersion = 0x12345678;
    gpu.subgroupSize = 32;
    gpu.deviceLocalMiB = 12288;
    gpu.candidates = {"Test GPU", "Other GPU"};
    gpu.presentModes = {"mailbox", "fifo", "immediate"};
    gpu.requirements = {
        {"VK_KHR_swapchain", "extension", true, true},
        {"shaderInt64", "feature", true, true},
        {"VK_EXT_memory_budget", "extension", false, true},
        {"VK_KHR_maintenance5", "extension", false, false},
    };
    facts.gpu = gpu;
    facts.shaderCache = AgcDriver::HostShaderCacheFacts{true, true, 512};
    facts.environment = {{"ANYPS5_OVERLAY", "1"}, {"APS5_PROFILE_GPU", "1"}};
    return facts;
}

constexpr const char* Golden =
    "AnyPS5 host report (format 1)\n"
    "commit: 0123456789ab\n"
    "[system]\n"
    "os: Linux 6.1.0-test\n"
    "cpu: Test CPU 9000\n"
    "cpu_threads: 12\n"
    "memory_mib: 32000\n"
    "cpu_gate: passed\n"
    "[sdl]\n"
    "linked: 2.30.0\n"
    "compiled: 2.30.1\n"
    "[gpu]\n"
    "device: Test GPU\n"
    "type: discrete\n"
    "vendor_id: 0x10de\n"
    "device_id: 0x2504\n"
    "vulkan_api: 1.3.280\n"
    "driver: test-driver\n"
    "driver_info: Test 1.2.3\n"
    "driver_version: unknown\n"
    "driver_version_raw: 0x12345678\n"
    "subgroup_size: 32\n"
    "device_local_mib: 12288\n"
    "candidates: Test GPU, Other GPU\n"
    "present_modes: fifo, immediate, mailbox\n"
    "present_mode_requested: default\n"
    "[vulkan]\n"
    "VK_KHR_swapchain: extension required present\n"
    "shaderInt64: feature required present\n"
    "VK_EXT_memory_budget: extension optional present\n"
    "VK_KHR_maintenance5: extension optional missing\n"
    "[shader_cache]\n"
    "state: present\n"
    "size_mib: 512\n"
    "[environment]\n"
    "ANYPS5_OVERLAY: 1\n"
    "APS5_PROFILE_GPU: 1\n";

void GoldenOutput() {
    Check(AgcDriver::FormatHostReport(SampleFacts()) == Golden, "the report differs from the golden text");
}

void OrderIsStable() {
    auto shuffled = SampleFacts();
    std::reverse(shuffled.environment.begin(), shuffled.environment.end());
    std::reverse(shuffled.gpu->requirements.begin(), shuffled.gpu->requirements.end());
    std::reverse(shuffled.gpu->presentModes.begin(), shuffled.gpu->presentModes.end());
    Check(AgcDriver::FormatHostReport(shuffled) == Golden, "the entry order of the input changed the report");
    Check(AgcDriver::FormatHostReport(SampleFacts()) == AgcDriver::FormatHostReport(SampleFacts()), "two formats of the same facts differ");
}

void MissingFactsAreNamed() {
    AgcDriver::HostFacts facts;
    const auto text = AgcDriver::FormatHostReport(facts);
    Check(text.find("os: unavailable\n") != std::string::npos, "an absent OS is not reported as unavailable");
    Check(text.find("cpu_gate: unavailable\n") != std::string::npos, "an unknown CPU gate is not reported as unavailable");
    Check(text.find("device: none selected\n") != std::string::npos, "an absent GPU is not reported");
    Check(text.find("state: unavailable\n") != std::string::npos, "an unknown shader cache is not reported as unavailable");
    Check(text.find("commit: unknown\n") != std::string::npos, "an empty commit is not reported as unknown");
    facts.cpuMissing = std::vector<std::string>{"AVX2", "BMI2"};
    facts.shaderCache = AgcDriver::HostShaderCacheFacts{false, false, 0};
    const auto failed = AgcDriver::FormatHostReport(facts);
    Check(failed.find("cpu_gate: failed, lacks AVX2, BMI2\n") != std::string::npos, "a failed CPU gate does not list the missing extensions");
    Check(failed.find("state: disabled\n") != std::string::npos, "a disabled shader cache is not reported");
    facts.shaderCache = AgcDriver::HostShaderCacheFacts{true, false, 0};
    Check(AgcDriver::FormatHostReport(facts).find("state: absent\n") != std::string::npos, "an absent shader cache directory is not reported");
}

std::string DecodedDriverVersion(AgcDriver::HostDriverVersionScheme scheme, std::uint32_t version) {
    auto facts = SampleFacts();
    facts.gpu->driverVersionScheme = scheme;
    facts.gpu->driverVersion = version;
    const auto text = AgcDriver::FormatHostReport(facts);
    const auto start = text.find("driver_version: ");
    if (start == std::string::npos) return {};
    const auto value = start + std::char_traits<char>::length("driver_version: ");
    return text.substr(value, text.find('\n', value) - value);
}

void DriverVersionsAreDecoded() {
    using Scheme = AgcDriver::HostDriverVersionScheme;
    Check(DecodedDriverVersion(Scheme::Nvidia, 0x9b438000u) == "621.14.0.0", "NVIDIA 621.14 was not decoded");
    Check(DecodedDriverVersion(Scheme::Nvidia, (580u << 22) | (95u << 14) | (5u << 6) | 1u) == "580.95.5.1", "the lower NVIDIA fields were not decoded");
    Check(DecodedDriverVersion(Scheme::IntelWindows, 0x0019485cu) == "101.2140", "Intel 101.2140 on Windows was not decoded");
    Check(DecodedDriverVersion(Scheme::Vulkan, (25u << 22) | (2u << 12) | 8u) == "25.2.8", "a Vulkan-encoded version was not decoded");
    Check(DecodedDriverVersion(Scheme::Unknown, 0x9b438000u) == "unknown", "a version without a known encoding was decoded");
    const auto text = AgcDriver::FormatHostReport(SampleFacts());
    Check(text.find("driver_version: ") < text.find("driver_version_raw: "), "the decoded version does not precede the raw value");
}

void PathsAndAddressesAreRedacted() {
    Check(AgcDriver::RedactHostText("/home/alice/.cache/shader_cache") == "<path>", "an absolute path was kept");
    Check(AgcDriver::RedactHostText("loaded from ~/games/x") == "loaded from <path>", "a home-relative path was kept");
    Check(AgcDriver::RedactHostText("C:\\Users\\bob\\AppData") == "<path>", "a Windows path was kept");
    Check(AgcDriver::RedactHostText("D:/Users/bob") == "<path>", "a Windows drive path with a slash was kept");
    Check(AgcDriver::RedactHostText("(/usr/lib/libvulkan.so.1)") == "<path>", "a parenthesised path was kept");
    Check(AgcDriver::RedactHostText("https://user:secret@example.org/a") == "<url>", "a URL was kept");
    Check(AgcDriver::RedactHostText("nic 00:1a:2B:3c:4d:5e up") == "nic <redacted> up", "a MAC address was kept");
    Check(AgcDriver::RedactHostText("00-1A-2B-3C-4D-5E") == "<redacted>", "a dashed MAC address was kept");
    Check(AgcDriver::RedactHostText("LAHF/SAHF") == "LAHF/SAHF", "a feature name with a slash was changed");
    Check(AgcDriver::RedactHostText("AMD Radeon RX 6750 XT (RADV NAVI22)") == "AMD Radeon RX 6750 XT (RADV NAVI22)", "a device name was changed");
    Check(AgcDriver::RedactHostText("a\nfake: line\r\tx") == "a fake: line  x", "control characters were kept");
}

void MissingRequirementsAreDescribed() {
    auto facts = SampleFacts();
    Check(AgcDriver::DescribeMissingRequirements(*facts.gpu).empty(), "a GPU with every required item got a message");
    Check(AgcDriver::MissingRequirements(*facts.gpu).empty(), "a GPU with every required item lists missing items");
    facts.gpu->requirements.push_back({"VK_KHR_8bit_storage", "extension", true, false});
    facts.gpu->requirements.push_back({"bufferDeviceAddress", "feature", true, false});
    facts.gpu->requirements.push_back({"VK_KHR_present_wait", "extension", false, false});
    const auto message = AgcDriver::DescribeMissingRequirements(*facts.gpu);
    const std::string expected = "Vulkan: Test GPU (test-driver Test 1.2.3, Vulkan 1.3.280) lacks 2 items AnyPS5 needs: "
        "VK_KHR_8bit_storage (extension), bufferDeviceAddress (feature). Update the graphics driver; an older GPU or driver may not offer them. "
        "Other Vulkan devices on this machine: Other GPU; set ANYPS5_GPU to a part of one name to run on it. "
        "Start again with ANYPS5_HOST_REPORT=1 and attach the report to the issue.";
    Check(message == expected, "the message differs from the expected text");
    Check(message.find("VK_KHR_present_wait") == std::string::npos, "an optional item was listed as needed");
    facts.gpu->candidates = {"Test GPU"};
    facts.gpu->requirements = {{"shaderInt64", "feature", true, false}};
    facts.gpu->driverName.clear();
    facts.gpu->driverInfo.clear();
    const auto single = AgcDriver::DescribeMissingRequirements(*facts.gpu);
    Check(single.find("lacks 1 item AnyPS5 needs: shaderInt64 (feature).") != std::string::npos, "the singular form is wrong");
    Check(single.find("unknown driver") != std::string::npos, "an unknown driver is not named as unknown");
    Check(single.find("Other Vulkan devices") == std::string::npos, "the only device was offered as an alternative");
    facts.gpu->driverName = "HoneyKrisp";
    facts.gpu->requirements = {{"vertexPipelineStoresAndAtomics", "feature", true, false}};
    const auto honeykrisp = AgcDriver::DescribeMissingRequirements(*facts.gpu);
    Check(honeykrisp.find("hk_enable_vertex_pipeline_stores_atomics=true") != std::string::npos, "the Honeykrisp feature opt-in was not named");
    Check(honeykrisp.find("Update the graphics driver") == std::string::npos, "the Honeykrisp feature opt-in was misdiagnosed as an old driver");
    facts.gpu->requirements.push_back({"shaderInt64", "feature", true, false});
    const auto multiple = AgcDriver::DescribeMissingRequirements(*facts.gpu);
    Check(multiple.find("Other missing items may still require a graphics driver update") != std::string::npos, "other missing Honeykrisp features lost their update advice");
    facts.gpu->requirements = {{"shaderInt64", "feature", true, false}};
    const auto unrelated = AgcDriver::DescribeMissingRequirements(*facts.gpu);
    Check(unrelated.find("hk_enable_vertex_pipeline_stores_atomics") == std::string::npos, "the Honeykrisp opt-in was suggested for another missing feature");
    facts.gpu->driverName = "test-driver";
    facts.gpu->requirements = {{"vertexPipelineStoresAndAtomics", "feature", true, false}};
    const auto otherDriver = AgcDriver::DescribeMissingRequirements(*facts.gpu);
    Check(otherDriver.find("hk_enable_vertex_pipeline_stores_atomics") == std::string::npos, "the Honeykrisp opt-in was suggested for another driver");
    facts.gpu->name = "GPU of alice at /home/alice";
    const auto redacted = AgcDriver::DescribeMissingRequirements(*facts.gpu, {"alice"});
    Check(redacted.find("alice") == std::string::npos, "the message leaked a listed name");
}

AgcDriver::HostSources FakeSources() {
    AgcDriver::HostSources sources;
    sources.commit = "feedface";
    sources.os = [] { return std::string("Fake OS 1"); };
    sources.cpuModel = [] { return std::string("Fake CPU"); };
    sources.cpuThreads = [] { return std::uint64_t{8}; };
    sources.memoryMiB = [] { return std::uint64_t{16000}; };
    sources.cpuMissing = [] { return std::vector<std::string>{"BMI2"}; };
    sources.sdlLinked = [] { return std::string("2.30.0"); };
    sources.sdlCompiled = [] { return std::string("2.30.1"); };
    sources.shaderCache = [] { return AgcDriver::HostShaderCacheFacts{true, true, 7}; };
    sources.environment = [] { return std::vector<std::string>{"HOME=/home/alice", "VK_INSTANCE_LAYERS=layer", "APS5_B=2", "ANYPS5_A=1", "SDL_VIDEODRIVER=x11", "NOEQUALS", "=x", "PATH=/bin"}; };
    sources.redactions = [] { return std::vector<std::string>{"alice"}; };
    return sources;
}

void SourcesAreCollected() {
    const auto facts = AgcDriver::CollectHostFacts(FakeSources());
    Check(facts.commit == "feedface" && facts.os == "Fake OS 1" && facts.cpuModel == "Fake CPU", "a text source was not collected");
    Check(facts.cpuThreads == 8 && facts.memoryMiB == 16000, "a number source was not collected");
    Check(facts.cpuMissing && *facts.cpuMissing == std::vector<std::string>{"BMI2"}, "the CPU gate source was not collected");
    Check(facts.sdlLinked == "2.30.0" && facts.sdlCompiled == "2.30.1", "an SDL source was not collected");
    Check(facts.shaderCache && facts.shaderCache->sizeMiB == 7, "the shader cache source was not collected");
    Check(facts.redactions == std::vector<std::string>{"alice"}, "the redaction source was not collected");
    Check(facts.unavailable.empty(), "a working source was reported as unavailable");
    Check(!facts.gpu, "the collector invented a GPU");
    const std::vector<std::pair<std::string, std::string>> environment{{"ANYPS5_A", "1"}, {"APS5_B", "2"}, {"SDL_VIDEODRIVER", "x11"}, {"VK_INSTANCE_LAYERS", "layer"}};
    Check(facts.environment == environment, "the environment was not filtered and sorted");
}

void FailingSourcesAreRecorded() {
    auto sources = FakeSources();
    sources.os = [] () -> std::string { throw std::runtime_error("uname failed"); };
    sources.memoryMiB = nullptr;
    sources.cpuModel = [] () -> std::string { throw std::logic_error("cpuid"); };
    const auto facts = AgcDriver::CollectHostFacts(sources);
    Check(!facts.os && !facts.memoryMiB && !facts.cpuModel, "a failing source produced a value");
    Check(facts.cpuThreads == 8 && facts.sdlLinked == "2.30.0", "a failing source hid the others");
    const std::vector<std::string> unavailable{"os: uname failed", "cpu: cpuid", "memory: no source"};
    Check(facts.unavailable.size() == 3, "the failures were not all recorded");
    for (const auto& item : unavailable) Check(std::find(facts.unavailable.begin(), facts.unavailable.end(), item) != facts.unavailable.end(), item.c_str());
    const auto text = AgcDriver::FormatHostReport(facts);
    Check(text.find("os: unavailable\n") != std::string::npos && text.find("memory_mib: unavailable\n") != std::string::npos, "a failed source is not shown as unavailable");
    Check(text.find("[unavailable]\ncpu: cpuid\nmemory: no source\nos: uname failed\n") != std::string::npos, "the unavailable section is missing or unsorted");
    Check(AgcDriver::FormatHostReport(AgcDriver::CollectHostFacts(FakeSources())).find("[unavailable]") == std::string::npos, "a report with every fact printed an unavailable section");
    auto leaking = facts;
    leaking.unavailable.push_back("environment: cannot read /home/alice/.config");
    Check(AgcDriver::FormatHostReport(leaking).find("alice") == std::string::npos, "a failure message leaked a path");
    AgcDriver::HostSources none;
    const auto empty = AgcDriver::CollectHostFacts(none);
    Check(empty.unavailable.size() == 10 && !empty.os && empty.environment.empty() && empty.redactions.empty(), "an empty source set did not report every source");
}

void RedactionNamesAreDerived() {
    Check(AgcDriver::HostRedactionNames("alice", "alice-pc", "/home/alice/") == std::vector<std::string>{"alice", "alice-pc"}, "the user, host and home names were not derived");
    Check(AgcDriver::HostRedactionNames("alice", "box", "/home/ally") == std::vector<std::string>{"alice", "box", "ally"}, "a home name that differs from the user was dropped");
    Check(AgcDriver::HostRedactionNames("", "", "/home/ally/") == std::vector<std::string>{"ally"}, "a trailing separator hid the home directory name");
    Check(AgcDriver::HostRedactionNames("", "", "C:\\Users\\Bob") == std::vector<std::string>{"Bob"}, "a Windows home directory was not split");
    Check(AgcDriver::HostRedactionNames("", "", "").empty(), "empty inputs produced names");
}

bool Refused(const char* value, std::string& message) {
    try {
        static_cast<void>(AgcDriver::ParseHostReportSetting(value));
    } catch (const std::runtime_error& error) {
        message = error.what();
        return true;
    }
    return false;
}

void SettingIsParsed() {
    Check(!AgcDriver::ParseHostReportSetting(nullptr), "an unset variable turned the report on");
    Check(!AgcDriver::ParseHostReportSetting(""), "an empty value turned the report on");
    Check(!AgcDriver::ParseHostReportSetting("0"), "0 turned the report on");
    Check(AgcDriver::ParseHostReportSetting("1"), "1 did not turn the report on");
    for (const char* bad : {"2", "true", "yes", " 1", "1 ", "01", "-1", "on", "00"}) {
        std::string message;
        Check(Refused(bad, message), bad);
        Check(message.find("ANYPS5_HOST_REPORT") != std::string::npos && message.find(std::string("'") + bad + "'") != std::string::npos, "the refusal does not name the variable and the value");
    }
}

void NamedValuesAreRedacted() {
    const std::vector<std::string> names{"alice", "al", "workstation-7", ""};
    Check(AgcDriver::RedactHostText("user alice on workstation-7", names) == "user <redacted> on <redacted>", "a listed name was kept");
    Check(AgcDriver::RedactHostText("al's radeon", names) == "<redacted>'s radeon", "a short name hit inside a word or was kept");
    Check(AgcDriver::RedactHostText("alicia palice", names) == "alicia palice", "a name inside another word was replaced");
    Check(AgcDriver::RedactHostText("alice-pc", names) == "<redacted>-pc", "a name before a dash was kept");
    Check(AgcDriver::RedactHostText("no names here", {}) == "no names here", "text without names changed");
#ifdef _WIN32
    Check(AgcDriver::RedactHostText("user AlIcE on WORKSTATION-7", names) == "user <redacted> on <redacted>", "a Windows name in another case was kept");
    Check(AgcDriver::RedactHostText("Users/ALICE/cache", names) == "Users/<redacted>/cache", "a Windows name in a forward-slash path was kept");
    Check(AgcDriver::RedactHostText("ALICIA PALICE", names) == "ALICIA PALICE", "a name inside another word was replaced in another case");
    Check(AgcDriver::RedactHostText("user \xc3\xa9lise", {"\xc3\x89LISE"}) == "user <redacted>", "a UTF-8 name in another case was kept");
    Check(AgcDriver::RedactHostText("user \xd0\xb8\xd1\x80\xd0\xb8\xd0\xbd\xd0\xb0", {"\xd0\x98\xd0\xa0\xd0\x98\xd0\x9d\xd0\x90"}) == "user <redacted>", "a UTF-8 Cyrillic name in another case was kept");
    Check(AgcDriver::RedactHostText("user \xd0\xb8\xd1\x80\xd0\xb8\xd0\xbd\xd0\xb0\xd0\xb1", {"\xd0\x98\xd0\xa0\xd0\x98\xd0\x9d\xd0\x90"}) == "user \xd0\xb8\xd1\x80\xd0\xb8\xd0\xbd\xd0\xb0\xd0\xb1", "a name inside a longer word in another script was replaced");
    Check(AgcDriver::RedactHostText("path Users/\xc3\x89lise/cache", {"\xc3\x89LISE"}) == "path Users/<redacted>/cache", "a UTF-8 name in another case in a path was kept");
    Check(AgcDriver::RedactHostText("user \xd0\xb8 \x80", {"\xd0\xb8"}).find("\xd0\xb8") == std::string::npos, "a case-exact name in native ANSI text was kept");
    Check(AgcDriver::RedactHostText("user \xd0\xb8 \x80 \xd0\xb8", {"\xd0\xb8"}).find("\xd0\xb8") == std::string::npos, "a case-exact name in native ANSI contextual text was kept");
    Check(AgcDriver::RedactHostText("a\x80\x84" "b", {}).find("\x80\x84") != std::string::npos, "a byte the code page cannot represent was changed");
    Check(AgcDriver::RedactHostText("user \x80 alice", {"alice"}).find("alice") == std::string::npos, "a name in a text the code page cannot represent was kept");
    Check(AgcDriver::RedactHostText("user \x80 ALICE", {"alice"}).find("ALICE") == std::string::npos, "a name in another case in a text the code page cannot represent was kept");
#else
    Check(AgcDriver::RedactHostText("user AlIcE on WORKSTATION-7", names) == "user AlIcE on WORKSTATION-7", "a POSIX name in another case was replaced");
#endif
    auto facts = SampleFacts();
    facts.redactions = {"alice", "workstation-7"};
    facts.os = "Linux workstation-7 6.1";
    facts.gpu->name = "GPU of alice";
    facts.environment.push_back({"APS5_NOTE", "alice"});
    const auto text = AgcDriver::FormatHostReport(facts);
    Check(text.find("alice") == std::string::npos && text.find("workstation-7") == std::string::npos, "a listed name reached the report");
    Check(text.find("os: Linux <redacted> 6.1\n") != std::string::npos, "the host name was not replaced in the OS line");
    Check(text.find("device: GPU of <redacted>\n") != std::string::npos, "the user name was not replaced in the GPU name");
#ifdef _WIN32
    auto mixed = SampleFacts();
    mixed.redactions = {"alice", "workstation-7"};
    mixed.environment.push_back({"APS5_NOTE", "user ALICE on Workstation-7 at Users/ALICE/cache"});
    const auto mixedText = AgcDriver::FormatHostReport(mixed);
    Check(mixedText.find("ALICE") == std::string::npos && mixedText.find("Workstation-7") == std::string::npos, "a mixed-case name reached the Windows report");
    Check(mixedText.find("APS5_NOTE: user <redacted> on <redacted> at Users/<redacted>/cache\n") != std::string::npos, "a mixed-case name was not replaced in the Windows report");
    auto unicode = SampleFacts();
    unicode.redactions = {"\xc3\x89LISE"};
    unicode.environment.push_back({"APS5_NOTE", "user \xc3\xa9lise"});
    const auto unicodeText = AgcDriver::FormatHostReport(unicode);
    Check(unicodeText.find("\xc3\xa9lise") == std::string::npos, "a UTF-8 name reached the Windows report");
    Check(unicodeText.find("APS5_NOTE: user <redacted>\n") != std::string::npos, "a UTF-8 name was not replaced in the Windows report");
    auto ansi = SampleFacts();
    ansi.redactions = {"\xd0\xb8"};
    ansi.environment.push_back({"APS5_NOTE", "user \xd0\xb8 \x80 \xd0\xb8"});
    const auto ansiText = AgcDriver::FormatHostReport(ansi);
    Check(ansiText.find("\xd0\xb8") == std::string::npos, "a case-exact name reached the Windows report in native ANSI text");
#endif
}

void FormattedReportHasNoSensitiveText() {
    auto facts = SampleFacts();
    facts.os = "Linux 6.1 /home/alice/kernel";
    facts.cpuModel = "CPU at \\\\server\\share";
    facts.gpu->driverInfo = "Mesa 24 /usr/lib/x86_64-linux-gnu/dri/radeonsi_dri.so";
    facts.gpu->candidates.push_back("Dev 00:11:22:33:44:55");
    facts.environment.push_back({"ANYPS5_SHADER_CACHE_DIR", "/home/alice/cache"});
    facts.environment.push_back({"APS5_TRACE_DIR", "C:\\Users\\alice\\trace"});
    facts.environment.push_back({"APS5_NOTE", "line\nINJECTED: yes"});
    const auto text = AgcDriver::FormatHostReport(facts);
    for (const char* forbidden : {"alice", "/home", "/usr", "C:\\", "00:11:22", "\\\\server", "\nINJECTED"})
        Check(text.find(forbidden) == std::string::npos, forbidden);
    Check(text.find("ANYPS5_SHADER_CACHE_DIR: <path>\n") != std::string::npos, "an environment path value was not replaced");
    Check(text.find("driver_info: Mesa 24 <path>\n") != std::string::npos, "a path in the driver info was not replaced");
    Check(text.find("APS5_NOTE: line INJECTED: yes\n") != std::string::npos, "a newline in a value was not flattened");
}

}

int main() {
    GoldenOutput();
    OrderIsStable();
    MissingFactsAreNamed();
    DriverVersionsAreDecoded();
    PathsAndAddressesAreRedacted();
    NamedValuesAreRedacted();
    SettingIsParsed();
    MissingRequirementsAreDescribed();
    SourcesAreCollected();
    FailingSourcesAreRecorded();
    RedactionNamesAreDerived();
    FormattedReportHasNoSensitiveText();
    return failures == 0 ? 0 : 1;
}
