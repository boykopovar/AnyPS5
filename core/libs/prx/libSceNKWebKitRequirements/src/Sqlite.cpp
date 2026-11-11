#include "prx/libSceNKWebKitRequirements/include/Sqlite.hpp"
#include "prx/libc/include/General.hpp"

#include <sqlite3.h>

#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <system_error>
#endif

namespace {

constexpr const char* GuestHooksKey = "anyps5.guest";

struct GuestHooks {
    NKWebKit::GuestSqliteBusy busy = nullptr;
    void* busyArgument = nullptr;
    NKWebKit::GuestSqliteAuthorizer authorizer = nullptr;
    void* authorizerArgument = nullptr;
    NKWebKit::GuestSqliteWalHook walHook = nullptr;
    void* walArgument = nullptr;
};

struct GuestCollation {
    NKWebKit::GuestSqliteCompare compare;
    NKWebKit::GuestSqliteDestructor destroy;
    void* argument;
};

struct GuestFunction {
    NKWebKit::GuestSqliteFunction function;
    NKWebKit::GuestSqliteFunction step;
    NKWebKit::GuestSqliteFinal final;
    void* application;
};

void FreeHooks(void* hooks) {
    delete static_cast<GuestHooks*>(hooks);
}

GuestHooks* Hooks(sqlite3* db) {
    if (auto* hooks = static_cast<GuestHooks*>(sqlite3_get_clientdata(db, GuestHooksKey))) {
        return hooks;
    }
    auto* hooks = new (std::nothrow) GuestHooks();
    if (hooks && sqlite3_set_clientdata(db, GuestHooksKey, hooks, FreeHooks) != SQLITE_OK) {
        return nullptr;
    }
    return hooks;
}

int CallBusy(void* hooks, int count) {
    auto* guest = static_cast<GuestHooks*>(hooks);
    return guest->busy(guest->busyArgument, count);
}

int CallAuthorizer(void* hooks, int action, const char* first, const char* second, const char* schema,
    const char* trigger) {
    if (action == SQLITE_ATTACH && first != nullptr && *first != '\0') return SQLITE_DENY;
    auto* guest = static_cast<GuestHooks*>(hooks);
    if (guest->authorizer == nullptr) return SQLITE_OK;
    return guest->authorizer(guest->authorizerArgument, action, first, second, schema, trigger);
}

int CallWalHook(void* hooks, sqlite3* db, const char* schema, int frames) {
    auto* guest = static_cast<GuestHooks*>(hooks);
    return guest->walHook(guest->walArgument, db, schema, frames);
}

int CallCompare(void* collation, int leftSize, const void* left, int rightSize, const void* right) {
    auto* guest = static_cast<GuestCollation*>(collation);
    return guest->compare(guest->argument, leftSize, left, rightSize, right);
}

void DestroyCollation(void* collation) {
    auto* guest = static_cast<GuestCollation*>(collation);
    if (guest->destroy) {
        guest->destroy(guest->argument);
    }
    delete guest;
}

void CallFunction(sqlite3_context* context, int count, sqlite3_value** values) {
    static_cast<GuestFunction*>(sqlite3_user_data(context))->function(context, count, values);
}

void CallStep(sqlite3_context* context, int count, sqlite3_value** values) {
    static_cast<GuestFunction*>(sqlite3_user_data(context))->step(context, count, values);
}

void CallFinal(sqlite3_context* context) {
    static_cast<GuestFunction*>(sqlite3_user_data(context))->final(context);
}

void DestroyFunction(void* function) {
    delete static_cast<GuestFunction*>(function);
}

#ifdef _WIN32

void CallBindingDestructor(void* data, NKWebKit::GuestSqliteDestructor destructor) {
    destructor(data);
}

struct BindingDestructor {
    void* code = nullptr;

