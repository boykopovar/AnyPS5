#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_value;
struct sqlite3_context;

namespace NKWebKit {
using GuestSqliteDestructor = void (APS5_VABI *)(void*);
using GuestSqliteBusy = int (APS5_VABI *)(void*, int);
using GuestSqliteCompare = int (APS5_VABI *)(void*, int, const void*, int, const void*);
using GuestSqliteFunction = void (APS5_VABI *)(sqlite3_context*, int, sqlite3_value**);
using GuestSqliteFinal = void (APS5_VABI *)(sqlite3_context*);
using GuestSqliteAuthorizer = int (APS5_VABI *)(void*, int, const char*, const char*, const char*, const char*);
using GuestSqliteWalHook = int (APS5_VABI *)(void*, sqlite3*, const char*, int);
}

extern "C" {
int APS5_VABI sqlite3_bind_blob_nid_postfix(sqlite3_stmt* statement, int index, const void* data, int size,
    NKWebKit::GuestSqliteDestructor destructor);
int APS5_VABI sqlite3_bind_double_nid_postfix(sqlite3_stmt* statement, int index, double value);
int APS5_VABI sqlite3_bind_int_nid_postfix(sqlite3_stmt* statement, int index, int value);
int APS5_VABI sqlite3_bind_int64_nid_postfix(sqlite3_stmt* statement, int index, std::int64_t value);
int APS5_VABI sqlite3_bind_null_nid_postfix(sqlite3_stmt* statement, int index);
int APS5_VABI sqlite3_bind_parameter_count_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_bind_text16_nid_postfix(sqlite3_stmt* statement, int index, const void* text, int size,
    NKWebKit::GuestSqliteDestructor destructor);
int APS5_VABI sqlite3_busy_handler_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteBusy handler, void* argument);
int APS5_VABI sqlite3_busy_timeout_nid_postfix(sqlite3* db, int milliseconds);
int APS5_VABI sqlite3_clear_bindings_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_close_nid_postfix(sqlite3* db);
const void* APS5_VABI sqlite3_column_blob_nid_postfix(sqlite3_stmt* statement, int column);
int APS5_VABI sqlite3_column_bytes_nid_postfix(sqlite3_stmt* statement, int column);
int APS5_VABI sqlite3_column_bytes16_nid_postfix(sqlite3_stmt* statement, int column);
const char* APS5_VABI sqlite3_column_decltype_nid_postfix(sqlite3_stmt* statement, int column);
double APS5_VABI sqlite3_column_double_nid_postfix(sqlite3_stmt* statement, int column);
int APS5_VABI sqlite3_column_int_nid_postfix(sqlite3_stmt* statement, int column);
std::int64_t APS5_VABI sqlite3_column_int64_nid_postfix(sqlite3_stmt* statement, int column);
const void* APS5_VABI sqlite3_column_name16_nid_postfix(sqlite3_stmt* statement, int column);
const void* APS5_VABI sqlite3_column_text16_nid_postfix(sqlite3_stmt* statement, int column);
int APS5_VABI sqlite3_column_type_nid_postfix(sqlite3_stmt* statement, int column);
sqlite3_value* APS5_VABI sqlite3_column_value_nid_postfix(sqlite3_stmt* statement, int column);
int APS5_VABI sqlite3_create_collation_v2_nid_postfix(sqlite3* db, const char* name, int encoding, void* argument,
    NKWebKit::GuestSqliteCompare compare, NKWebKit::GuestSqliteDestructor destroy);
int APS5_VABI sqlite3_create_function_nid_postfix(sqlite3* db, const char* name, int arguments, int encoding,
    void* application, NKWebKit::GuestSqliteFunction function, NKWebKit::GuestSqliteFunction step,
    NKWebKit::GuestSqliteFinal final);
int APS5_VABI sqlite3_data_count_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_errcode_nid_postfix(sqlite3* db);
const char* APS5_VABI sqlite3_errmsg_nid_postfix(sqlite3* db);
const char* APS5_VABI sqlite3_errstr_nid_postfix(int code);
int APS5_VABI sqlite3_extended_result_codes_nid_postfix(sqlite3* db, int enabled);
int APS5_VABI sqlite3_finalize_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_get_autocommit_nid_postfix(sqlite3* db);
int APS5_VABI sqlite3_initialize_nid_postfix();
void APS5_VABI sqlite3_interrupt_nid_postfix(sqlite3* db);
std::int64_t APS5_VABI sqlite3_last_insert_rowid_nid_postfix(sqlite3* db);
sqlite3_stmt* APS5_VABI sqlite3_next_stmt_nid_postfix(sqlite3* db, sqlite3_stmt* statement);
int APS5_VABI sqlite3_open_v2_nid_postfix(const char* filename, sqlite3** db, int flags, const char* vfs);
int APS5_VABI sqlite3_prepare_v2_nid_postfix(sqlite3* db, const char* sql, int size, sqlite3_stmt** statement,
    const char** tail);
int APS5_VABI sqlite3_reset_nid_postfix(sqlite3_stmt* statement);
void APS5_VABI sqlite3_result_error_nid_postfix(sqlite3_context* context, const char* message, int size);
int APS5_VABI sqlite3_set_authorizer_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteAuthorizer authorizer,
    void* argument);
const char* APS5_VABI sqlite3_sql_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_step_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_stmt_busy_nid_postfix(sqlite3_stmt* statement);
int APS5_VABI sqlite3_total_changes_nid_postfix(sqlite3* db);
void* APS5_VABI sqlite3_user_data_nid_postfix(sqlite3_context* context);
double APS5_VABI sqlite3_value_double_nid_postfix(sqlite3_value* value);
const void* APS5_VABI sqlite3_value_text16_nid_postfix(sqlite3_value* value);
int APS5_VABI sqlite3_value_type_nid_postfix(sqlite3_value* value);
int APS5_VABI sqlite3_wal_checkpoint_v2_nid_postfix(sqlite3* db, const char* schema, int mode, int* logFrames,
    int* checkpointedFrames);
void* APS5_VABI sqlite3_wal_hook_nid_postfix(sqlite3* db, NKWebKit::GuestSqliteWalHook hook, void* argument);
}
