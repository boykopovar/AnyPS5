#include "prx/libSceNKWebKitRequirements/include/GuestSqlite.hpp"
#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"
#include "prx/libc/include/FilesystemError.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

struct BusyState {
    NKWebKit::GuestSqliteBusyHandler handler;
    void* argument;
};

struct AuthorizerState {
    NKWebKit::GuestSqliteAuthorizer authorizer;
    void* argument;
};

struct WalState {
    NKWebKit::GuestSqliteWalHook hook;
    void* argument;
};

struct CollationState {
    NKWebKit::GuestSqliteCollation compare;
    void* argument;
    NKWebKit::GuestSqliteDestructor destroy;
};

struct FunctionState {
    NKWebKit::GuestSqliteFunction function;
    NKWebKit::GuestSqliteFunction step;
    NKWebKit::GuestSqliteFinal final;
    void* userData;
};

struct DatabaseHooks {
    std::unique_ptr<BusyState> busy;
    std::unique_ptr<AuthorizerState> authorizer;
    std::unique_ptr<WalState> wal;
};

std::recursive_mutex g_hooksMutex;
std::unordered_map<sqlite3*, DatabaseHooks> g_hooks;

int BusyTrampoline(void* state, int attempts) {
    const auto* busy = static_cast<const BusyState*>(state);
    return busy->handler(busy->argument, attempts);
}

int AuthorizerTrampoline(void* state, int action, const char* first, const char* second, const char* database, const char* trigger) {
    const auto* authorizer = static_cast<const AuthorizerState*>(state);
    return authorizer->authorizer(authorizer->argument, action, first, second, database, trigger);
}

int WalTrampoline(void* state, sqlite3* db, const char* schema, int pages) {
    const auto* wal = static_cast<const WalState*>(state);
    return wal->hook(wal->argument, db, schema, pages);
}

int CollationTrampoline(void* state, int firstSize, const void* first, int secondSize, const void* second) {
    const auto* collation = static_cast<const CollationState*>(state);
    return collation->compare(collation->argument, firstSize, first, secondSize, second);
}

void CollationDestroy(void* state) {
    std::unique_ptr<CollationState> collation(static_cast<CollationState*>(state));
    if (collation->destroy) collation->destroy(collation->argument);
}

void FunctionTrampoline(sqlite3_context* context, int count, sqlite3_value** values) {
    static_cast<const FunctionState*>(sqlite3_user_data(context))->function(context, count, values);
}

void StepTrampoline(sqlite3_context* context, int count, sqlite3_value** values) {
    static_cast<const FunctionState*>(sqlite3_user_data(context))->step(context, count, values);
}

void FinalTrampoline(sqlite3_context* context) {
    static_cast<const FunctionState*>(sqlite3_user_data(context))->final(context);
}

void FunctionDestroy(void* state) {
    delete static_cast<FunctionState*>(state);
}

bool IsStaticOrTransient(NKWebKit::GuestSqliteDestructor destructor) {
    const auto value = reinterpret_cast<std::intptr_t>(destructor);
    return value == 0 || value == -1;
}

sqlite3_destructor_type HostDestructor(NKWebKit::GuestSqliteDestructor destructor) {
    return destructor == nullptr ? SQLITE_STATIC : SQLITE_TRANSIENT;
}

template <typename Bind>
int BindWithDestructor(const void* data, NKWebKit::GuestSqliteDestructor destructor, Bind bind) {
    if (IsStaticOrTransient(destructor)) return bind(HostDestructor(destructor));
    const int result = bind(SQLITE_TRANSIENT);
    if (data) destructor(const_cast<void*>(data));
    return result;
}

}

extern "C" {

int APS5_VABI sqlite3_bind_blob_nid_postfix(sqlite3_stmt* stmt, int index, const void* data, int size, NKWebKit::GuestSqliteDestructor destructor) {
    return BindWithDestructor(data, destructor, [&](sqlite3_destructor_type host) { return sqlite3_bind_blob(stmt, index, data, size, host); });
}

int APS5_VABI sqlite3_bind_double_nid_postfix(sqlite3_stmt* stmt, int index, double value) {
    return sqlite3_bind_double(stmt, index, value);
}

int APS5_VABI sqlite3_bind_int_nid_postfix(sqlite3_stmt* stmt, int index, int value) {
    return sqlite3_bind_int(stmt, index, value);
}

int APS5_VABI sqlite3_bind_int64_nid_postfix(sqlite3_stmt* stmt, int index, long long value) {
    return sqlite3_bind_int64(stmt, index, value);
}

int APS5_VABI sqlite3_bind_null_nid_postfix(sqlite3_stmt* stmt, int index) {
    return sqlite3_bind_null(stmt, index);
}

int APS5_VABI sqlite3_bind_parameter_count_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_bind_parameter_count(stmt);
}

