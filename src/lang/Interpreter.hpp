#pragma once

#include <cassert>
#include <cstring>
#include <deque>
#include <iterator>
#include <memory>
#include <print>
#include <ranges>
#include <utility>

#include "Lexer.hpp"
#include "os/Os.hpp"
#include "Parser.hpp"
#include "Util.hpp"

namespace Interpreter
{

struct [[nodiscard]] Value final
{
    using ValueType = std::variant<std::string_view, std::size_t, std::vector<Value>, std::monostate>;

    Value()
      : value { std::monostate {} }
    {}

    explicit Value(const std::string_view string)
      : value { string }
    {}

    explicit Value(const std::size_t number)
      : value { number }
    {}

    explicit Value(const std::vector<Value>& values)
      : value { values }
    {}

    [[nodiscard]] constexpr auto is_string() const -> bool { return std::holds_alternative<std::string_view>(value); }

    [[nodiscard]] constexpr auto as_string() const -> std::string_view
    {
        return Util::get<std::string_view>(value).value();
    }

    template<std::invocable Callback>
    [[nodiscard]] constexpr auto as_string_or(Callback callback) const -> std::optional<std::string_view>
    {
        return is_string() ? as_string() : callback();
    }

    [[nodiscard]] constexpr auto is_number() const -> bool { return std::holds_alternative<std::size_t>(value); }

    [[nodiscard]] constexpr auto as_number() const -> std::size_t { return Util::get<std::size_t>(value).value(); }

    template<std::invocable Callback>
    [[nodiscard]] constexpr auto as_number_or(Callback callback) const -> std::optional<std::size_t>
    {
        return is_number() ? as_number() : callback();
    }

    [[nodiscard]] constexpr auto is_value_list() const -> bool
    {
        return std::holds_alternative<std::vector<Value>>(value);
    }

    [[nodiscard]] constexpr auto as_value_list() const -> std::vector<Value>
    {
        return Util::get<std::vector<Value>>(value).value();
    }

    template<std::invocable Callback>
    [[nodiscard]] constexpr auto as_value_list_or(Callback callback) const -> std::optional<std::vector<Value>>
    {
        return is_value_list() ? as_value_list() : callback();
    }

    [[nodiscard]] constexpr auto is_monostate() const -> bool { return std::holds_alternative<std::monostate>(value); }

  private:
    ValueType value;
};

template<typename Sim>
class [[nodiscard]] Interpreter final
{
  public:
    [[nodiscard]] static auto eval(const std::string_view file_content, const std::shared_ptr<Sim>& sim) -> bool
    {
        const auto tokens = Lexer::lex(file_content);
        if (!tokens) { return false; }

#ifdef DEBUG
        std::println("- Tokens -");
        for (const auto& [idx, token] : std::views::zip(std::views::iota(0), *tokens)) {
            std::println("#{}: {}", idx, token);
        }
#endif

        const auto ast = Parser::parse(*tokens);

#ifdef DEBUG
        std::println("- Statements -");
        if (!ast) { return false; }
        for (const auto& [idx, statement] : std::views::zip(std::views::iota(0), ast->statements)) {
            std::println("#{}: {}", idx, statement);
        }

        std::println("- Expressions -");
        for (const auto& [idx, expression] : std::views::zip(std::views::iota(0), ast->expressions)) {
            std::println("#{}: {}", idx, expression);
        }
#endif


        Interpreter interpreter(sim, *ast);
        return interpreter.evaluate_ast().has_value();
    }

  private:
    [[nodiscard]] auto evaluate_ast() -> std::optional<bool>
    {
        bool failed = false;
        for (const auto& statement : ast.statements) { failed |= TRY(evaluate_statement(statement)); }
        return failed;
    }

    [[nodiscard]] auto evaluate_statement(const Statement& statement) -> std::optional<bool>
    {
        const auto expression_visitor = [this](const StatementKind& kind) -> std::optional<bool> {
            const auto expr_id = TRY(Util::get<ExpressionId>(kind));
            return evaluate_expression(ast.expression_by_id(expr_id)).has_value();
        };

        const auto visitor = Util::make_visitor(expression_visitor);
        return std::visit(visitor, statement.kind);
    }

