#pragma once

#include <cassert>
#include <print>
#include <utility>

#include "Util.hpp"

namespace Os
{

enum class EventKind : std::uint8_t
{
    Cpu = 0,
    Io,
    Count,
};

[[nodiscard]] constexpr static auto event_kind_to_str(EventKind event_kind) -> std::string
{
    static_assert(
      std::to_underlying(EventKind::Count) == 2,
      "[ERROR] Exhaustive handling of all enum variants for EventKind is required"
    );

    switch (event_kind) {
        case EventKind::Cpu: {
            return "cpu";
        }
        case EventKind::Io: {
            return "io";
        }
        default: {
            assert(false && "unreachable");
        }
    }
}

[[nodiscard]] constexpr static auto event_kind_try_from_str(std::string_view str) -> std::optional<EventKind>
{
    static_assert(
      std::to_underlying(EventKind::Count) == 2,
      "[ERROR] Exhaustive handling of all enum variants for EventKind is required"
    );

    if (Util::to_lower(str) == "cpu") {
        return EventKind::Cpu;
    } else if (Util::to_lower(str) == "io") {
        return EventKind::Io;
    }

    std::println("[ERROR] Unknown event kind: {}", str);
    return std::nullopt;
}

struct [[nodiscard]] Event final
{
    EventKind   kind;
    std::size_t duration;
};

} // namespace Os
