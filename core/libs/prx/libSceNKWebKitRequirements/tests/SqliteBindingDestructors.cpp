#include "prx/libSceNKWebKitRequirements/include/Sqlite.hpp"

#include <sqlite3.h>

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <vector>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "SQLite binding destructor assertion failed: %s\n", message);
    std::abort();
}

template<class TNative, class TGuest, class... TArgs>
auto Call(bool guest, TNative native, TGuest adapter, TArgs... arguments) {
    return guest ? adapter(arguments...) : native(arguments...);
}

struct Trace {
    std::vector<int> events;
    std::array<const void*, 3> buffers;

    void Record(int callback, void* pointer) {
        for (std::size_t i = 0; i < buffers.size(); ++i) {
            if (pointer == buffers[i]) {
                events.push_back(callback * 10 + static_cast<int>(i));
                return;
            }
        }
        Require(false, "destructor receives the original argument");
    }

    void Result(int result) {
        events.push_back(-1);
        events.push_back(result);
    }
};

thread_local Trace* activeTrace = nullptr;

void First(void* pointer) { activeTrace->Record(1, pointer); }
void Second(void* pointer) { activeTrace->Record(2, pointer); }
void APS5_VABI GuestFirst(void* pointer) { First(pointer); }
void APS5_VABI GuestSecond(void* pointer) { Second(pointer); }

struct Database {
    bool guest;
    sqlite3* db = nullptr;

    explicit Database(bool useGuest, const char* encoding) : guest(useGuest) {
        Require(Call(guest, sqlite3_open_v2, sqlite3_open_v2_nid_postfix, ":memory:", &db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, static_cast<const char*>(nullptr)) == SQLITE_OK, "open");
        sqlite3_stmt* statement = Prepare(encoding);
        Require(Step(statement) == SQLITE_DONE, "set encoding");
        Require(Finalize(statement) == SQLITE_OK, "finalize encoding");
    }

    ~Database() {
        Require(Call(guest, sqlite3_close, sqlite3_close_nid_postfix, db) == SQLITE_OK, "close");
    }

    sqlite3_stmt* Prepare(const char* sql = "SELECT ?1") {
        sqlite3_stmt* statement = nullptr;
        Require(Call(guest, sqlite3_prepare_v2, sqlite3_prepare_v2_nid_postfix, db, sql, -1, &statement,
            static_cast<const char**>(nullptr)) == SQLITE_OK, "prepare");
        return statement;
    }

    int Bind(sqlite3_stmt* statement, int index, const void* data, int size, bool text, int callback) {
        if (guest) {
            auto destructor = callback == 1 ? GuestFirst : GuestSecond;
            if (callback <= 0) destructor = reinterpret_cast<NKWebKit::GuestSqliteDestructor>(
                static_cast<std::intptr_t>(callback));
            return text ? sqlite3_bind_text16_nid_postfix(statement, index, data, size, destructor) :
                sqlite3_bind_blob_nid_postfix(statement, index, data, size, destructor);
        }
        sqlite3_destructor_type destructor = callback == 1 ? First : Second;
        if (callback <= 0) destructor = reinterpret_cast<sqlite3_destructor_type>(static_cast<std::intptr_t>(callback));
        return text ? sqlite3_bind_text16(statement, index, data, size, destructor) :
            sqlite3_bind_blob(statement, index, data, size, destructor);
    }

    int Step(sqlite3_stmt* statement) { return Call(guest, sqlite3_step, sqlite3_step_nid_postfix, statement); }
    int Reset(sqlite3_stmt* statement) { return Call(guest, sqlite3_reset, sqlite3_reset_nid_postfix, statement); }
    int Clear(sqlite3_stmt* statement) {
        return Call(guest, sqlite3_clear_bindings, sqlite3_clear_bindings_nid_postfix, statement);
    }
    int Finalize(sqlite3_stmt* statement) {
        return Call(guest, sqlite3_finalize, sqlite3_finalize_nid_postfix, statement);
    }

    void Read(sqlite3_stmt* statement) {
        Require(Step(statement) == SQLITE_ROW, "read bound value");
        const auto* data = static_cast<const unsigned char*>(Call(guest, sqlite3_column_blob,
            sqlite3_column_blob_nid_postfix, statement, 0));
        const int size = Call(guest, sqlite3_column_bytes, sqlite3_column_bytes_nid_postfix, statement, 0);
        activeTrace->Result(size);
        for (int i = 0; i < size; ++i) activeTrace->Result(data[i]);
        Call(guest, sqlite3_column_text16, sqlite3_column_text16_nid_postfix, statement, 0);
        activeTrace->Result(Reset(statement));
    }
};

