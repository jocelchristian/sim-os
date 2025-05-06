#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <print>
#include <sstream>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Interpreter
{

struct [[nodiscard]] Span final
{
    std::size_t start = 0;
    std::size_t end   = 0;
};

enum class [[nodiscard]] TokenKind : std::uint8_t
{
    // Single character token
    LeftParen = 0,
    RightParen,
    LeftBracket,
    RightBracket,
    Comma,

    // Multi character token
    Keyword,
    Identifier,
    StringLiteral,
    Number,

    Count,
};

struct [[nodiscard]] Token final
{
    [[nodiscard]] constexpr static auto is_keyword(const std::string_view lexeme) -> bool
    {
        constexpr static std::string_view keywords[] = { "spawn_process" };
        return std::ranges::contains(keywords, lexeme);
    }

    std::string_view lexeme;
    TokenKind        kind;
    Span             span;
};

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

// FIXME: change this type when actually implementing the parsing
using Statement = std::variant<int>;

class [[nodiscard]] Parser final
{
  public:
    [[nodiscard]] static auto parse(std::vector<Token> &&tokens) -> std::optional<Statement>;

  private:
    explicit Parser(std::vector<Token> &&tokens);

    [[nodiscard]] auto has_more() const -> bool;

  private:
    std::vector<Token> tokens;
    std::size_t        cursor = 0;
};

template<typename Simulation>
class [[nodiscard]] Interpreter final
{
  public:
    [[nodiscard]] static auto eval_file(const std::filesystem::path &path) -> bool
    {
        auto file = std::ifstream(path);
        if (!file) { std::println(stderr, "[ERROR] Unable to read file {}: {}", path.string(), strerror(errno)); }

        // Read source code
        std::stringstream ss;
        ss << file.rdbuf();
        const auto file_content = ss.str();

        // Lex the file
        const auto tokens = Lexer::lex(file_content);
        if (!tokens) { return false; }
        for (const auto &token : *tokens) { std::println("{}", token); }

        // Generate AST

        // Evaluate AST

        return true;
    }

  private:
    explicit Interpreter(Simulation &sim);

    std::shared_ptr<Simulation> sim;
};

} // namespace Interpreter

template<>
struct std::formatter<Interpreter::Span>
{
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(const Interpreter::Span &span, auto &ctx) const
    {
        return std::format_to(ctx.out(), "{{ start = {}, end = {} }}", span.start, span.end);
    }
};

template<>
struct std::formatter<Interpreter::TokenKind>
{
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(Interpreter::TokenKind kind, auto &ctx) const
    {
        const auto kind_to_str = [](Interpreter::TokenKind kind) {
            static_assert(
              std::to_underlying(Interpreter::TokenKind::Count) == 9
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
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(const Interpreter::Token &token, auto &ctx) const
    {
        return std::format_to(
          ctx.out(), "{{ lexeme = {}, kind = {}, span = {} }}", token.lexeme, token.kind, token.span
        );
    }
};
