#include "Parser.hpp"
#include "Token.hpp"
#include "Util.hpp"

namespace Interpreter
{

auto Parser::parse(const std::vector<Token>& tokens) -> std::optional<Ast>
{
    Parser parser(tokens);

    while (parser.has_more()) {
        if (const auto statement = parser.expression_statement(); statement) {
            parser.ast.statements.push_back(*statement);
        }
    }

    return parser.ast;
}

auto Parser::expression_statement() -> std::optional<Statement>
{
    const auto expr = TRY(expression());
    return Statement { .kind = expr.id, .span = expr.span, .id = expr.id };
}

auto Parser::expression() -> std::optional<Expression>
{
    const auto current_token = TRY(peek());
    switch (current_token.kind) {
        case TokenKind::Keyword: {
            if (current_token.lexeme == "for") { return for_loop(); }
        }
        default: {
            return primary_expression();
        }
    }

    assert(false && "unreachable");
}

auto Parser::primary_expression() -> std::optional<Expression>
{
    const auto maybe_token = peek();
    if (!maybe_token) {
        std::println("[ERROR] Expected primary expression but ran out of tokens");
        return std::nullopt;
    }

    const auto token = *maybe_token;
    switch (token.kind) {
        case TokenKind::Identifier: {
            // FIXME: This is a hack
            if (const auto maybe_token = peek(1); maybe_token && maybe_token->kind == TokenKind::LeftParen) {
                return call_expression();
            } else if (const auto maybe_token = peek(1); maybe_token && maybe_token->kind == TokenKind::ColonColon) {
                return constant_definition();
            } else {
                TRY(consume_then_match(TokenKind::Identifier));
                return ast.emplace_expression(Variable { .name = token }, token.span, expression_id++);
            }
        }
        case TokenKind::StringLiteral: {
            return string_literal();
        }
        case TokenKind::Number: {
            return number();
        }
        case TokenKind::LeftBracket: {
            return list();
        }
        case TokenKind::LeftParen: {
            return tuple();
        }
        default: {
            assert(false && "unhandled primary expression");
        }
    }
}

auto Parser::string_literal() -> std::optional<Expression>
{
    const auto token = TRY(consume_then_match(TokenKind::StringLiteral));
    return ast.emplace_expression(StringLiteral { .literal = token }, token.span, expression_id++);
}

auto Parser::number() -> std::optional<Expression>
{
    const auto token = TRY(consume_then_match(TokenKind::Number));
    return ast.emplace_expression(Number { .number = token }, token.span, expression_id++);
}

auto Parser::list() -> std::optional<Expression>
{
    const auto left_bracket = TRY(consume_then_match(TokenKind::LeftBracket));

    std::vector<ExpressionId> elements = {};

    // TODO: Find a way to generalize this logic
    bool done     = false;
    auto end_span = left_bracket.span;
    for (auto maybe_token = peek(); maybe_token.has_value() && !done; maybe_token = peek()) {
        const auto token = *maybe_token;
        switch (token.kind) {
            case TokenKind::RightBracket: {
                const auto right_bracket = TRY(consume_then_match(TokenKind::RightBracket));
                end_span                 = right_bracket.span;
                done                     = true;
                break;
            }
            case TokenKind::Comma: {
                TRY(consume_then_match(TokenKind::Comma));
                break;
            }
            default: {
                const auto expr = TRY(expression());
                elements.push_back(expr.id);
                break;
            }
        }
    }

    return ast.emplace_expression(
      List { .elements = elements }, Span::join(left_bracket.span, end_span), expression_id++
    );
}

auto Parser::tuple() -> std::optional<Expression>
{
    const auto left_paren = TRY(consume_then_match(TokenKind::LeftParen));

    std::vector<ExpressionId> elements = {};

    // TODO: Find a way to generalize this logic
    bool done     = false;
    auto end_span = left_paren.span;
    for (auto maybe_token = peek(); maybe_token.has_value() && !done; maybe_token = peek()) {
        const auto token = *maybe_token;
        switch (token.kind) {
            case TokenKind::RightParen: {
                const auto right_paren = TRY(consume_then_match(TokenKind::RightParen));
                end_span               = right_paren.span;
                done                   = true;
                break;
            }
            case TokenKind::Comma: {
                TRY(consume_then_match(TokenKind::Comma));
                break;
            }
            default: {
                const auto expr = TRY(expression());
                elements.push_back(expr.id);
                break;
            }
        }
    }

    return ast.emplace_expression(
      Tuple { .elements = elements }, Span::join(left_paren.span, end_span), expression_id++
    );
}

auto Parser::call_expression() -> std::optional<Expression>
{
    const auto callee    = TRY(identifier());
    const auto arguments = TRY(call_expression_arguments());

    auto end_span = callee.span;
    if (!arguments.empty()) { end_span = ast.expression_by_id(arguments.back()).span; }

    return ast.emplace_expression(
      Call { .identifier = callee, .arguments = arguments }, Span::join(callee.span, end_span), expression_id++
    );
}

auto Parser::call_expression_arguments() -> std::optional<std::vector<ExpressionId>>
{
    (void)consume_then_match(TokenKind::LeftParen);

    // TODO: Find a way to generalize this logic
    std::vector<ExpressionId> result = {};
    bool                      done   = false;
    for (auto maybe_token = peek(); maybe_token.has_value() && !done; maybe_token = peek()) {
        const auto token = *maybe_token;
        switch (token.kind) {
            case TokenKind::RightParen: {
                TRY(consume_then_match(TokenKind::RightParen));
                done = true;
                break;
            }
            case TokenKind::Comma: {
                TRY(consume_then_match(TokenKind::Comma));
                break;
            }
            default: {
                const auto expr = TRY(expression());
                result.push_back(expr.id);
                break;
            }
        }
    }

    return result;
}

auto Parser::constant_definition() -> std::optional<Expression>
{
    const auto name = TRY(identifier());

    (void)TRY(consume_then_match(TokenKind::ColonColon));

    const auto value = TRY(primary_expression());

    return ast.emplace_expression(
      Constant {
        .name  = name,
        .value = value.id,
      },
      Span::join(name.span, value.span),
      expression_id++
    );
}

auto Parser::for_loop() -> std::optional<Expression>
{
    const auto for_token = TRY(consume_then_match(TokenKind::Keyword));
    assert(for_token.lexeme == "for" && "unreachable");

    const auto range_expression = TRY(range());

    const auto left_curly = TRY(consume_then_match(TokenKind::LeftCurly));
    assert(left_curly.lexeme[0] == '{' && "unreachable");

    std::vector<ExpressionId> body;
    auto                      last_span = left_curly.span;
    for (auto current_token = peek(); current_token && current_token->kind != TokenKind::RightCurly;
         current_token      = next()) {
        const auto expr = TRY(expression());
        body.push_back(expr.id);
        last_span = expr.span;
    }

    return ast.emplace_expression(
      For {
        .range = range_expression.id,
        .body  = body,
      },
      Span::join(for_token.span, last_span),
      expression_id++
    );
}

auto Parser::range() -> std::optional<Expression>
{
    const auto start_range = TRY(consume_then_match(TokenKind::Number));
    const auto dotdot      = TRY(consume_then_match(TokenKind::DotDot));
    const auto end_range   = TRY(consume_then_match(TokenKind::Number));

    return ast.emplace_expression(
      Range {
        .start = start_range,
        .end   = end_range,
      },
      Span::join(start_range.span, end_range.span),
      expression_id++
    );
}

auto Parser::identifier() -> std::optional<Token> { return consume_then_match(TokenKind::Identifier); }

auto Parser::consume_then_match(TokenKind expected) -> std::optional<Token>
{
    const auto maybe_token = next();
    if (!maybe_token) {
        std::println(stderr, "[ERROR] Expected {} but ran out of tokens", expected);
        return std::nullopt;
    }

    const auto token = *maybe_token;
    if (token.kind != expected) {
        std::println(stderr, "[ERROR] Expected {} but got {}", expected, token.kind);
        return std::nullopt;
    }

    return token;
}

auto Parser::has_more() const -> bool { return !(cursor >= tokens.size()); }

auto Parser::peek(const std::size_t offset) const -> std::optional<Token>
{
    if (cursor + offset < tokens.size()) { return tokens[cursor + offset]; }

    return std::nullopt;
}

auto Parser::next() -> std::optional<Token>
{
    if (has_more()) { return tokens[cursor++]; }

    return std::nullopt;
}

Parser::Parser(const std::vector<Token>& tokens)
  : tokens { tokens }
{}

} // namespace Interpreter
