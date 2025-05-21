#pragma once

#include <cassert>
#include <deque>
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

[[nodiscard]] constexpr static auto event_kind_try_from_str(std::string_view str) -> std::optional<EventKind>
{
    static_assert(
      std::to_underlying(EventKind::Count) == 2,
      "[ERROR] Exhaustive handling of all enum variants for EventKind is required"
    );

    auto lowered = Util::to_lower(str);
    if (lowered == "cpu") {
        return EventKind::Cpu;
    } else if (lowered == "io") {
        return EventKind::Io;
    }

    std::println("[ERROR] Unknown event kind: {}", str);
    return std::nullopt;
}

struct [[nodiscard]] Event final
{
    EventKind   kind;
    std::size_t duration;
    float       resource_usage;
};

struct [[nodiscard]] Process final
{
    using EventsQueue = std::deque<Event>;

    std::string name;
    std::size_t pid;
    std::size_t arrival;
    EventsQueue events;

    std::optional<std::size_t> start_time  = std::nullopt;
    std::optional<std::size_t> finish_time = std::nullopt;
};

} // namespace Os


template<>
struct std::formatter<Os::EventKind>
{
    constexpr auto parse(auto& ctx) { return ctx.begin(); }

    auto format(Os::EventKind event, auto& ctx) const
    {
        constexpr static auto visitor = [](Os::EventKind value) constexpr -> std::string {
            static_assert(
              std::to_underlying(Os::EventKind::Count) == 2,
              "[ERROR] Exhaustive handling of all enum variants for EventKind is required"
            );

            switch (value) {
                case Os::EventKind::Cpu: {
                    return "Cpu";
                }
                case Os::EventKind::Io: {
                    return "Io";
                }
                default: {
                    assert(false && "unreachable");
                    return "unreachable";
                }
            }
        };

        return std::format_to(ctx.out(), "{}", visitor(event));
    }
};

template<>
struct std::formatter<Os::Event>
{
    constexpr auto parse(auto& ctx) { return ctx.begin(); }

    auto format(const Os::Event& event, auto& ctx) const
    {
        return std::format_to(
          ctx.out(),
          "Event {{ kind = {}, duration = {}, usage = {}% }}",
          event.kind,
          event.duration,
          static_cast<std::size_t>(event.resource_usage * 100)
        );
    }
};

template<>
struct std::formatter<std::deque<Os::Event>>
{
    constexpr auto parse(auto& ctx)
    {
        auto       it  = ctx.begin();
        const auto end = ctx.end();

        if (it != end && *it == 's') {
            line_mode = LineMode::SingleLine;
            ++it;
        } else if (it != end && *it == 'm') {
            line_mode = LineMode::Multiline;
            ++it;
        }

        if (it != end && *it != '}') { throw std::format_error("invalid format"); }

        return it;
    }

    auto format(const std::deque<Os::Event>& events, auto& ctx) const
    {
        switch (line_mode) {
            case LineMode::Multiline: {
                std::format_to(ctx.out(), "[\n");
                for (const auto& event : events) { std::format_to(ctx.out(), "            {},\n", event); }
                return std::format_to(ctx.out(), "        ]");
            }
            case LineMode::SingleLine: {
                std::format_to(ctx.out(), "[ ");
                for (const auto& event : events) { std::format_to(ctx.out(), "{}, ", event); }
                return std::format_to(ctx.out(), "]");
            }
        }

        assert(false && "unreachable");
    }

  private:
    enum class LineMode : std::uint8_t
    {
        SingleLine = 0,
        Multiline,
    };

    LineMode line_mode = LineMode::Multiline;
};

template<>
struct std::formatter<Os::Process>
{
    constexpr auto parse(auto& ctx)
    {
        auto       it  = ctx.begin();
        const auto end = ctx.end();

        if (it != end && *it == 's') {
            line_mode = LineMode::SingleLine;
            ++it;
        } else if (it != end && *it == 'm') {
            line_mode = LineMode::Multiline;
            ++it;
        }

        if (it != end && *it != '}') { throw std::format_error("invalid format"); }

        return it;
    }

    auto format(const Os::Process& process, auto& ctx) const
    {
        switch (line_mode) {
            case LineMode::Multiline: {
                return std::format_to(
                  ctx.out(),
                  "Process {{\n        name: {},\n        pid: {},\n        arrival: {},\n        events: {}\n        "
                  "waiting time: {}\n        turnaround time: {}\n    }}",
                  process.name,
                  process.pid,
                  process.arrival,
                  process.events,
                  process.start_time.has_value() ? *process.start_time - process.arrival : 0,
                  process.finish_time.has_value() ? *process.finish_time - process.arrival : 0
                );
            }
            case LineMode::SingleLine: {
                return std::format_to(
                  ctx.out(),
                  "Process {{ name: {}, pid: {}, arrival: {}, events: {:s}, waiting time: {}, turnaround time: {} }}",
                  process.name,
                  process.pid,
                  process.arrival,
                  process.events,
                  process.start_time.has_value() ? *process.start_time - process.arrival : 0,
                  process.finish_time.has_value() ? *process.finish_time - process.arrival : 0
                );
            }
        }

        assert(false && "unreachable");
    }


  private:
    enum class LineMode : std::uint8_t
    {
        SingleLine = 0,
        Multiline,
    };

    LineMode line_mode = LineMode::Multiline;
};
