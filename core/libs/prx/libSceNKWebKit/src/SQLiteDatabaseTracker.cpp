// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/sql/SQLiteDatabaseTracker.cpp
/*
 * Copyright (C) 2010, 2012 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/sql/SQLiteDatabaseTrackerClient.h
/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/SQLiteDatabaseTrackerExports.hpp"

#include <mutex>
#include <stdexcept>

namespace {

using NKWebKit::SQLiteAbi::SQLiteDatabaseTrackerClient;

std::mutex transactionInProgressLock;
SQLiteDatabaseTrackerClient* staticSQLiteDatabaseTrackerClient = nullptr;
unsigned transactionInProgressCounter = 0;

void CallClient(SQLiteDatabaseTrackerClient* client, unsigned slot) {
    using Function = void (APS5_VABI*)(SQLiteDatabaseTrackerClient*);
    reinterpret_cast<Function>(const_cast<void*>(client->vtable[slot]))(client);
}

}

extern "C" void APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE(SQLiteDatabaseTrackerClient* client) {
    std::lock_guard<std::mutex> locker(transactionInProgressLock);
    staticSQLiteDatabaseTrackerClient = client;
}

extern "C" void APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker35incrementTransactionInProgressCountEv() {
    std::lock_guard<std::mutex> locker(transactionInProgressLock);
    if (!staticSQLiteDatabaseTrackerClient) return;
    ++transactionInProgressCounter;
    if (transactionInProgressCounter == 1) CallClient(staticSQLiteDatabaseTrackerClient, NKWebKit::SQLiteAbi::WillBeginFirstTransactionSlot);
}

extern "C" void APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker35decrementTransactionInProgressCountEv() {
    std::lock_guard<std::mutex> locker(transactionInProgressLock);
    if (!staticSQLiteDatabaseTrackerClient) return;
    if (!transactionInProgressCounter) throw std::logic_error("WebCore::SQLiteDatabaseTracker transaction count decremented below zero");
    --transactionInProgressCounter;
    if (!transactionInProgressCounter) CallClient(staticSQLiteDatabaseTrackerClient, NKWebKit::SQLiteAbi::DidFinishLastTransactionSlot);
}

extern "C" bool APS5_VABI _ZN7WebCore21SQLiteDatabaseTracker24hasTransactionInProgressEv() {
    std::lock_guard<std::mutex> locker(transactionInProgressLock);
    return !staticSQLiteDatabaseTrackerClient || transactionInProgressCounter > 0;
}
