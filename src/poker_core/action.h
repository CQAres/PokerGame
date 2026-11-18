#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace poker::core {

enum class ActionKind : std::uint8_t {
    Fold = 0,
    Check,
    Call,
    BetTo,
    RaiseTo,
    AllIn,
};

const char* ActionKindName(ActionKind kind) noexcept;

struct Action {
    ActionKind kind = ActionKind::Fold;
    // BetTo / RaiseTo use this as the target committed_round for this round.
    // Fold / Check / Call / AllIn ignore it.
    std::int64_t target_total = 0;
};

struct ActionCandidate {
    std::string action_id;
    ActionKind kind = ActionKind::Fold;
    std::int64_t amount = 0;       // chips added by this action
    std::int64_t target_total = 0; // committed_round after this action
    std::int64_t min_raise_to = 0;
    std::int64_t max_raise_to = 0;
    bool is_all_in = false;
    std::string label;
};

struct LegalActions {
    std::int64_t to_call = 0;
    std::int64_t min_raise_to = 0;
    std::int64_t max_raise_to = 0;
    bool can_check = false;
    bool can_call = false;
    bool can_raise = false;
    std::vector<ActionCandidate> candidates;

    const ActionCandidate* Find(const std::string& action_id) const;
};

} // namespace poker::core
