#include "poker_core/action.h"

namespace poker::core {

const char* ActionKindName(ActionKind kind) noexcept {
    switch (kind) {
    case ActionKind::Fold:
        return "fold";
    case ActionKind::Check:
        return "check";
    case ActionKind::Call:
        return "call";
    case ActionKind::BetTo:
        return "bet_to";
    case ActionKind::RaiseTo:
        return "raise_to";
    case ActionKind::AllIn:
        return "all_in";
    }
    return "unknown";
}

const ActionCandidate* LegalActions::Find(const std::string& action_id) const {
    for (const ActionCandidate& candidate : candidates) {
        if (candidate.action_id == action_id) {
            return &candidate;
        }
    }
    return nullptr;
}

} // namespace poker::core
