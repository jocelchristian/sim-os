#pragma once

#include <print>

namespace Interpreter
{

struct [[nodiscard]] Span final
{
    [[nodiscard]] auto static join(const Span& lhs, const Span& rhs) -> Span
    {
        return Span { .start = lhs.start, .end = rhs.end };
    }

    std::size_t start = 0;
    std::size_t end   = 0;
};

} // namespace Interpreter

template<>
struct std::formatter<Interpreter::Span>
{
    constexpr auto parse(auto& ctx) { return ctx.begin(); }

    auto format(const Interpreter::Span& span, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{{ start = {}, end = {} }}", span.start, span.end);
    }
};