std::vector<int> Run(bool guest, std::string_view mode) {
    alignas(8) std::array<unsigned char, 64> first{};
    alignas(8) std::array<unsigned char, 64> second{};
    std::memcpy(first.data(), u"hé€", 8);
    std::memcpy(second.data(), u"other", 12);
    Trace trace{{}, {first.data(), second.data(), nullptr}};
    activeTrace = &trace;
    {
        const char* encoding = mode == "conversion" ? "PRAGMA encoding='UTF-8'" : "PRAGMA encoding='UTF-16le'";
        Database database(guest, encoding);
        if (mode == "errors") {
            for (bool text : {false, true}) {
                for (int index : {0, 2, INT_MIN + 1, INT_MAX}) {
                    for (const void* pointer : {static_cast<const void*>(first.data()), static_cast<const void*>(nullptr)}) {
                        sqlite3_stmt* statement = database.Prepare();
                        trace.Result(database.Bind(statement, index, pointer, 8, text, 1));
                        trace.Result(database.Finalize(statement));
                    }
                }
                trace.Result(database.Bind(nullptr, 1, first.data(), 8, text, 2));
                trace.Result(database.Bind(nullptr, 1, nullptr, 8, text, 1));
                sqlite3_stmt* statement = database.Prepare();
                trace.Result(database.Bind(statement, 1, first.data(), 8, text, 1));
                trace.Result(database.Step(statement));
                trace.Result(database.Bind(statement, 1, second.data(), 8, text, 2));
                trace.Result(database.Reset(statement));
                trace.Result(database.Finalize(statement));
                statement = database.Prepare();
                const int limit = sqlite3_limit(database.db, SQLITE_LIMIT_LENGTH, 4);
                trace.Result(database.Bind(statement, 1, first.data(), 8, text, 1));
                sqlite3_limit(database.db, SQLITE_LIMIT_LENGTH, limit);
                trace.Result(database.Finalize(statement));
            }
            sqlite3_stmt* statement = database.Prepare();
            trace.Result(database.Bind(statement, 2, first.data(), -1, true, 1));
            trace.Result(database.Finalize(statement));
        } else {
            for (bool text : {false, true}) {
                for (int size : {0, 6, 8}) {
                    sqlite3_stmt* statement = database.Prepare();
                    trace.Result(database.Bind(statement, 1, first.data(), size, text, 1));
                    database.Read(statement);
                    trace.Result(database.Clear(statement));
                    trace.Result(database.Finalize(statement));
                }
                sqlite3_stmt* statement = database.Prepare();
                trace.Result(database.Bind(statement, 1, first.data(), 6, text, 1));
                trace.Result(database.Bind(statement, 1, second.data(), 8, text, 2));
                trace.Result(Call(guest, sqlite3_bind_int, sqlite3_bind_int_nid_postfix, statement, 1, 42));
                trace.Result(database.Bind(statement, 1, first.data(), 6, text, 2));
                trace.Result(Call(guest, sqlite3_bind_null, sqlite3_bind_null_nid_postfix, statement, 1));
                trace.Result(database.Bind(statement, 1, first.data(), 6, text, 1));
                trace.Result(database.Bind(statement, 1, nullptr, 8, text, 2));
                trace.Result(database.Finalize(statement));
                for (int callback : {0, -1}) {
                    statement = database.Prepare();
                    trace.Result(database.Bind(statement, 1, first.data(), 6, text, callback));
                    if (callback == -1) first[0] ^= 0x20;
                    database.Read(statement);
                    trace.Result(database.Finalize(statement));
                    if (callback == -1) first[0] ^= 0x20;
                }
            }
            for (int size : {-1, -2, INT_MIN}) {
                sqlite3_stmt* statement = database.Prepare();
                trace.Result(database.Bind(statement, 1, first.data(), size, true, 1));
                database.Read(statement);
                trace.Result(database.Finalize(statement));
            }
            sqlite3_stmt* firstStatement = database.Prepare();
            sqlite3_stmt* secondStatement = database.Prepare();
            trace.Result(database.Bind(firstStatement, 1, first.data(), 8, false, 1));
            trace.Result(database.Bind(secondStatement, 1, first.data(), 8, false, 2));
            trace.Result(database.Finalize(secondStatement));
            trace.Result(database.Finalize(firstStatement));
        }
    }
    activeTrace = nullptr;
    return trace.events;
}

void Check(std::string_view mode) {
    const auto native = Run(false, mode);
    const auto guest = Run(true, mode);
    if (native == guest) return;
    std::size_t index = 0;
    while (index < native.size() && index < guest.size() && native[index] == guest[index]) ++index;
    std::fprintf(stderr, "mode %.*s differs at event %zu: native=%d guest=%d, counts=%zu/%zu\n",
        static_cast<int>(mode.size()), mode.data(), index, index < native.size() ? native[index] : INT_MIN,
        index < guest.size() ? guest[index] : INT_MIN, native.size(), guest.size());
    Require(false, "guest result and destructor trace matches bundled SQLite");
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc == 2 ? argv[1] : "lifetime";
    Require(mode == "lifetime" || mode == "errors" || mode == "conversion" || mode == "threads", "mode");
    if (mode == "threads") {
        std::array<std::thread, 8> threads;
        for (auto& thread : threads) thread = std::thread([] { Check("lifetime"); Check("errors"); });
        for (auto& thread : threads) thread.join();
    } else {
        Check(mode);
    }
    return 0;
}
