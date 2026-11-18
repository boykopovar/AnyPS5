#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/SQLiteDatabaseTrackerAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE(NKWebKit::SQLiteAbi::SQLiteDatabaseTrackerClient* client);
void APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker35incrementTransactionInProgressCountEv();
void APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker35decrementTransactionInProgressCountEv();
bool APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker24hasTransactionInProgressEv();

}
