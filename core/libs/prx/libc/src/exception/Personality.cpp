#include "Runtime.hpp"

extern "C" _Unwind_Reason_Code __gxx_personality_v0_nid_postfix(
    int version, _Unwind_Action actions, std::uint64_t exceptionClass,
    _Unwind_Exception* exception, _Unwind_Context* context
) {
    using namespace LibcUnwind;
    using namespace LibcException;
    if (version != 1) return _URC_FATAL_PHASE1_ERROR;
    if (!context->lsda) return _URC_CONTINUE_UNWIND;
    const Byte* p = reinterpret_cast<const Byte*>(context->lsda);
    Byte landingEncoding = *p++;
    Word landingBase = landingEncoding == 255 ? context->region : Encoded(p, landingEncoding, context->dataBase, context->region, context->textBase);
    Byte typeEncoding = *p++;
    const Byte* typeTable = nullptr;
    if (typeEncoding != 255) { Word offset = Uleb(p); typeTable = p + offset; }
    Byte callEncoding = *p++;
    Word length = Uleb(p);
    const Byte* callEnd = p + length;
    const Byte* actionsBegin = callEnd;
    Word ip = context->registers[16] - !context->signalFrame;
    Header* header = Native(exceptionClass) ? FromUnwind(exception) : nullptr;
    while (p < callEnd) {
        Word start = Encoded(p, callEncoding);
        Word size = Encoded(p, callEncoding);
        Word landing = Encoded(p, callEncoding);
        Word action = Uleb(p);
        if (ip < context->region + start || ip - context->region - start >= size) continue;
        if (!landing) return _URC_CONTINUE_UNWIND;
        bool cleanup = !action, matched = false;
        std::intptr_t selector = 0;
        void* adjusted = header ? static_cast<void*>(Primary(header) + 1) : nullptr;
        if (header && std::strcmp(Kind(Primary(header)->type), "N10__cxxabiv119__pointer_type_infoE") == 0)
            std::memcpy(&adjusted, adjusted, sizeof(adjusted));
        if (action) {
            const Byte* record = actionsBegin + action - 1;
            for (unsigned steps = 0; steps < 4096; ++steps) {
                auto filter = Sleb(record);
                const Byte* nextField = record;
                auto next = Sleb(record);
                if (filter == 0) cleanup = true;
                else if (filter > 0 && !(actions & _UA_FORCE_UNWIND) && typeTable) {
                    const Byte* entry = typeTable - filter * EncodingSize(typeEncoding);
                    auto* caught = reinterpret_cast<const std::type_info*>(Encoded(entry, typeEncoding, context->dataBase, context->region, context->textBase));
                    void* candidate = adjusted;
                    if (!caught || (header && Match(caught, Primary(header)->type, candidate))) {
                        matched = true; selector = filter; adjusted = candidate; break;
                    }
                } else if (filter < 0) return (actions & _UA_SEARCH_PHASE) ? _URC_FATAL_PHASE1_ERROR : _URC_FATAL_PHASE2_ERROR;
                if (!next) break;
                record = nextField + next;
            }
        }
        if (actions & _UA_SEARCH_PHASE) {
            if (!matched) return _URC_CONTINUE_UNWIND;
            if (header) { header->adjusted = adjusted; header->selector = static_cast<int>(selector); }
            return _URC_HANDLER_FOUND;
        }
        if (actions & _UA_HANDLER_FRAME) {
            if (!matched) return _URC_FATAL_PHASE2_ERROR;
            if (header) header->adjusted = adjusted;
        } else {
            if (!cleanup) return _URC_CONTINUE_UNWIND;
            selector = 0;
        }
        context->registers[0] = reinterpret_cast<Word>(exception);
        context->registers[1] = selector;
        context->registers[16] = landingBase + landing;
        return _URC_INSTALL_CONTEXT;
    }
    return _URC_CONTINUE_UNWIND;
}
