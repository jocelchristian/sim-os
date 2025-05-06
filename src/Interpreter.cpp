#include "Interpreter.hpp"
#include "Util.hpp"
#include <cassert>
#include <utility>

namespace Interpreter
{

[[nodiscard]] static auto token_kind_try_from_character(const std::string_view character) -> std::optional<TokenKind>
{
    static_assert(
      std::to_underlying(TokenKind::Count) == 9, "Exhastive handling of all enum variants for TokenKind is required."
    );

    assert(character.size() >= 1);
    switch (character[0]) {
        case '(':
            return TokenKind::LeftParen;
        case ')':
            return TokenKind::RightParen;
        case '[':
            return TokenKind::LeftBracket;
        case ']':
            return TokenKind::RightBracket;
        case ',':
            return TokenKind::Comma;
        default: {
            std::println(stderr, "[ERROR] Unexpected single character token {}", character[0]);
            return std::nullopt;
        }
    }
}

auto Lexer::lex(const std::string_view source) -> std::optional<std::vector<Token>>
{
    std::vector<Token> result = {};
    Lexer              lexer(source);

    while (lexer.has_more()) {
        if (const auto token = lexer.next_token(); token) {
            result.push_back(*token);
        }
    }

    return result;
}

Lexer::Lexer(const std::string_view source)
  : source{ source }
{}

auto Lexer::single_character_token(const std::string_view character) -> std::optional<Token> {
    const auto kind = TRY(token_kind_try_from_character(character));

    const auto lexeme = source.substr(cursor, 1);
    const auto token = Token{ .lexeme = lexeme,
                              .kind   = kind,
                              .span   = Span{ .start = cursor, .end = cursor + 1 } };

    advance();
    return token;
}

auto Lexer::keyword_or_identifier() -> std::optional<Token>
{
    assert(std::isalnum(source[cursor]) && "unexpected character");

    const auto start_idx = cursor;
    auto       end_idx   = cursor;
    while (true) { const auto peeked = TRY(peek());
        if (std::isalnum(peeked[0]) == 0 && peeked[0] != '_') {
            end_idx = cursor;
            break;
        }
        advance();
    }

    const auto lexeme = source.substr(start_idx, end_idx - start_idx);
    return Token{
        .lexeme = lexeme,
        .kind   = Token::is_keyword(lexeme) ? TokenKind::Keyword : TokenKind::Identifier,
        .span   = Span{ .start = start_idx, .end = end_idx },
    };
}

auto Lexer::string_literal() -> std::optional<Token>
{
    assert((*next())[0] == '"' && "expected \"");

    const auto start_idx = cursor;
    auto       end_idx   = cursor;
    while (true) {
        const auto peeked = TRY(peek());
        if (peeked[0] == '"') {
            end_idx = cursor;
            advance();
            break;
        }

        advance();
    }

    return Token{ .lexeme = source.substr(start_idx, end_idx - start_idx),
                  .kind   = TokenKind::StringLiteral,
                  .span   = Span{ .start = start_idx, .end = end_idx } };
}

auto Lexer::number() -> std::optional<Token>
{
    assert(std::isdigit((*peek())[0]) && "expected number");

    const auto start_idx = cursor;
    auto       end_idx   = cursor;
    while (true) {
        const auto peeked = TRY(peek());
        if (std::isdigit(peeked[0]) == 0) {
            end_idx = cursor;
            break;
        }

        advance();
    }

    return Token{ .lexeme = source.substr(start_idx, end_idx - start_idx),
                  .kind   = TokenKind::Number,
                  .span   = Span{ .start = start_idx, .end = end_idx } };
}

auto Lexer::has_more() const -> bool { return !(cursor >= source.size()); }

auto Lexer::next_token() -> std::optional<Token>
{
    skip_whitespace();

    const auto next_character = TRY(peek());

    if (std::isdigit(next_character[0]) != 0) { return number(); }

    assert(next_character.size() >= 1);
    switch (next_character[0]) {
        case '[':
        case ']':
        case ',':
        case '(':
        case ')': {
            return single_character_token(next_character);
        }
        case '"': {
            return string_literal();
        }
        default: {
            return keyword_or_identifier();
        }
    }
}

auto Lexer::peek(const std::size_t offset) const -> std::optional<std::string_view>
{
    if (cursor + offset >= source.size()) { return std::nullopt; }

    return source.substr(cursor + offset, 1);
}


auto Lexer::next() -> std::optional<std::string_view>
{
    if (!has_more()) { return std::nullopt; }

    auto ret = source.substr(cursor, 1);
    ++cursor;
    return ret;
}

void Lexer::advance(const std::size_t amount) { cursor += amount; }

void Lexer::skip_whitespace()
{
    while (true) {
        if (const auto peeked = peek(); !peeked || std::isspace((*peeked)[0]) == 0) { break; }
        advance();
    }
}

auto Parser::parse(std::vector<Token> &&tokens) -> std::optional<Statement>
{
    Parser parser(std::move(tokens));

    while (parser.has_more()) {
        // TODO: Decide how we are going to handle parsing
    }

    return std::nullopt;
}

auto Parser::has_more() const -> bool { return !(cursor >= tokens.size()); }

Parser::Parser(std::vector<Token> &&tokens)
  : tokens{ std::move(tokens) }
{}

} // namespace Interpreter