    ~BindingDestructor() {
        if (code) VirtualFree(code, 0, MEM_RELEASE);
    }
};

sqlite3_destructor_type HostDestructor(NKWebKit::GuestSqliteDestructor destructor) {
    if (!destructor) return SQLITE_STATIC;
    if (reinterpret_cast<std::intptr_t>(destructor) == -1) return SQLITE_TRANSIENT;
    static std::mutex mutex;
    static std::map<NKWebKit::GuestSqliteDestructor, std::unique_ptr<BindingDestructor>> destructors;
    std::lock_guard lock(mutex);
    if (const auto found = destructors.find(destructor); found != destructors.end()) {
        return reinterpret_cast<sqlite3_destructor_type>(found->second->code);
    }
    std::array<unsigned char, 26> bytes{
        0xF3, 0x0F, 0x1E, 0xFA, 0x48, 0xBA, 0, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0
    };
    const auto guest = reinterpret_cast<std::uintptr_t>(destructor);
    const auto host = reinterpret_cast<std::uintptr_t>(&CallBindingDestructor);
    static_assert(sizeof(guest) == 8);
    std::memcpy(bytes.data() + 6, &guest, sizeof(guest));
    std::memcpy(bytes.data() + 16, &host, sizeof(host));
    auto thunk = std::make_unique<BindingDestructor>();
    thunk->code = VirtualAlloc(nullptr, bytes.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!thunk->code) throw std::bad_alloc();
    std::memcpy(thunk->code, bytes.data(), bytes.size());
    DWORD previous;
    if (!VirtualProtect(thunk->code, bytes.size(), PAGE_EXECUTE_READ, &previous) ||
        !FlushInstructionCache(GetCurrentProcess(), thunk->code, bytes.size())) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
    }
    const auto result = reinterpret_cast<sqlite3_destructor_type>(thunk->code);
    destructors.emplace(destructor, std::move(thunk));
    return result;
}

#else

sqlite3_destructor_type HostDestructor(NKWebKit::GuestSqliteDestructor destructor) {
    return destructor;
}

#endif

}

extern "C" {

int APS5_VABI sqlite3_bind_blob_nid_postfix(sqlite3_stmt* statement, int index, const void* data, int size,
    NKWebKit::GuestSqliteDestructor destructor) {
    return sqlite3_bind_blob(statement, index, data, size, HostDestructor(destructor));
}

int APS5_VABI sqlite3_bind_double_nid_postfix(sqlite3_stmt* statement, int index, double value) {
    return sqlite3_bind_double(statement, index, value);
}

int APS5_VABI sqlite3_bind_int_nid_postfix(sqlite3_stmt* statement, int index, int value) {
    return sqlite3_bind_int(statement, index, value);
}

int APS5_VABI sqlite3_bind_int64_nid_postfix(sqlite3_stmt* statement, int index, std::int64_t value) {
    return sqlite3_bind_int64(statement, index, value);
}

int APS5_VABI sqlite3_bind_null_nid_postfix(sqlite3_stmt* statement, int index) {
    return sqlite3_bind_null(statement, index);
}

int APS5_VABI sqlite3_bind_parameter_count_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_bind_parameter_count(statement);
}

int APS5_VABI sqlite3_bind_text16_nid_postfix(sqlite3_stmt* statement, int index, const void* text, int size,
    NKWebKit::GuestSqliteDestructor destructor) {
    return sqlite3_bind_text16(statement, index, text, size, HostDestructor(destructor));
}

int APS5_VABI sqlite3_busy_handler_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteBusy handler, void* argument) {
    if (!db || !handler) {
        return sqlite3_busy_handler(db, nullptr, nullptr);
    }
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    GuestHooks* hooks = Hooks(db);
    int result = SQLITE_NOMEM;
    if (hooks) {
        hooks->busy = handler;
        hooks->busyArgument = argument;
        result = sqlite3_busy_handler(db, CallBusy, hooks);
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return result;
}

int APS5_VABI sqlite3_busy_timeout_nid_postfix(sqlite3* db, int milliseconds) {
    return sqlite3_busy_timeout(db, milliseconds);
}

int APS5_VABI sqlite3_clear_bindings_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_clear_bindings(statement);
}