    [[nodiscard]] auto evaluate_expression(const Expression& expression) -> std::optional<Value>
    {
        static_assert(
          std::variant_size_v<ExpressionKind> == 9,
          "Exhaustive handling for all variants for ExpressionKind is required"
        );
        const auto call_expression_visitor = [this](const Call& call_expression) -> std::optional<Value> {
            const auto& [name, arguments] = call_expression;
            if (is_builtin(name)) {
                return builtin_handler(name.lexeme, arguments);
            } else {
                assert(false && "not implemented");
            }

            return Value();
        };

        const auto string_literal_visitor = [this](const StringLiteral& string_literal) -> std::optional<Value> {
            return Value(string_literal.literal.lexeme);
        };

        const auto number_visitor = [this](const Number& number) -> std::optional<Value> {
            const auto parsed_number = TRY(Util::parse_number(number.number.lexeme));
            return Value(parsed_number);
        };

        const auto list_visitor = [this](const List& list) -> std::optional<Value> {
            std::vector<Value> result;
            result.reserve(list.elements.size());

            for (const auto& elem : materialize_expressions(list.elements)) {
                result.push_back(TRY(evaluate_expression(elem)));
            }

            return Value(result);
        };


        const auto tuple_visitor = [this](const Tuple& tuple) -> std::optional<Value> {
            std::vector<Value> result;
            result.reserve(tuple.elements.size());

            for (const auto& elem : materialize_expressions(tuple.elements)) {
                result.push_back(TRY(evaluate_expression(elem)));
            }

            return Value(result);
        };

        const auto variable_visitor = [this](const Variable& variable) -> std::optional<Value> {
            return Value(variable.name.lexeme);
        };

        const auto constant_visitor = [this](const Constant& constant) -> std::optional<Value> {
            const auto name   = constant.name.lexeme;
            const auto number = TRY(Util::get<Number>(ast.expression_by_id(constant.value).kind));

            if (name == "max_processes") {
                sim->max_processes = TRY(Util::parse_number(number.number.lexeme));
            } else if (name == "max_events_per_process") {
                sim->max_events_per_process = TRY(Util::parse_number(number.number.lexeme));
            } else if (name == "max_single_event_duration") {
                sim->max_single_event_duration = TRY(Util::parse_number(number.number.lexeme));
            } else if (name == "max_arrival_time") {
                sim->max_arrival_time = TRY(Util::parse_number(number.number.lexeme));
            } else {
                report_error("invalid constant for current simulation: {}", name);
                report_note(
                  "available constants are: max_processes, max_events_per_process, max_single_event_duration, "
                  "max_arrival_time"
                );
            }


            return Value();
        };

        const auto range_visitor = [this](const Range& range) -> std::optional<Value> {
            const auto start = TRY(Util::parse_number(range.start.lexeme));
            const auto end   = TRY(Util::parse_number(range.end.lexeme));
            return Value(std::vector { Value(start), Value(end) });
        };

        const auto for_visitor = [this](const For& four) -> std::optional<Value> {
            return evalute_for_expression(four);
        };

        const auto visitor = Util::make_visitor(
          call_expression_visitor,
          string_literal_visitor,
          number_visitor,
          list_visitor,
          tuple_visitor,
          variable_visitor,
          constant_visitor,
          range_visitor,
          for_visitor
        );

        return std::visit(visitor, expression.kind);
    }

    [[nodiscard]] auto evalute_for_expression(const For& four) -> std::optional<Value>
    {
        const auto  range = TRY(Util::get<Range>(ast.expression_by_id(four.range).kind));
        const auto& start = TRY(Util::parse_number(range.start.lexeme));
        const auto& end   = TRY(Util::parse_number(range.end.lexeme));

        const auto body = materialize_expressions(four.body);
        for (std::size_t i = start; i < end; ++i) {
            for (const auto& expr : body) { (void)evaluate_expression(expr); }
        }

        return Value();
    }

    [[nodiscard]] constexpr static auto is_builtin(const Token& token) -> bool
    {
        constexpr static std::string_view builtins[] = { "spawn_process", "spawn_random_process" };
        return std::ranges::contains(builtins, token.lexeme);
    }

    [[nodiscard]] auto list_as_events_deque(const std::vector<Value>& list) const
      -> std::optional<std::deque<Os::Event>>
    {
        std::deque<Os::Event> events = {};
        for (const auto& tuple_value : list) {
            const auto tuple = TRY(tuple_value.as_value_list_or([&] -> std::optional<std::vector<Value>> {
                report_error("mismatched type for argument #{} of builtin `{}`: expected type `List<Tuple: Event>`");
                return report_note("(e.g. [(event_type: `Io` or `Cpu`, duration: int)])");
            }));

            const auto event_kind_str = TRY(tuple[0].as_string_or([&] -> std::optional<std::string_view> {
                report_error("mismatched type for argument #{} of builtin `{}`: expected type `List<Tuple: Event>`");
                return report_note("(e.g. [(event_type: `Io` or `Cpu`, duration: int)])");
            }));

            const auto duration = TRY(tuple[1].as_number_or([&] -> std::optional<std::size_t> {
                report_error("mismatched type for argument #{} of builtin `{}`: expected type `List<Tuple: Event>`");
                return report_note("(e.g. [(event_type: `Io` or `Cpu`, duration: int)])");
            }));

            const auto maybe_event_kind = Os::event_kind_try_from_str(event_kind_str);
            if (!maybe_event_kind) {
                report_error("mismatched type for argument #{} of builtin `{}`: expected type `List<Tuple: Event>`");
                return report_note("(e.g. [(event_type: `Io` or `Cpu`, duration: int)])");
            }

            events.push_back(Os::Event { .kind           = *maybe_event_kind,
                                         .duration       = duration,
                                         .resource_usage = std::max(0.01F, Util::random_float()) });
        }

        return events;
    }

