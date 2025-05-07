#pragma once

#include <sstream>
#include <variant>
#include <vector>

#include "Span.hpp"
#include "Token.hpp"

namespace Interpreter
{

using ExpressionId  = std::size_t;
using StatementKind = std::variant<ExpressionId>;

struct [[nodiscard]] Call final
{
    Token                     identifier;
    std::vector<ExpressionId> arguments;
};

struct [[nodiscard]] StringLiteral final
{
    Token literal;
};

struct [[nodiscard]] Number final
{
    Token number;
};

struct [[nodiscard]] List final
{
    std::vector<ExpressionId> elements;
};

struct [[nodiscard]] Tuple final
{
    std::vector<ExpressionId> elements;
};

struct [[nodiscard]] Variable final
{
    Token name;
};

using ExpressionKind = std::variant<Call, StringLiteral, Number, List, Tuple, Variable>;

struct [[nodiscard]] Expression final
{
    ExpressionKind kind;
    Span           span;
    std::size_t    id;
};

struct [[nodiscard]] Statement final
{
    StatementKind kind;
    Span          span;
    std::size_t   id;
};

struct [[nodiscard]] Ast final
{
    std::vector<Statement>  statements;
    std::vector<Expression> expressions;

    [[nodiscard]] auto statement_by_id(const std::size_t id) const -> Statement { return statements[id]; }

    [[nodiscard]] auto expression_by_id(const std::size_t id) const -> Expression { return expressions[id]; }

    template<typename... Args>
    Statement &emplace_statement(Args &&...args)
    {
        return statements.emplace_back(std::forward<Args>(args)...);
    }

    template<typename... Args>
    Expression &emplace_expression(Args &&...args)
    {
        return expressions.emplace_back(std::forward<Args>(args)...);
    }
};

} // namespace Interpreter

template<>
struct std::formatter<Interpreter::StatementKind>
{
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(const Interpreter::StatementKind &kind, auto &ctx) const
    {
        static_assert(
          std::variant_size_v<Interpreter::StatementKind> == 1
          && "Exhaustive handling of all variants for StatementKind is required."
        );
        const auto result = std::visit(
          [](const auto &value) -> std::string {
              if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::ExpressionId>) {
                  return std::format("ExpressiondId {{ id = {} }}", value);
              } else {
                  static_assert(false, "Unhandled StatementKind variant alternative");
              }
          },
          kind
        );

        return std::format_to(ctx.out(), "{}", result);
    }
};

template<>
struct std::formatter<Interpreter::Statement>
{
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(const Interpreter::Statement &statement, auto &ctx) const
    {
        return std::format_to(
          ctx.out(), "Statement {{ kind = {}, span = {}, id = {} }}", statement.kind, statement.span, statement.id
        );
    }
};

template<>
struct std::formatter<Interpreter::ExpressionKind>
{
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(const Interpreter::ExpressionKind &kind, auto &ctx) const
    {
        const auto join_expressions = [](const auto &arguments) -> std::string {
            std::stringstream ss;
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                const auto elem = arguments[i];
                if (i != arguments.size() - 1) {
                    ss << std::format("ExpressionId(#{}), ", elem);
                } else {
                    ss << std::format("ExpressionId(#{})", elem);
                }
            }
            return ss.str();
        };


        static_assert(
          std::variant_size_v<Interpreter::ExpressionKind> == 6,
          "Exhaustive handling of all variants for ExpressionKind is required."
        );
        const auto result = std::visit(
          [&](const auto &value) -> std::string {
              if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::Call>) {
                  return std::format(
                    "Call {{ identifier = {}, arguments = {} }}", value.identifier, join_expressions(value.arguments)
                  );
              } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::StringLiteral>) {
                  return std::format("StringLiteral {{ literal = {} }}", value.literal.lexeme);
              } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::Number>) {
                  return std::format("Number {{ number = {} }}", value.number.lexeme);
              } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::List>) {
                  return std::format("List {{ elements = {} }}", join_expressions(value.elements));
              } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::Tuple>) {
                  return std::format("Tuple {{ elements = {} }}", join_expressions(value.elements));
              } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Interpreter::Variable>) {
                  return std::format("Variable {{ name = {} }}", value.name.lexeme);
              } else {
                  static_assert(false, "Unhandled ExpressionKind variant alternative");
              }
          },
          kind
        );

        return std::format_to(ctx.out(), "{}", result);
    }
};

template<>
struct std::formatter<Interpreter::Expression>
{
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(const Interpreter::Expression &expression, auto &ctx) const
    {
        return std::format_to(
          ctx.out(), "Expression {{ kind = {}, span = {}, id = {} }}", expression.kind, expression.span, expression.id
        );
    }
};