int APS5_VABI sqlite3_bind_text16_nid_postfix(sqlite3_stmt* stmt, int index, const void* data, int size, NKWebKit::GuestSqliteDestructor destructor) {
    return BindWithDestructor(data, destructor, [&](sqlite3_destructor_type host) { return sqlite3_bind_text16(stmt, index, data, size, host); });
}

int APS5_VABI sqlite3_busy_handler_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteBusyHandler handler, void* argument) {
    if (!db) return SQLITE_MISUSE;
    std::lock_guard lock(g_hooksMutex);
    auto fresh = handler ? std::make_unique<BusyState>(BusyState{handler, argument}) : nullptr;
    const int result = sqlite3_busy_handler(db, handler ? BusyTrampoline : nullptr, fresh.get());
    if (result == SQLITE_OK) g_hooks[db].busy = std::move(fresh);
    return result;
}

int APS5_VABI sqlite3_busy_timeout_nid_postfix(sqlite3* db, int milliseconds) {
    if (!db) return SQLITE_MISUSE;
    std::lock_guard lock(g_hooksMutex);
    const int result = sqlite3_busy_timeout(db, milliseconds);
    if (result == SQLITE_OK) g_hooks[db].busy.reset();
    return result;
}

int APS5_VABI sqlite3_clear_bindings_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_clear_bindings(stmt);
}

int APS5_VABI sqlite3_close_nid_postfix(sqlite3* db) {
    std::lock_guard lock(g_hooksMutex);
    const int result = sqlite3_close(db);
    if (result == SQLITE_OK) g_hooks.erase(db);
    return result;
}

const void* APS5_VABI sqlite3_column_blob_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_blob(stmt, column);
}

int APS5_VABI sqlite3_column_bytes_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_bytes(stmt, column);
}

int APS5_VABI sqlite3_column_bytes16_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_bytes16(stmt, column);
}

const char* APS5_VABI sqlite3_column_decltype_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_decltype(stmt, column);
}

double APS5_VABI sqlite3_column_double_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_double(stmt, column);
}

int APS5_VABI sqlite3_column_int_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_int(stmt, column);
}

long long APS5_VABI sqlite3_column_int64_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_int64(stmt, column);
}

const void* APS5_VABI sqlite3_column_name16_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_name16(stmt, column);
}

const void* APS5_VABI sqlite3_column_text16_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_text16(stmt, column);
}

int APS5_VABI sqlite3_column_type_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_type(stmt, column);
}

sqlite3_value* APS5_VABI sqlite3_column_value_nid_postfix(sqlite3_stmt* stmt, int column) {
    return sqlite3_column_value(stmt, column);
}

int APS5_VABI sqlite3_create_collation_v2_nid_postfix(sqlite3* db, const char* name, int encoding, void* argument, NKWebKit::GuestSqliteCollation compare, NKWebKit::GuestSqliteDestructor destroy) {
    auto* state = new CollationState{compare, argument, destroy};
    return sqlite3_create_collation_v2(db, name, encoding, state, compare ? CollationTrampoline : nullptr, CollationDestroy);
}

int APS5_VABI sqlite3_create_function_nid_postfix(sqlite3* db, const char* name, int argumentCount, int encoding, void* userData, NKWebKit::GuestSqliteFunction function, NKWebKit::GuestSqliteFunction step, NKWebKit::GuestSqliteFinal final) {
    auto* state = new FunctionState{function, step, final, userData};
    return sqlite3_create_function_v2(db, name, argumentCount, encoding, state,
        function ? FunctionTrampoline : nullptr, step ? StepTrampoline : nullptr, final ? FinalTrampoline : nullptr, FunctionDestroy);
}

int APS5_VABI sqlite3_data_count_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_data_count(stmt);
}

int APS5_VABI sqlite3_errcode_nid_postfix(sqlite3* db) {
    return sqlite3_errcode(db);
}

const char* APS5_VABI sqlite3_errmsg_nid_postfix(sqlite3* db) {
    return sqlite3_errmsg(db);
}

const char* APS5_VABI sqlite3_errstr_nid_postfix(int code) {
    return sqlite3_errstr(code);
}

int APS5_VABI sqlite3_extended_result_codes_nid_postfix(sqlite3* db, int enabled) {
    return sqlite3_extended_result_codes(db, enabled);
}

int APS5_VABI sqlite3_finalize_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_finalize(stmt);
}

int APS5_VABI sqlite3_get_autocommit_nid_postfix(sqlite3* db) {
    return sqlite3_get_autocommit(db);
}

