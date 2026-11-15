#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/SQLiteAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI _ZN7WebCore14SQLiteDatabaseC1Ev(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
void APS5_VABI _ZN7WebCore14SQLiteDatabaseC2Ev(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
void APS5_VABI _ZN7WebCore14SQLiteDatabaseD1Ev(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
void APS5_VABI _ZN7WebCore14SQLiteDatabaseD2Ev(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
bool APS5_VABI _ZN7WebCore14SQLiteDatabase4openERKN3WTF6StringENS0_8OpenModeE(NKWebKit::WebCoreAbi::SQLiteDatabase* self, const NKWebKit::WtfAbi::String* filename, int openMode);
void APS5_VABI _ZN7WebCore14SQLiteDatabase5closeEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
bool APS5_VABI _ZN7WebCore14SQLiteDatabase14executeCommandERKN3WTF6StringE(NKWebKit::WebCoreAbi::SQLiteDatabase* self, const NKWebKit::WtfAbi::String* sql);
bool APS5_VABI _ZN7WebCore14SQLiteDatabase11tableExistsERKN3WTF6StringE(NKWebKit::WebCoreAbi::SQLiteDatabase* self, const NKWebKit::WtfAbi::String* tableName);
void APS5_VABI _ZN7WebCore14SQLiteDatabase14clearAllTablesEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
int APS5_VABI _ZN7WebCore14SQLiteDatabase16runVacuumCommandEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
int APS5_VABI _ZN7WebCore14SQLiteDatabase27runIncrementalVacuumCommandEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
void APS5_VABI _ZN7WebCore14SQLiteDatabase9interruptEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
int APS5_VABI _ZN7WebCore14SQLiteDatabase9lastErrorEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
const char* APS5_VABI _ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
bool APS5_VABI _ZN7WebCore14SQLiteDatabase27turnOnIncrementalAutoVacuumEv(NKWebKit::WebCoreAbi::SQLiteDatabase* self);
void APS5_VABI _ZN7WebCore14SQLiteDatabase20setCollationFunctionERKN3WTF6StringEONS1_8FunctionIFiiPKviS7_EEE(NKWebKit::WebCoreAbi::SQLiteDatabase* self, const NKWebKit::WtfAbi::String* collationName, NKWebKit::WebCoreAbi::CollationFunction* collationFunction);
void APS5_VABI _ZN7WebCore14SQLiteDatabase29setIsDatabaseOpeningForbiddenEb(bool isForbidden);

void APS5_VABI _ZN7WebCore15SQLiteStatementC1ERNS_14SQLiteDatabaseERKN3WTF6StringE(NKWebKit::WebCoreAbi::SQLiteStatement* self, NKWebKit::WebCoreAbi::SQLiteDatabase* db, const NKWebKit::WtfAbi::String* sql);
void APS5_VABI _ZN7WebCore15SQLiteStatementC2ERNS_14SQLiteDatabaseERKN3WTF6StringE(NKWebKit::WebCoreAbi::SQLiteStatement* self, NKWebKit::WebCoreAbi::SQLiteDatabase* db, const NKWebKit::WtfAbi::String* sql);
void APS5_VABI _ZN7WebCore15SQLiteStatementD1Ev(NKWebKit::WebCoreAbi::SQLiteStatement* self);
void APS5_VABI _ZN7WebCore15SQLiteStatementD2Ev(NKWebKit::WebCoreAbi::SQLiteStatement* self);
int APS5_VABI _ZN7WebCore15SQLiteStatement7prepareEv(NKWebKit::WebCoreAbi::SQLiteStatement* self);
int APS5_VABI _ZN7WebCore15SQLiteStatement4stepEv(NKWebKit::WebCoreAbi::SQLiteStatement* self);
int APS5_VABI _ZN7WebCore15SQLiteStatement8finalizeEv(NKWebKit::WebCoreAbi::SQLiteStatement* self);
int APS5_VABI _ZN7WebCore15SQLiteStatement5resetEv(NKWebKit::WebCoreAbi::SQLiteStatement* self);
bool APS5_VABI _ZN7WebCore15SQLiteStatement14executeCommandEv(NKWebKit::WebCoreAbi::SQLiteStatement* self);
int APS5_VABI _ZN7WebCore15SQLiteStatement8bindBlobEiPKvi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, const void* blob, int size);
int APS5_VABI _ZN7WebCore15SQLiteStatement8bindBlobEiRKN3WTF6StringE(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, const NKWebKit::WtfAbi::String* text);
int APS5_VABI _ZN7WebCore15SQLiteStatement8bindTextEiRKN3WTF6StringE(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, const NKWebKit::WtfAbi::String* text);
int APS5_VABI _ZN7WebCore15SQLiteStatement7bindIntEii(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, int integer);
int APS5_VABI _ZN7WebCore15SQLiteStatement9bindInt64Eil(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, std::int64_t integer);
int APS5_VABI _ZN7WebCore15SQLiteStatement10bindDoubleEid(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, double number);
int APS5_VABI _ZN7WebCore15SQLiteStatement8bindNullEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index);
int APS5_VABI _ZN7WebCore15SQLiteStatement9bindValueEiRKN3WTF7VariantIJDnNS1_6StringEdEEE(NKWebKit::WebCoreAbi::SQLiteStatement* self, int index, const NKWebKit::WebCoreAbi::SQLValue* value);
unsigned APS5_VABI _ZNK7WebCore15SQLiteStatement18bindParameterCountEv(const NKWebKit::WebCoreAbi::SQLiteStatement* self);
bool APS5_VABI _ZN7WebCore15SQLiteStatement12isColumnNullEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
bool APS5_VABI _ZN7WebCore15SQLiteStatement22isColumnDeclaredAsBlobEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore15SQLiteStatement13getColumnTextEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
double APS5_VABI _ZN7WebCore15SQLiteStatement15getColumnDoubleEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
int APS5_VABI _ZN7WebCore15SQLiteStatement12getColumnIntEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
std::int64_t APS5_VABI _ZN7WebCore15SQLiteStatement14getColumnInt64Ei(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore15SQLiteStatement21getColumnBlobAsStringEi(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col);
void APS5_VABI _ZN7WebCore15SQLiteStatement21getColumnBlobAsVectorEiRN3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col, NKWebKit::WtfAbi::Vector<char>* result);
void APS5_VABI _ZN7WebCore15SQLiteStatement21getColumnBlobAsVectorEiRN3WTF6VectorIhLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::SQLiteStatement* self, int col, NKWebKit::WtfAbi::Vector<unsigned char>* result);

