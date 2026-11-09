#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

namespace NKWebKit {

class FormSubmissionListener final : public ObjectOf<ObjectType::FormSubmissionListener> {
public:
    bool continued = false;
};

class FramePolicyListener final : public ObjectOf<ObjectType::FramePolicyListener> {
public:
    enum class Policy {
        None,
        Use,
        Download,
        Ignore,
        UseInNewProcess,
    };

    Policy policy = Policy::None;
    Retained<Object> websitePolicies;
};

class MessageListener final : public ObjectOf<ObjectType::MessageListener> {
public:
    bool replied = false;
    Retained<Object> reply;
};

class RequestStorageAccessConfirmResultListener final : public ObjectOf<ObjectType::RequestStorageAccessConfirmResultListener> {
public:
    bool called = false;
    bool allowed = false;
};

class RunBeforeUnloadConfirmPanelResultListener final : public ObjectOf<ObjectType::RunBeforeUnloadConfirmPanelResultListener> {
public:
    bool called = false;
    bool result = false;
};

class RunJavaScriptAlertResultListener final : public ObjectOf<ObjectType::RunJavaScriptAlertResultListener> {
public:
    bool called = false;
};

class RunJavaScriptConfirmResultListener final : public ObjectOf<ObjectType::RunJavaScriptConfirmResultListener> {
public:
    bool called = false;
    bool result = false;
};

class RunJavaScriptPromptResultListener final : public ObjectOf<ObjectType::RunJavaScriptPromptResultListener> {
public:
    bool called = false;
    Retained<String> result;
};

}  // namespace NKWebKit
