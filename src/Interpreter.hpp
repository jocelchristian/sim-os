#pragma once

#include <cassert>
#include <cstring>
#include <deque>
#include <print>
#include <ranges>

#include "Lexer.hpp"
#include "Parser.hpp"
#include "Simulation.hpp"
#include "Util.hpp"

namespace Interpreter
{

template<typename SimOs>
class [[nodiscard]] Interpreter final
{
  public:
    [[nodiscard]] static auto eval_file(const std::string_view file_content, SimOs &sim) -> bool
    {
        Interpreter interpreter(sim);

        // Lex the file
        std::println("--- Lexing file ---");
        const auto tokens = Lexer::lex(file_content);
        if (!tokens) { return false; }

        std::println("- Tokens -");
        for (const auto &[idx, token] : std::views::zip(std::views::iota(0), *tokens)) {
            std::println("#{}: {}", idx, token);
        }

        // Generate AST
        std::println("--- Generating AST ---");
        const auto ast = Parser::parse(*tokens);

        std::println("- Statements -");
        if (!ast) { return false; }
        for (const auto &[idx, statement] : std::views::zip(std::views::iota(0), ast->statements)) {
            std::println("#{}: {}", idx, statement);
        }

        std::println("- Expressions -");
        for (const auto &[idx, expression] : std::views::zip(std::views::iota(0), ast->expressions)) {
            std::println("#{}: {}", idx, expression);
        }

        // Evaluate AST

        // FIXME: Super mega hyper duper stupid evaluation
        // this code is trash i just wanted to see it happen
        // for the moment. Will clean up for sure
        for (const auto &statement : ast->statements) {
            if (std::holds_alternative<ExpressionId>(statement.kind)) {
                const auto expression_id = std::get<ExpressionId>(statement.kind);
                const auto expr          = ast->expression_by_id(expression_id);
                if (std::holds_alternative<Call>(expr.kind)) {
                    const auto call = std::get<Call>(expr.kind);
                    if (call.identifier.lexeme == "spawn_process") {
                        const auto name_id   = call.arguments[0];
                        const auto name_expr = ast->expression_by_id(name_id);
                        assert(std::holds_alternative<StringLiteral>(name_expr.kind));
                        const auto name = std::get<StringLiteral>(name_expr.kind);

                        const auto pid_id   = call.arguments[1];
                        const auto pid_expr = ast->expression_by_id(pid_id);
                        assert(std::holds_alternative<Number>(pid_expr.kind));
                        const auto pid_s     = std::get<Number>(pid_expr.kind);
                        const auto maybe_pid = Util::parse_number(pid_s.number.lexeme);
                        if (!maybe_pid) { return false; }
                        const auto pid = *maybe_pid;

                        const auto arrival_id   = call.arguments[2];
                        const auto arrival_expr = ast->expression_by_id(arrival_id);
                        assert(std::holds_alternative<Number>(arrival_expr.kind));
                        const auto arrival_s     = std::get<Number>(arrival_expr.kind);
                        const auto maybe_arrival = Util::parse_number(arrival_s.number.lexeme);
                        if (!maybe_arrival) { return false; }
                        const auto arrival = *maybe_arrival;

                        const auto events_argument_id = call.arguments[3];
                        const auto events_expr        = ast->expression_by_id(events_argument_id);
                        assert(std::holds_alternative<List>(events_expr.kind));
                        const auto events_arguments = std::get<List>(events_expr.kind);

                        std::deque<Simulation::Event> events = {};
                        for (unsigned long event_tuple_id : events_arguments.elements) {
                            const auto event_tuple = ast->expression_by_id(event_tuple_id);
                            if (std::holds_alternative<Tuple>(event_tuple.kind)) {
                                const auto tuple = std::get<Tuple>(event_tuple.kind);

                                const auto event_kind_id      = tuple.elements[0];
                                const auto event_kind_variant = ast->expression_by_id(event_kind_id);
                                assert(std::holds_alternative<Variable>(event_kind_variant.kind));
                                const auto event_kind = std::get<Variable>(event_kind_variant.kind);

                                const auto duration_id      = tuple.elements[1];
                                const auto duration_variant = ast->expression_by_id(duration_id);
                                assert(std::holds_alternative<Number>(duration_variant.kind));
                                const auto duration_s   = std::get<Number>(duration_variant.kind);
                                const auto maybe_number = Util::parse_number(duration_s.number.lexeme);
                                if (!maybe_number) { return false; }
                                const auto duration = *maybe_number;


                                const auto event = Simulation::Event{ .kind     = event_kind.name.lexeme == "Io"
                                                                                    ? Simulation::EventKind::Io
                                                                                    : Simulation::EventKind::Cpu,
                                                                      .duration = duration };

                                events.push_back(event);
                            } else {
                                std::println(stderr, "[ERROR] Unsupported expression kind: {}", statement);
                                return false;
                            }
                        }

                        interpreter.sim.emplace_process(name.literal.lexeme, pid, arrival, events);
                    }
                } else {
                    std::println(stderr, "[ERROR] Unsupported expression kind: {}", statement);
                    return false;
                }
            } else {
                std::println(stderr, "[ERROR] Unsupported statement kind: {}", statement);
                return false;
            }
        }

        return true;
    }

  private:
    explicit Interpreter(SimOs &sim)
      : sim{ sim }
    {}

    SimOs& sim;
};

} // namespace Interpreter