    [[nodiscard]] auto spawn_process_builtin(const std::vector<Expression>& arguments) -> std::optional<Value>
    {
        constexpr static auto NAME = "spawn_process";
        constexpr static auto ARGC = 4;
        if (arguments.size() != ARGC) { report_function_call_mismatched_argc(NAME, arguments.size()); }

        std::size_t argument_count     = 0;
        const auto  process_name_value = TRY(evaluate_expression(arguments[argument_count++]));
        const auto  process_name       = TRY(process_name_value.as_string_or([&] -> std::optional<std::string_view> {
            return report_error(
              "mismatched type for argument #{} of builting `{}`: expected type `string`", argument_count - 1, NAME
            );
        }));

        const auto pid_value = TRY(evaluate_expression(arguments[argument_count++]));
        const auto pid       = TRY(pid_value.as_number_or([&] -> std::optional<std::size_t> {
            return report_error(
              "mismatched type for argument #{} of builting `{}`: expected type `int`", argument_count - 1, NAME
            );
        }));

        const auto arrival_value = TRY(evaluate_expression(arguments[argument_count++]));
        const auto arrival       = TRY(arrival_value.as_number_or([&] -> std::optional<std::size_t> {
            return report_error(
              "mismatched type for argument #{} of builting `{}`: expected type `int`", argument_count - 1, NAME
            );
        }));

        const auto list_value = TRY(evaluate_expression(arguments[argument_count++]));
        const auto list       = TRY(list_value.as_value_list_or([&] -> std::optional<std::vector<Value>> {
            report_error("mismatched type for argument #{} of builtin `{}`: expected type `List<Tuple: Event>`");
            return report_note("(e.g. [(event_type: `Io` or `Cpu`, duration: int)])");
        }));

        const auto events = TRY(list_as_events_deque(list));
        sim->emplace_process(process_name, pid, arrival, events);

        return Value();
    }

    [[nodiscard]] auto spawn_random_process_builtin(const std::vector<Expression>& arguments) -> std::optional<Value>
    {
        static std::vector<std::size_t> spawned_pids;

        constexpr static auto NAME = "spawn_process";
        constexpr static auto ARGC = 0;
        if (arguments.size() != ARGC) { report_function_call_mismatched_argc(NAME, arguments.size()); }

        auto pid = Util::random_natural(0, sim->max_processes);
        while (std::ranges::contains(spawned_pids, pid)) { pid = Util::random_natural(0, sim->max_processes); }
        spawned_pids.push_back(pid);

        const auto arrival = Util::random_natural(0, sim->max_arrival_time);

        std::deque<Os::Event> events;
        const auto            events_count = Util::random_natural(1, sim->max_events_per_process);
        for (std::size_t i = 0; i < events_count; ++i) { events.push_back(process_random_event()); }

        sim->emplace_process("Process", pid, arrival, events);

        return Value();
    }

    [[nodiscard]] auto process_random_event() const -> Os::Event
    {
        const auto kind =
          static_cast<Os::EventKind>(Util::random_natural(0, std::to_underlying(Os::EventKind::Count) - 1));

        const auto duration = Util::random_natural(1, sim->max_single_event_duration);

        return Os::Event {
            .kind           = kind,
            .duration       = duration,
            .resource_usage = std::max(0.01F, Util::random_float()),
        };
    }

    [[nodiscard]] auto builtin_handler(const std::string_view name, const std::vector<ExpressionId>& arguments)
      -> std::optional<Value>
    {
        const auto arguments_exprs = materialize_expressions(arguments);

        if (name == "spawn_process") { return spawn_process_builtin(arguments_exprs); }
        if (name == "spawn_random_process") { return spawn_random_process_builtin(arguments_exprs); }

        return Value();
    }

    static auto report_function_call_mismatched_argc(const std::string_view name, const std::size_t got)
      -> std::nullopt_t
    {
        return report_error(
          "failed to interpret call to builtin `{}`: expected 4 arguments, {} were provided", name, got
        );
    }

    template<typename... Args>
    static auto report_error(const std::string_view message, Args&&... args) -> std::nullopt_t
    {
        std::println(stderr, "[ERROR] (interpreter) {}", message, std::forward<Args>(args)...);
        return std::nullopt;
    }

    template<typename... Args>
    static auto report_note(const std::string_view message, Args&&... args) -> std::nullopt_t
    {
        std::println(stderr, "[NOTE] (interpreter) {}", message, std::forward<Args>(args)...);
        return std::nullopt;
    }

    [[nodiscard]] auto materialize_expressions(const std::vector<ExpressionId>& expr_ids) const
      -> std::vector<Expression>
    {
        return std::views::transform(
                 expr_ids, [this](const auto& expr_id) -> Expression { return ast.expression_by_id(expr_id); }
               )
               | std::ranges::to<std::vector>();
    }

    explicit Interpreter(const std::shared_ptr<Sim>& sim, Ast ast)
      : sim { sim },
        ast { std::move(ast) }
    {}

    std::shared_ptr<Sim> sim;
    Ast                  ast;
};

} // namespace Interpreter