int APS5_VABI sqlite3_close_nid_postfix(sqlite3* db) {
    return sqlite3_close(db);
}

const void* APS5_VABI sqlite3_column_blob_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_blob(statement, column);
}

int APS5_VABI sqlite3_column_bytes_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_bytes(statement, column);
}

int APS5_VABI sqlite3_column_bytes16_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_bytes16(statement, column);
}

const char* APS5_VABI sqlite3_column_decltype_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_decltype(statement, column);
}

double APS5_VABI sqlite3_column_double_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_double(statement, column);
}

int APS5_VABI sqlite3_column_int_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_int(statement, column);
}

std::int64_t APS5_VABI sqlite3_column_int64_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_int64(statement, column);
}

const void* APS5_VABI sqlite3_column_name16_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_name16(statement, column);
}

const void* APS5_VABI sqlite3_column_text16_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_text16(statement, column);
}

int APS5_VABI sqlite3_column_type_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_type(statement, column);
}

sqlite3_value* APS5_VABI sqlite3_column_value_nid_postfix(sqlite3_stmt* statement, int column) {
    return sqlite3_column_value(statement, column);
}

int APS5_VABI sqlite3_create_collation_v2_nid_postfix(sqlite3* db, const char* name, int encoding, void* argument,
    NKWebKit::GuestSqliteCompare compare, NKWebKit::GuestSqliteDestructor destroy) {
    auto* collation = new (std::nothrow) GuestCollation{compare, destroy, argument};
    if (!collation) {
        return SQLITE_NOMEM;
    }
    const int result = sqlite3_create_collation_v2(db, name, encoding, collation, compare ? CallCompare : nullptr,
        DestroyCollation);
    if (result != SQLITE_OK) {
        delete collation;
    }
    return result;
}

int APS5_VABI sqlite3_create_function_nid_postfix(sqlite3* db, const char* name, int arguments, int encoding,
    void* application, NKWebKit::GuestSqliteFunction function, NKWebKit::GuestSqliteFunction step,
    NKWebKit::GuestSqliteFinal final) {
    auto* guest = new (std::nothrow) GuestFunction{function, step, final, application};
    if (!guest) {
        return SQLITE_NOMEM;
    }
    return sqlite3_create_function_v2(db, name, arguments, encoding, guest, function ? CallFunction : nullptr,
        step ? CallStep : nullptr, final ? CallFinal : nullptr, DestroyFunction);
}

int APS5_VABI sqlite3_data_count_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_data_count(statement);
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

int APS5_VABI sqlite3_finalize_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_finalize(statement);
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

std::int64_t APS5_VABI sqlite3_last_insert_rowid_nid_postfix(sqlite3* db) {
    return sqlite3_last_insert_rowid(db);
}

sqlite3_stmt* APS5_VABI sqlite3_next_stmt_nid_postfix(sqlite3* db, sqlite3_stmt* statement) {
    return sqlite3_next_stmt(db, statement);
}

static bool MemoryUri(const char* filename) {
    const char* path = filename + 5;
    if (std::strchr(path, '%') != nullptr) return false;
    const char* end = path + std::strcspn(path, "?#");
    if (std::string_view(path, static_cast<std::size_t>(end - path)) == ":memory:") return true;
    if (*end != '?') return false;
    bool memory = false;
    for (const char* parameter = end + 1; *parameter != '\0' && *parameter != '#';) {
        const char* stop = parameter + std::strcspn(parameter, "&#");
        const std::string_view text(parameter, static_cast<std::size_t>(stop - parameter));
        if (text.substr(0, 5) == "mode=") memory = text.substr(5) == "memory";
        parameter = *stop == '&' ? stop + 1 : stop;
    }
    return memory;
}