int APS5_VABI sqlite3_initialize_nid_postfix() {
    return sqlite3_initialize();
}

void APS5_VABI sqlite3_interrupt_nid_postfix(sqlite3* db) {
    sqlite3_interrupt(db);
}

long long APS5_VABI sqlite3_last_insert_rowid_nid_postfix(sqlite3* db) {
    return sqlite3_last_insert_rowid(db);
}

sqlite3_stmt* APS5_VABI sqlite3_next_stmt_nid_postfix(sqlite3* db, sqlite3_stmt* stmt) {
    return sqlite3_next_stmt(db, stmt);
}

int APS5_VABI sqlite3_open_v2_nid_postfix(const char* filename, sqlite3** db, int flags, const char* vfs) {
    if (!db) return SQLITE_MISUSE;
    *db = nullptr;
    if (vfs) throw std::runtime_error(std::string(__func__) + ": named VFS '" + vfs + "' is not implemented");
    if (!filename) return sqlite3_open_v2(filename, db, flags, nullptr);
    const std::string name = filename;
    if (name.empty() || name == ":memory:") return sqlite3_open_v2(filename, db, flags, nullptr);
    if ((flags & SQLITE_OPEN_URI) && name.rfind("file:", 0) == 0) throw std::runtime_error(std::string(__func__) + ": URI filenames are not implemented");
    std::filesystem::path resolved;
    if (ResolvePathError_nid_no_patch(filename, resolved) != 0) return SQLITE_CANTOPEN;
    const auto utf8 = resolved.u8string();
    const std::string native(utf8.begin(), utf8.end());
    return sqlite3_open_v2(native.c_str(), db, flags & ~SQLITE_OPEN_URI, nullptr);
}

int APS5_VABI sqlite3_prepare_v2_nid_postfix(sqlite3* db, const char* sql, int size, sqlite3_stmt** stmt, const char** tail) {
    return sqlite3_prepare_v2(db, sql, size, stmt, tail);
}

int APS5_VABI sqlite3_reset_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_reset(stmt);
}

void APS5_VABI sqlite3_result_error_nid_postfix(sqlite3_context* context, const char* message, int size) {
    sqlite3_result_error(context, message, size);
}

int APS5_VABI sqlite3_set_authorizer_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteAuthorizer authorizer, void* argument) {
    if (!db) return SQLITE_MISUSE;
    std::lock_guard lock(g_hooksMutex);
    auto fresh = authorizer ? std::make_unique<AuthorizerState>(AuthorizerState{authorizer, argument}) : nullptr;
    const int result = sqlite3_set_authorizer(db, authorizer ? AuthorizerTrampoline : nullptr, fresh.get());
    if (result == SQLITE_OK) g_hooks[db].authorizer = std::move(fresh);
    return result;
}

const char* APS5_VABI sqlite3_sql_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_sql(stmt);
}

int APS5_VABI sqlite3_step_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_step(stmt);
}

int APS5_VABI sqlite3_stmt_busy_nid_postfix(sqlite3_stmt* stmt) {
    return sqlite3_stmt_busy(stmt);
}

int APS5_VABI sqlite3_total_changes_nid_postfix(sqlite3* db) {
    return sqlite3_total_changes(db);
}

void* APS5_VABI sqlite3_user_data_nid_postfix(sqlite3_context* context) {
    return static_cast<const FunctionState*>(sqlite3_user_data(context))->userData;
}

double APS5_VABI sqlite3_value_double_nid_postfix(sqlite3_value* value) {
    return sqlite3_value_double(value);
}

const void* APS5_VABI sqlite3_value_text16_nid_postfix(sqlite3_value* value) {
    return sqlite3_value_text16(value);
}

int APS5_VABI sqlite3_value_type_nid_postfix(sqlite3_value* value) {
    return sqlite3_value_type(value);
}

int APS5_VABI sqlite3_wal_checkpoint_v2_nid_postfix(sqlite3* db, const char* schema, int mode, int* logFrames, int* checkpointedFrames) {
    return sqlite3_wal_checkpoint_v2(db, schema, mode, logFrames, checkpointedFrames);
}

void* APS5_VABI sqlite3_wal_hook_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteWalHook hook, void* argument) {
    if (!db) return nullptr;
    std::lock_guard lock(g_hooksMutex);
    auto fresh = hook ? std::make_unique<WalState>(WalState{hook, argument}) : nullptr;
    auto& slot = g_hooks[db].wal;
    void* previous = slot ? slot->argument : nullptr;
    sqlite3_wal_hook(db, hook ? WalTrampoline : nullptr, fresh.get());
    slot = std::move(fresh);
    return previous;
}

}
