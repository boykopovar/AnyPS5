// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/RunLoop.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/FunctionDispatcher.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/ThreadSafeRefCounted.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007, 2008, 2010, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfFunctionAbi.hpp"
#include <atomic>
#include <cstddef>

namespace NKWebKit::WtfAbi {

struct RunLoop;
struct RunLoopImpl;

struct RunLoopVtable {
    void (APS5_VABI* completeDestructor)(RunLoop* self);
    void (APS5_VABI* deletingDestructor)(RunLoop* self);
    void (APS5_VABI* dispatch)(RunLoop* self, VoidFunction* function);
};

struct RunLoop {
    const RunLoopVtable* vtable;
    std::atomic<unsigned> refCount;
    RunLoopImpl* impl;
};

struct RunLoopTimerBase;
struct RunLoopScheduledTask;

struct RunLoopTimerBaseVtable {
    void (APS5_VABI* completeDestructor)(RunLoopTimerBase* self);
    void (APS5_VABI* deletingDestructor)(RunLoopTimerBase* self);
    void (APS5_VABI* fired)(RunLoopTimerBase* self);
};

struct RunLoopTimerBase {
    const RunLoopTimerBaseVtable* vtable;
    RunLoop* runLoop;
    RunLoopScheduledTask* scheduledTask;
};

struct RunLoopDispatchTimer {
    RunLoopTimerBase timerBase;
    std::atomic<unsigned> refCount;
    VoidFunction function;
};

struct RunLoopDispatchTimerRef {
    RunLoopDispatchTimer* ptr;
};

enum class RunLoopCycleResult : int {
    Continue,
    Stop,
};

static_assert(sizeof(std::atomic<unsigned>) == 4);
static_assert(sizeof(RunLoopVtable) == 24);
static_assert(offsetof(RunLoopVtable, completeDestructor) == 0);
static_assert(offsetof(RunLoopVtable, deletingDestructor) == 8);
static_assert(offsetof(RunLoopVtable, dispatch) == 16);
static_assert(offsetof(RunLoop, vtable) == 0);
static_assert(offsetof(RunLoop, refCount) == 8);
static_assert(offsetof(RunLoop, impl) == 16);
static_assert(sizeof(RunLoopTimerBaseVtable) == 24);
static_assert(offsetof(RunLoopTimerBaseVtable, completeDestructor) == 0);
static_assert(offsetof(RunLoopTimerBaseVtable, deletingDestructor) == 8);
static_assert(offsetof(RunLoopTimerBaseVtable, fired) == 16);
static_assert(sizeof(RunLoopTimerBase) == 24);
static_assert(offsetof(RunLoopTimerBase, vtable) == 0);
static_assert(offsetof(RunLoopTimerBase, runLoop) == 8);
static_assert(offsetof(RunLoopTimerBase, scheduledTask) == 16);
static_assert(sizeof(RunLoopDispatchTimer) == 40);
static_assert(offsetof(RunLoopDispatchTimer, timerBase) == 0);
static_assert(offsetof(RunLoopDispatchTimer, refCount) == 24);
static_assert(offsetof(RunLoopDispatchTimer, function) == 32);
static_assert(sizeof(RunLoopDispatchTimerRef) == 8);
static_assert(sizeof(RunLoopCycleResult) == 4);

void InitializeMainRunLoop();
bool IsMainRunLoop();

}  // namespace NKWebKit::WtfAbi