static int GuardAttach(int result, sqlite3** db) {
    if (result != SQLITE_OK || db == nullptr || *db == nullptr) return result;
    sqlite3_mutex_enter(sqlite3_db_mutex(*db));
    GuestHooks* hooks = Hooks(*db);
    const int installed = hooks ? sqlite3_set_authorizer(*db, CallAuthorizer, hooks) : SQLITE_NOMEM;
    sqlite3_mutex_leave(sqlite3_db_mutex(*db));
    if (installed == SQLITE_OK) return SQLITE_OK;
    sqlite3_close(*db);
    *db = nullptr;
    return installed;
}

int APS5_VABI sqlite3_open_v2_nid_postfix(const char* filename, sqlite3** db, int flags, const char* vfs) {
    static_cast<void>(vfs);
    const bool uri = filename && (flags & SQLITE_OPEN_URI) && std::strncmp(filename, "file:", 5) == 0;
    const bool memory = uri ? MemoryUri(filename) :
        (!filename || !*filename || std::strcmp(filename, ":memory:") == 0 || (flags & SQLITE_OPEN_MEMORY));
    if (memory) {
        return GuardAttach(sqlite3_open_v2(filename, db, flags, nullptr), db);
    }
    if (uri) {
        NotImplemented_nid_no_patch("sqlite3_open_v2 file URI");
    }
    const std::filesystem::path host = ResolvePath_nid_no_patch(filename);
    const int result = GuardAttach(sqlite3_open_v2(reinterpret_cast<const char*>(host.u8string().c_str()), db, flags, nullptr), db);
    if (result == SQLITE_OK && (flags & SQLITE_OPEN_READWRITE)) {
        for (const char* suffix : {"", "-wal", "-shm", "-journal"}) {
            std::filesystem::path file = host;
            file += suffix;
            RecordWrittenPath_nid_no_patch(file);
        }
    }
    return result;
}

int APS5_VABI sqlite3_prepare_v2_nid_postfix(sqlite3* db, const char* sql, int size, sqlite3_stmt** statement,
    const char** tail) {
    return sqlite3_prepare_v2(db, sql, size, statement, tail);
}

int APS5_VABI sqlite3_reset_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_reset(statement);
}

void APS5_VABI sqlite3_result_error_nid_postfix(sqlite3_context* context, const char* message, int size) {
    sqlite3_result_error(context, message, size);
}

int APS5_VABI sqlite3_set_authorizer_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteAuthorizer authorizer,
    void* argument) {
    if (!db) {
        return sqlite3_set_authorizer(db, nullptr, nullptr);
    }
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    GuestHooks* hooks = Hooks(db);
    int result = SQLITE_NOMEM;
    if (hooks) {
        hooks->authorizer = authorizer;
        hooks->authorizerArgument = argument;
        result = sqlite3_set_authorizer(db, CallAuthorizer, hooks);
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return result;
}

const char* APS5_VABI sqlite3_sql_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_sql(statement);
}

int APS5_VABI sqlite3_step_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_step(statement);
}

int APS5_VABI sqlite3_stmt_busy_nid_postfix(sqlite3_stmt* statement) {
    return sqlite3_stmt_busy(statement);
}

int APS5_VABI sqlite3_total_changes_nid_postfix(sqlite3* db) {
    return sqlite3_total_changes(db);
}

void* APS5_VABI sqlite3_user_data_nid_postfix(sqlite3_context* context) {
    return static_cast<GuestFunction*>(sqlite3_user_data(context))->application;
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

int APS5_VABI sqlite3_wal_checkpoint_v2_nid_postfix(sqlite3* db, const char* schema, int mode, int* logFrames,
    int* checkpointedFrames) {
    return sqlite3_wal_checkpoint_v2(db, schema, mode, logFrames, checkpointedFrames);
}

void* APS5_VABI sqlite3_wal_hook_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteWalHook hook, void* argument) {
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    GuestHooks* hooks = Hooks(db);
    void* previous = nullptr;
    if (hooks) {
        void* host = sqlite3_wal_hook(db, hook ? CallWalHook : nullptr, hook ? hooks : nullptr);
        previous = host == hooks ? hooks->walArgument : host;
        hooks->walHook = hook;
        hooks->walArgument = argument;
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return previous;
}

}
