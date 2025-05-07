add_library(
    sim-lang
    Interpreter.cpp
    Lexer.cpp
    Parser.cpp
)
target_include_directories(sim-lang PRIVATE "../")
