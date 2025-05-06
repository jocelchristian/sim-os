#pragma once

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <print>
#include <ranges>
#include <sstream>

#include "Lexer.hpp"
#include "Parser.hpp"

namespace Interpreter
{

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
        std::println("--- Lexing file ---");
        const auto tokens = Lexer::lex(file_content);
        if (!tokens) { return false; }

        std::println("- Tokens -");
        for (const auto &[idx, token] : std::views::zip(std::views::iota(0), *tokens)) { std::println("#{}: {}", idx, token); }

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

        return true;
    }

  private:
    explicit Interpreter(Simulation &sim);

    std::shared_ptr<Simulation> sim;
};

} // namespace Interpreter
