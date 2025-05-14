#pragma once

#include <algorithm>
#include <cassert>
#include <print>
#include <utility>

#include "Span.hpp"

namespace Interpreter
{
enum class [[nodiscard]] TokenKind : std::uint8_t
{
    // Single character token
    LeftParen = 0,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftCurly,
    RightCurly,
    Comma,

    // Multi character token
    Keyword,
    Identifier,
    StringLiteral,
    Number,
    ColonColon,
    DotDot,

    Count,
};

struct [[nodiscard]] Token final
{
    [[nodiscard]] constexpr static auto is_keyword(const std::string_view lexeme) -> bool
    {
        constexpr static std::string_view keywords[] = { "for" };
        return std::ranges::contains(keywords, lexeme);
    }

    std::string_view lexeme;
    TokenKind        kind;
    Span             span;
};
} // namespace Interpreter

template<>
struct std::formatter<Interpreter::TokenKind>
{
    constexpr auto parse(auto& ctx) { return ctx.begin(); }

    auto format(Interpreter::TokenKind kind, auto& ctx) const
    {
        const auto kind_to_str = [](Interpreter::TokenKind kind) {
            static_assert(
              std::to_underlying(Interpreter::TokenKind::Count) == 13
              && "Exhaustive handling of all enum variants for Interpreter::TokenKind is required."
            );
            switch (kind) {
                case Interpreter::TokenKind::LeftParen: {
                    return "TokenKind::LeftParen";
                }
                case Interpreter::TokenKind::RightParen: {
                    return "TokenKind::RightParen";
                }
                case Interpreter::TokenKind::Keyword: {
                    return "TokenKind::Keyword";
                }
                case Interpreter::TokenKind::Identifier: {
                    return "TokenKind::Identifier";
                }
                case Interpreter::TokenKind::StringLiteral: {
                    return "TokenKind::StringLiteral";
                }
                case Interpreter::TokenKind::Number: {
                    return "TokenKind::Number";
                }
                case Interpreter::TokenKind::LeftBracket: {
                    return "TokenKind::LeftBracket";
                }
                case Interpreter::TokenKind::RightBracket: {
                    return "TokenKind::RightBracket";
                }
                case Interpreter::TokenKind::Comma: {
                    return "TokenKind::Comma";
                }
                case Interpreter::TokenKind::ColonColon: {
                    return "TokenKind::ColonColon";
                }
                case Interpreter::TokenKind::LeftCurly: {
                    return "TokenKind::LeftCurly";
                }
                case Interpreter::TokenKind::RightCurly: {
                    return "TokenKind::RightCurly";
                }
                case Interpreter::TokenKind::DotDot: {
                    return "TokenKind::DotDot";
                }
                default: {
                    assert(false && "unreachable");
                }
            }
        };

        return std::format_to(ctx.out(), "{}", kind_to_str(kind));
    }
};

template<>
struct std::formatter<Interpreter::Token>
{
    constexpr auto parse(auto& ctx) { return ctx.begin(); }

    auto format(const Interpreter::Token& token, auto& ctx) const
    {
        return std::format_to(
          ctx.out(), "{{ lexeme = \"{}\", kind = {}, span = {} }}", token.lexeme, token.kind, token.span
        );
    }
};