void APS5_VABI _ZN7WebCore17SQLiteTransactionC1ERNS_14SQLiteDatabaseEb(NKWebKit::WebCoreAbi::SQLiteTransaction* self, NKWebKit::WebCoreAbi::SQLiteDatabase* db, bool readOnly);
void APS5_VABI _ZN7WebCore17SQLiteTransactionC2ERNS_14SQLiteDatabaseEb(NKWebKit::WebCoreAbi::SQLiteTransaction* self, NKWebKit::WebCoreAbi::SQLiteDatabase* db, bool readOnly);
void APS5_VABI _ZN7WebCore17SQLiteTransactionD1Ev(NKWebKit::WebCoreAbi::SQLiteTransaction* self);
void APS5_VABI _ZN7WebCore17SQLiteTransactionD2Ev(NKWebKit::WebCoreAbi::SQLiteTransaction* self);
void APS5_VABI _ZN7WebCore17SQLiteTransaction5beginEv(NKWebKit::WebCoreAbi::SQLiteTransaction* self);
void APS5_VABI _ZN7WebCore17SQLiteTransaction6commitEv(NKWebKit::WebCoreAbi::SQLiteTransaction* self);
void APS5_VABI _ZN7WebCore17SQLiteTransaction8rollbackEv(NKWebKit::WebCoreAbi::SQLiteTransaction* self);

void APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeC1EOS0_(NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* self, NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* other);
void APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeC2EOS0_(NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* self, NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* other);
NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope& APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeaSEOS0_(NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* self, NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* other);
void APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeD1Ev(NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* self);
void APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeD2Ev(NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope* self);

}
