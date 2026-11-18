#include "poker_core/player.h"

namespace poker::core {

const char* PlayerTypeName(PlayerType type) noexcept {
    switch (type) {
    case PlayerType::Human:
        return "human";
    case PlayerType::Rule:
        return "rule";
    case PlayerType::Llm:
        return "llm";
    case PlayerType::Random:
        return "random";
    }
    return "unknown";
}

} // namespace poker::core
