#include "Archive.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Gpu.hpp"
#include "Native.hpp"
#include "Pack.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "ShaderDiskCache.hpp"
#include <iostream>
#include <map>
#include <set>
#include <spirv-tools/libspirv.hpp>

namespace ShaderArchive {
namespace {
struct Options {
    std::string command, device;
    fs::path paks, extracted, out, profile, select, requests, catalog, cache, publish;
    unsigned workers = std::min(16u, std::max(1u, std::thread::hardware_concurrency()));
    unsigned target = 0, sample = 0, limit = 0;
    bool offline = false, retain = false, validation = false, friendlyNames = false;
};
Options Arguments(int argc, char **argv) {
    Check(argc > 1,
          "usage: agc_shader_archive extract|build|replay|device --paks DIR|--extracted DIR|--requests LIST --out DIR "
          "[--jobs N] [--offline --profile JSON] [--sample N] [--select FILE] [--retain] [--validation]");
    Options o;
    o.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--offline") {
            o.offline = true;
            continue;
        }
        if (flag == "--retain") {
            o.retain = true;
            continue;
        }
        if (flag == "--validation") {
            o.validation = true;
            continue;
        }
        if (flag == "--friendly-names") {
            o.friendlyNames = true;
            continue;
        }
        Check(i + 1 < argc, "missing value for " + flag);
        const std::string value = argv[++i];
        if (flag == "--paks")
            o.paks = value;
        else if (flag == "--extracted")
            o.extracted = value;
        else if (flag == "--out")
            o.out = value;
        else if (flag == "--profile")
            o.profile = value;
        else if (flag == "--device")
            o.device = value;
        else if (flag == "--select")
            o.select = value;
        else if (flag == "--requests")
            o.requests = value;
        else if (flag == "--catalog")
            o.catalog = value;
        else if (flag == "--cache")
            o.cache = value;
        else if (flag == "--publish")
            o.publish = value;
        else if (flag == "--jobs" || flag == "--native-target" || flag == "--sample" || flag == "--limit") {
            std::size_t used = 0;
            auto n = std::stoul(value, &used);
            Check(used == value.size() && n <= 1000000, "invalid integer");
            if (flag == "--jobs")
                o.workers = n;
            else if (flag == "--native-target")
                o.target = n;
            else if (flag == "--sample")
                o.sample = n;
            else
                o.limit = n;
        } else
            throw std::runtime_error("unknown option " + flag);
    }
    Check(o.workers > 0 && o.workers <= 128, "jobs must be 1..128");
    Check(!(o.sample && o.limit), "sample and limit are exclusive");
    return o;
}
struct OfflineProfile {
    ShaderRecompiler::SpirvTarget target{};
    std::vector<unsigned> capabilities;
    std::vector<std::string> strings;
    std::vector<std::string_view> extensions;
    unsigned wave32 = 32, wave64 = 64;
    Json::Value json;
    explicit OfflineProfile(const fs::path &path) {
        Check(!path.empty(), "--offline requires --profile for a declared target");
        json = ReadJson(path);
        const auto &p = json;
        Check(p["schema"].asUInt() == 1, "unknown profile schema");
        for (const auto &c : p["capabilities"])
            capabilities.push_back(c.asUInt());
        for (const auto &e : p["extensions"])
            strings.push_back(e.asString());
        for (const auto &e : strings)
            extensions.push_back(e);
        target.vulkanVersion = p["vulkan_version"].asUInt();
        target.spirvVersion = p["spirv_version"].asUInt();
        wave32 = p["subgroup_size"].asUInt();
        wave64 = p["wave64_subgroup_size"].asUInt();
        target.subgroupSize = wave32;
        target.bdaAbiVersion = p["bda_abi_version"].asUInt();
        target.supportedCapabilities = capabilities;
        target.supportedExtensions = extensions;
        target.fragmentShaderBarycentricEnabled = p["fragment_shader_barycentric"].asBool();
        for (unsigned i = 0; i < 3; ++i)
            target.maxWorkgroupSize[i] = p["max_workgroup_size"][i].asUInt();
        target.maxWorkgroupInvocations = p["max_workgroup_invocations"].asUInt();
        target.maxWorkgroupSharedMemoryBytes = p["max_workgroup_shared_memory_bytes"].asUInt();
        if (!p["mesh_limits"].isNull()) {
            const auto &m = p["mesh_limits"];
            Check(m.size() == 11, "mesh limit count");
            target.mesh = ShaderRecompiler::MeshTargetLimits{{m[0].asUInt(), m[1].asUInt(), m[2].asUInt()},
                                                             m[3].asUInt(),
                                                             m[4].asUInt(),
                                                             m[5].asUInt(),
                                                             m[6].asUInt(),
                                                             m[7].asUInt(),
                                                             m[8].asUInt(),
                                                             m[9].asUInt(),
                                                             m[10].asUInt()};
        }
        target.nonConstantImageOffsets = p["non_constant_image_offsets"].asBool();
        target.srgbDecodeFormats = p["srgb_decode_formats"].asUInt();
        target.narrowSubgroupClock = p["narrow_subgroup_clock"].asBool();
        Check(!p["native_float32_fma"].asBool(), "unsupported standalone profile configuration");
        Check(!p["native_float16_minmax_zero_order"].asBool(), "unsupported standalone profile configuration");
        Check(!p["native_float32_flush_to_zero_mixed"].asBool(), "unsupported standalone profile configuration");
        Check(p["push_constant_bytes"].asUInt() == 128 && p["mesh_input_primitive"].asUInt() == 1 &&
                  !p["dual_source_blend"].asBool(),
              "unsupported standalone profile configuration");
        for (unsigned i = 0; i < 32; ++i)
            Check(p["interpolator_settings"][i].asUInt() == i, "custom interpolants require linked requests");
        for (unsigned i = 0; i < 8; ++i)
            Check(p["target_export_mapping"][i].asUInt() == i && p["target_export_packing"][i].asUInt() == 0,
                  "custom exports require linked requests");
    }
};
std::string Diagnostic(std::string text) {
    if (auto p = text.find("\nRecompileRequest:"); p != std::string::npos)
        text.resize(p);
    return text;
}
std::string Kind(const std::string &message) {
    if (message.find("unsupported MIMG") != std::string::npos || message.find("unsupported DS") != std::string::npos)
        return "decoder";
    if (message.find("ancillary") != std::string::npos)
        return "pixel_ancillary";
    if (message.find("aperture") != std::string::npos)
        return "scalar_aperture";
    if (message.find("merge block") != std::string::npos)
        return "control_flow";
    if (message.find("maintenance8") != std::string::npos)
        return "target_profile";
    return "other";
}
} // namespace
int Run(int argc, char **argv) {
    const auto o = Arguments(argc, argv);
    const auto started = Clock::now();
    if (o.command == "pack") {
        Check(!o.cache.empty() && !o.out.empty(), "pack requires --cache DIR --out FILE.pack");
        std::cout << JsonText(PackFolder(o.cache, o.out, o.limit)) << '\n';
        return 0;
    }
    if (o.command == "probe") {
        Check(!o.requests.empty(), "probe requires --requests LIST");
        std::ifstream list(o.requests);
        Check(bool(list), "open request list");
        Json::Value report, rows(Json::arrayValue);
        std::size_t count = 0;
        for (std::string line; std::getline(list, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (o.limit && count >= o.limit) break;
            const auto bytes = ReadFile(line);
            auto owned = ShaderRecompiler::RequestSerializer{}.Deserialize(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            std::vector<std::byte> key;
            ShaderRecompiler::ShaderDiskCache::BuildKey(owned.request, owned.request.target.subgroupSize, key);
            ShaderRecompiler::CompiledVariant variant;
            const auto begin = Clock::now();
            const bool hit = ShaderRecompiler::ShaderDiskCache::Load(key, variant);
            Json::Value row;
            row["request"] = line;
            row["hit"] = hit;
            row["load_seconds"] = Seconds(begin);
            if (hit) {
                const auto& words = variant.artifact.spirv.Words();
                row["spirv_sha256"] = Sha256({reinterpret_cast<const std::uint8_t*>(words.data()), words.size() * 4});
            }
            rows.append(row);
            ++count;
        }
        ShaderRecompiler::ShaderDiskCache::Flush();
        const auto totals = ShaderRecompiler::ShaderDiskCache::Totals();
        report["requests"] = rows;
        report["hits"] = Json::UInt64(totals.hits);
        report["prebuilt_hits"] = Json::UInt64(totals.prebuiltHits);
        report["misses"] = Json::UInt64(totals.misses);
        report["lookup_ms"] = totals.lookupNanoseconds / 1e6;
        report["read_ms"] = totals.readNanoseconds / 1e6;
        report["decode_ms"] = totals.decodeNanoseconds / 1e6;
        report["load_ms"] = totals.loadNanoseconds / 1e6;
        report["max_load_ms"] = totals.maxLoadNanoseconds / 1e6;
        report["seconds"] = Seconds(started);
        if (!o.out.empty()) WriteJson(o.out, report);
        std::cout << JsonText(report) << '\n';
        return count && totals.hits == count ? 0 : 1;
    }
    if (o.command == "extract") {
        Check(!o.paks.empty() && !o.out.empty(), "extract requires --paks and --out");
        auto a = Extract(o.paks, o.workers, o.out);
        std::cout << JsonText(a.report) << '\n';
        return 0;
    }
    Check(o.command == "device" || o.command == "build" || o.command == "replay", "unknown command");
    std::unique_ptr<GpuCompiler> gpu;
    if (!o.offline)
        gpu = std::make_unique<GpuCompiler>(o.device, o.workers, o.validation);
    if (o.command == "device") {
        Check(bool(gpu), "device requires Vulkan");
        std::cout << JsonText(gpu->Info()) << '\n';
        return gpu->Info()["validation_errors"].asUInt() != 0 ? 1 : 0;
    }
    Check(!o.out.empty() && !fs::exists(o.out), "choose a fresh --out directory");
    fs::create_directories(o.out / "failures");
    if (o.retain)
        fs::create_directories(o.out / "artifacts");
    if (!o.cache.empty())
        fs::create_directories(o.cache);
    if (gpu)
        fs::create_directories(o.out / "gpu");
    std::unique_ptr<OfflineProfile> profile;
    if (o.offline && !o.profile.empty())
        profile = std::make_unique<OfflineProfile>(o.profile);
    Check(o.command != "build" || gpu || profile, "offline archive builds require --profile");
    auto profileJson = gpu ? gpu->Info() : profile ? profile->json : Json::Value(Json::objectValue);
    if (!gpu && o.command == "replay") profileJson["target_source"] = "serialized_request";
    const auto profileText = JsonText(profileJson);
    const auto profileHash = Sha256({reinterpret_cast<const std::uint8_t *>(profileText.data()), profileText.size()});
    WriteJson(o.out / "profile.json", profileJson);
    std::vector<Program> programs;
    std::vector<fs::path> requests;
    std::vector<std::size_t> selected;
    std::size_t total = 0, occurrences = 0;
    double extractionSeconds = 0, planningSeconds = 0;
    Json::Value indexed;
    if (o.command == "build") {
        std::set<std::string> filter;
        if (!o.select.empty()) {
            std::ifstream f(o.select);
            Check(bool(f), "open selection file");
            for (std::string line; std::getline(f, line);) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                    filter.insert(line);
            }
        }
        Check(o.paks.empty() != o.extracted.empty(), "build requires exactly one of --paks or --extracted");
        if (!o.catalog.empty()) {
            Check(!o.extracted.empty(), "--catalog requires --extracted");
            const auto t = Clock::now();
            programs =
                LoadProgramIndex(o.catalog, o.extracted, filter, o.target, o.sample, o.limit, o.workers, indexed);
            planningSeconds = Seconds(t);
            total = indexed["planned_jobs"].asUInt64();
        } else {
            const auto t = Clock::now();
            auto archive = o.paks.empty() ? LoadExtracted(o.extracted, o.workers) : Extract(o.paks, o.workers);
            extractionSeconds = Seconds(t);
            WriteJson(o.out / "extraction.json", archive.report);
            const auto p = Clock::now();
            programs = Programs(archive, o.workers);
            WriteProgramIndex(o.out / "programs.idx", programs);
            planningSeconds = Seconds(p);
            total = programs.size();
        }
        std::set<std::string> matched;
        std::ofstream manifest(o.out / "programs.jsonl", std::ios::binary);
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto &p = programs[i];
            occurrences += p.aliases.size();
            Json::Value v;
            v["id"] = p.id;
            v["code_sha256"] = p.native.codeHash;
            v["header_sha256"] = p.native.headerHash;
            v["native_target"] = p.native.target;
            v["native_type"] = p.native.type;
            v["aliases"] = p.aliases;
            manifest << JsonText(v) << '\n';
            if (o.target && p.native.target != o.target)
                continue;
            if (!filter.empty()) {
                bool found = false;
                for (const auto &value : {p.id, p.native.codeHash})
                    if (filter.contains(value)) {
                        matched.insert(value);
                        found = true;
                    }
                if (!found)
                    continue;
            }
            selected.push_back(i);
        }
        manifest.close();
        Check(bool(manifest), "write programs manifest");
        if (o.catalog.empty())
            Check(matched == filter, "selection contains unknown/excluded identities");
        else
            occurrences = indexed["native_occurrences"].asUInt64();
    } else {
        Check(!o.requests.empty(), "replay requires --requests list");
        std::ifstream list(o.requests);
        Check(bool(list), "open request list");
        for (std::string line; std::getline(list, line);) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                requests.emplace_back(line);
        }
        total = requests.size();
        for (std::size_t i = 0; i < total; ++i)
            selected.push_back(i);
    }
    const auto eligible = o.catalog.empty() ? selected.size() : indexed["eligible_jobs"].asUInt64();
    if (o.catalog.empty() && o.sample && o.sample < selected.size()) {
        std::vector<std::size_t> sample;
        for (std::size_t i = 0; i < o.sample; ++i)
            sample.push_back(selected[i * selected.size() / o.sample]);
        selected = std::move(sample);
    }
    if (o.limit && selected.size() > o.limit)
        selected.resize(o.limit);
    Check(!selected.empty(), "no shaders selected");
    std::unique_ptr<PackWriter> publisher;
    if (!o.publish.empty()) publisher = std::make_unique<PackWriter>(o.publish, selected.size());
    std::ofstream results(o.out / "results.jsonl.partial", std::ios::binary),
        failures(o.out / "failures.jsonl", std::ios::binary);
    std::mutex outputMutex;
    Json::Value counts, groups, phaseTotals;
    std::size_t cacheHits = 0;
    std::size_t completed = 0;
    const auto compileStart = Clock::now();
    std::vector<std::map<spv_target_env, std::unique_ptr<spvtools::SpirvTools>>> validators(o.workers);
    const auto target = gpu ? gpu->Target() : profile ? profile->target : ShaderRecompiler::SpirvTarget{};
    Parallel(selected.size(), o.workers, [&](std::size_t index, unsigned worker) {
        const auto i = selected[index];
        Json::Value result;
        std::string id;
        std::unique_ptr<RequestData> data;
        std::string requestText;
        const auto jobStarted = Clock::now();
        std::string phase = "prepare";
        auto compile = [&](ShaderRecompiler::RecompileRequest &request) {
            request.useCache = false;
            if (gpu) {
                request.target = gpu->Target();
                request.target.subgroupSize = gpu->Subgroup(request.context.waveSize, request.shader.stage);
            }
            result["stage"] = unsigned(request.shader.stage);
            result["wave_size"] = request.context.waveSize;
            result["vulkan_version"] = request.target.vulkanVersion;
            result["spirv_version"] = request.target.spirvVersion;
            if (o.retain)
                WriteText(o.out / "artifacts" / (id + ".req"),
                          ShaderRecompiler::RequestSerializer{}.Serialize(request));
            result["prepare_seconds"] = Seconds(jobStarted);
            phase = "compiler";
            std::vector<std::byte> cacheKey;
            fs::path cacheFile;
            std::shared_ptr<const ShaderRecompiler::CompiledVariant> variant;
            bool hit = false;
            const auto cacheStart = Clock::now();
            if (!o.cache.empty()) {
                ShaderRecompiler::ShaderDiskCache::BuildKey(request, request.target.subgroupSize, cacheKey);
                const std::string receipt = std::string("archive-validated-v1:") + spvSoftwareVersionDetailsString();
                cacheKey.insert(cacheKey.end(), reinterpret_cast<const std::byte *>(receipt.data()),
                                reinterpret_cast<const std::byte *>(receipt.data() + receipt.size()));
                cacheFile = o.cache / ShaderRecompiler::ShaderDiskCache::EntryName(cacheKey);
                if (fs::exists(cacheFile)) {
                    auto file = ReadFile(cacheFile);
                    auto decoded = std::make_shared<ShaderRecompiler::CompiledVariant>();
                    if (ShaderRecompiler::ShaderDiskCache::DecodeEntry(
                            {reinterpret_cast<const std::byte *>(file.data()), file.size()}, cacheKey, *decoded) ==
                        ShaderRecompiler::ShaderDiskCache::LoadStatus::Loaded) {
                        decoded->layout = request.layout;
                        variant = std::move(decoded);
                        hit = true;
                    } else
                        result["cache_rejected"] = true;
                }
            }
            result["cache_read_seconds"] = Seconds(cacheStart);
            result["cache_hit"] = hit;
            const auto t = Clock::now();
            ShaderRecompiler::OfflineCompileTimings timings;
            if (!variant)
                variant = ShaderRecompiler::CompileShaderUncached(request, nullptr, &timings);
            result["compile_seconds"] = Seconds(t);
            result["frontend_seconds"] = timings.frontendSeconds;
            result["backend_seconds"] = timings.backendSeconds;
            const auto hashStart = Clock::now();
            const auto &words = variant->artifact.spirv.Words();
            result["spirv_bytes"] = Json::UInt64(words.size() * 4);
            result["spirv_sha256"] = Sha256({reinterpret_cast<const std::uint8_t *>(words.data()), words.size() * 4});
            result["hash_seconds"] = Seconds(hashStart);
            phase = "validation";
            const auto validationStart = Clock::now();
            if (!hit) {
                std::string messages;
                const auto api = request.target.vulkanVersion;
                const auto environment = api >= VK_API_VERSION_1_3 ? SPV_ENV_VULKAN_1_3 : api >= VK_API_VERSION_1_2 ? SPV_ENV_VULKAN_1_2 : api >= VK_API_VERSION_1_1 ? (request.target.spirvVersion >= 0x00010400 ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1) : SPV_ENV_VULKAN_1_0;
                auto& validator = validators[worker][environment];
                if (!validator) validator = std::make_unique<spvtools::SpirvTools>(environment);
                validator->SetMessageConsumer([&](auto, const char *, const spv_position_t &p, const char *m) {
                    messages += std::to_string(p.index) + ": " + m + "\n";
                });
                spvtools::ValidatorOptions options;
                options.SetFriendlyNames(o.friendlyNames);
                options.SetAllowOffsetTextureOperand(request.target.nonConstantImageOffsets);
                const bool valid = validator->Validate(words.data(), words.size(), options);
                result["validation_seconds"] = Seconds(validationStart);
                Check(valid, "SPIR-V validation: " + messages);
            } else {
                result["validation_seconds"] = 0.0;
                result["validation_reused"] = true;
            }
            result["status"] = "validated";
            const auto storeStart = Clock::now();
            if (!cacheFile.empty() && !hit) {
                auto entry = ShaderRecompiler::ShaderDiskCache::EncodeEntry(cacheKey, *variant);
                WriteAtomic(cacheFile, {reinterpret_cast<const std::uint8_t *>(entry.data()), entry.size()});
            }
            result["cache_write_seconds"] = Seconds(storeStart);
            if (publisher) {
                const auto publishStart = Clock::now();
                std::vector<std::byte> key;
                ShaderRecompiler::ShaderDiskCache::BuildKey(request, variant->artifact.hostSubgroupSize, key);
                publisher->Add(ShaderRecompiler::ShaderDiskCache::EncodeEntry(key, *variant));
                result["publish_seconds"] = Seconds(publishStart);
            }
            if (o.retain) {
                WriteFile(o.out / "artifacts" / (id + ".spv"),
                          {reinterpret_cast<const std::uint8_t *>(words.data()), words.size() * 4});
                std::vector<std::byte> key;
                ShaderRecompiler::ShaderDiskCache::BuildKey(request, variant->artifact.hostSubgroupSize, key);
                auto entry = ShaderRecompiler::ShaderDiskCache::EncodeEntry(key, *variant);
                WriteFile(o.out / "artifacts" / (id + ".artifact"),
                          {reinterpret_cast<const std::uint8_t *>(entry.data()), entry.size()});
            }
            if (gpu) {
                phase = "gpu";
                std::fprintf(stderr, "GPU %s stage %u\n", id.c_str(), unsigned(request.shader.stage));
                const auto g = Clock::now();
                gpu->Compile(request, *variant, worker);
                result["gpu_seconds"] = Seconds(g);
                result["status"] = "gpu_compiled";
            }
        };
        try {
            if (o.command == "build") {
                const auto &p = programs[i];
                id = p.id;
                result["code_sha256"] = p.native.codeHash;
                result["header_sha256"] = p.native.headerHash;
                result["native_target"] = p.native.target;
                data = MakeRequest(p, target, gpu ? 32 : profile->wave32, gpu ? 64 : profile->wave64);
                compile(data->request);
            } else {
                const auto bytes = ReadFile(requests[i]);
                requestText.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
                id = requests[i].parent_path().filename().string();
                if (id.size() != 64)
                    id = Sha256(bytes);
                auto owned = ShaderRecompiler::RequestSerializer{}.Deserialize(requestText);
                compile(owned.request);
            }
        } catch (const std::exception &error) {
            auto diagnostic = Diagnostic(error.what());
            const bool config = diagnostic.starts_with("needs_configuration:");
            result["status"] = config                  ? "needs_configuration"
                               : phase == "gpu"        ? "gpu_failed"
                               : phase == "validation" ? "validation_failed"
                               : phase == "prepare"    ? "prepare_failed"
                                                       : "compiler_failed";
            result["diagnostic"] = diagnostic;
            result["failure_kind"] = config ? "stage_pairing" : Kind(diagnostic);
            if (!id.empty()) {
                if (data)
                    requestText = ShaderRecompiler::RequestSerializer{}.Serialize(data->request);
                if (!requestText.empty())
                    WriteText(o.out / "failures" / (id + ".req"), requestText);
                WriteText(o.out / "failures" / (id + ".log"), diagnostic + "\n");
            }
        }
        result["id"] = id;
        result["profile_sha256"] = profileHash;
        result["seconds"] = Seconds(jobStarted);
        std::lock_guard lock(outputMutex);
        if (result["cache_hit"].asBool())
            ++cacheHits;
        for (const auto &key : result.getMemberNames())
            if (key.ends_with("_seconds"))
                phaseTotals[key] = phaseTotals[key].asDouble() + result[key].asDouble();
        const auto status = result["status"].asString();
        counts[status] = Json::UInt64(counts[status].asUInt64() + 1);
        results << JsonText(result) << '\n';
        if (gpu)
            results.flush();
        if (status != "validated" && status != "gpu_compiled") {
            failures << JsonText(result) << '\n';
            auto key = result["failure_kind"].asString();
            groups[key] = Json::UInt64(groups[key].asUInt64() + 1);
        }
        Check(bool(results) && bool(failures), "write compilation results");
        ++completed;
        if (completed % 100 == 0 || completed == selected.size())
            std::fprintf(stderr, "%zu/%zu shaders, %.1f seconds\n", completed, selected.size(), Seconds(compileStart));
    });
    results.close();
    failures.close();
    Check(bool(results) && bool(failures), "finish results");
    fs::rename(o.out / "results.jsonl.partial", o.out / "results.jsonl");
    if (gpu) WriteJson(o.out / "device.json", gpu->Info());
    Json::Value report;
    report["schema"] = 1;
    report["planned_jobs"] = Json::UInt64(total);
    report["eligible_jobs"] = Json::UInt64(eligible);
    report["attempted"] = Json::UInt64(completed);
    report["unattempted"] = Json::UInt64(total - completed);
    report["complete_plan"] = total == completed;
    report["native_occurrences"] = Json::UInt64(occurrences);
    report["results"] = counts;
    report["cache_hits"] = Json::UInt64(cacheHits);
    report["worker_phase_seconds"] = phaseTotals;
    report["process"] = ProcessUsage();
    report["indexed"] = indexed;
    report["friendly_names"] = o.friendlyNames;
    report["failure_kinds"] = groups;
    report["profile_sha256"] = profileHash;
    report["compiler_source"] = Json::UInt64(ShaderRecompiler::ShaderDiskCache::SourceVersion());
    report["cache_format"] = ShaderRecompiler::ShaderDiskCache::FormatVersion;
    report["workers"] = o.workers;
    report["extract_seconds"] = extractionSeconds;
    report["plan_seconds"] = planningSeconds;
    report["compile_seconds"] = Seconds(compileStart);
    report["seconds"] = Seconds(started);
    report["gpu"] = bool(gpu);
    if (publisher) report["published"] = publisher->Finish();
    if (gpu)
        report["device"] = gpu->Info();
    WriteJson(o.out / "report.json", report);
    std::cout << JsonText(report) << '\n';
    return gpu && gpu->Info()["validation_errors"].asUInt() != 0 ? 1 : 0;
}
} // namespace ShaderArchive
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h"))) {
        std::cout << "agc_shader_archive extract --paks DIR --out FRESH [--jobs N]\n"
                     "agc_shader_archive build (--paks DIR | --extracted DIR) --out FRESH [options]\n"
                     "agc_shader_archive replay --requests LIST --out FRESH [options]\n"
                     "agc_shader_archive device [--device NAME] [--validation]\n\n"
                     "agc_shader_archive pack --cache DIR --out FILE.pack [--limit N]\n\n"
                     "agc_shader_archive probe --requests LIST [--out JSON] [--limit N]\n\n"
                     "Options: --jobs N, --select FILE, --sample N, --limit N, --native-target 0|14|15,\n"
                     "  --catalog programs.idx (with --extracted), --cache DIR, --retain, --publish FILE.pack,\n"
                     "  --offline [--profile JSON (required for archive builds)], --device NAME, --validation, --friendly-names\n"
                     "GPU compilation is enabled by default. Selectors are full program IDs or code SHA256s.\n";
        return 0;
    }
    try {
        return ShaderArchive::Run(argc, argv);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
}
