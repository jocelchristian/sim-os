#pragma once

#include "Ast.hpp"
#include "Token.hpp"

#include <vector>

namespace Interpreter
{

class [[nodiscard]] Parser final
{
  public:
    [[nodiscard]] static auto parse(const std::vector<Token> &tokens) -> std::optional<Ast>;

  private:
    explicit Parser(const std::vector<Token> &tokens);

    [[nodiscard]] auto expression_statement() -> std::optional<Statement>;

    [[nodiscard]] auto expression() -> std::optional<Expression>;
    [[nodiscard]] auto primary_expression() -> std::optional<Expression>;
    [[nodiscard]] auto string_literal() -> std::optional<Expression>;
    [[nodiscard]] auto number() -> std::optional<Expression>;
    [[nodiscard]] auto list() -> std::optional<Expression>;
    [[nodiscard]] auto tuple() -> std::optional<Expression>;
    [[nodiscard]] auto call_expression() -> std::optional<Expression>;
    [[nodiscard]] auto call_expression_arguments() -> std::optional<std::vector<ExpressionId>>;

    [[nodiscard]] auto identifier() -> std::optional<Token>;

    [[nodiscard]] auto consume_then_match(TokenKind expected) -> std::optional<Token>;

    [[nodiscard]] auto has_more() const -> bool;
    [[nodiscard]] auto peek(const std::size_t offset = 0) const -> std::optional<Token>;
    [[nodiscard]] auto next() -> std::optional<Token>;

  private:
    std::vector<Token> tokens;
    std::size_t        cursor = 0;

    Ast ast = {};
    std::size_t expression_id = 0;
};

} // namespace Interpreter
