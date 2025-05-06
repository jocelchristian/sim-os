#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "Token.hpp"

namespace Interpreter
{

class [[nodiscard]] Lexer final
{
  public:
    [[nodiscard]] static auto lex(const std::string_view source) -> std::optional<std::vector<Token>>;

  private:
    explicit Lexer(const std::string_view source);

    [[nodiscard]] auto single_character_token(const std::string_view character) -> std::optional<Token>;
    [[nodiscard]] auto keyword_or_identifier() -> std::optional<Token>;
    [[nodiscard]] auto string_literal() -> std::optional<Token>;
    [[nodiscard]] auto number() -> std::optional<Token>;

    [[nodiscard]] auto has_more() const -> bool;
    [[nodiscard]] auto next_token() -> std::optional<Token>;

    [[nodiscard]] auto peek(const std::size_t offset = 0) const -> std::optional<std::string_view>;
    [[nodiscard]] auto next() -> std::optional<std::string_view>;
    void               advance(const std::size_t amount = 1);

    void skip_whitespace();

  private:
    std::string_view source;
    std::size_t      cursor = 0;
};

} // namespace Interpreter
