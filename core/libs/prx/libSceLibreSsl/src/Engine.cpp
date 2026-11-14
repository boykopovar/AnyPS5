#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstdint>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int EngineLibrary = 38;
constexpr int UnnamedFunction = 0xfff;
constexpr int PassedNullParameter = 67;
constexpr int NoSuchEngine = 116;

void PutEngineError(int reason) {
    LibreSslPutError_nid_no_patch(EngineLibrary, UnnamedFunction, reason);
}

int RequireNoEngine(const void* engine, const char* function) {
    if (engine != nullptr) NotImplemented_nid_no_patch(function);
    PutEngineError(PassedNullParameter);
    return 0;
}

}

extern "C" {

void* APS5_VABI ENGINE_by_id_nid_postfix(const char* id) {
    PutEngineError(id == nullptr ? PassedNullParameter : NoSuchEngine);
    return nullptr;
}

void APS5_VABI ENGINE_cleanup_nid_postfix(void) {
}

int APS5_VABI ENGINE_ctrl_nid_postfix(void* engine, int, std::int64_t, void*, void*) {
    return RequireNoEngine(engine, "ENGINE_ctrl of an ENGINE, which this library cannot create");
}

int APS5_VABI ENGINE_ctrl_cmd_nid_postfix(void* engine, const char* command, std::int64_t, void*, void*, int) {
    return RequireNoEngine(command == nullptr ? nullptr : engine, "ENGINE_ctrl_cmd of an ENGINE, which this library cannot create");
}

int APS5_VABI ENGINE_finish_nid_postfix(void* engine) {
    return RequireNoEngine(engine, "ENGINE_finish of an ENGINE, which this library cannot create");
}

int APS5_VABI ENGINE_free_nid_postfix(void* engine) {
    return RequireNoEngine(engine, "ENGINE_free of an ENGINE, which this library cannot create");
}

void* APS5_VABI ENGINE_get_first_nid_postfix(void) {
    return nullptr;
}

void* APS5_VABI ENGINE_get_next_nid_postfix(void* engine) {
    RequireNoEngine(engine, "ENGINE_get_next of an ENGINE, which this library cannot create");
    return nullptr;
}

int APS5_VABI ENGINE_init_nid_postfix(void* engine) {
    return RequireNoEngine(engine, "ENGINE_init of an ENGINE, which this library cannot create");
}

void APS5_VABI ENGINE_load_builtin_engines_nid_postfix(void) {
}

void* APS5_VABI ENGINE_load_private_key_nid_postfix(void* engine, const char*, void*, void*) {
    RequireNoEngine(engine, "ENGINE_load_private_key of an ENGINE, which this library cannot create");
    return nullptr;
}

}
