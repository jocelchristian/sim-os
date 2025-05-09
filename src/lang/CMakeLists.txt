add_library(
    sim-lang
    Lexer.cpp
    Parser.cpp
)
target_include_directories(sim-lang PRIVATE "../")
