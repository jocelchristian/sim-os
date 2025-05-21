add_library(
    sim-lang
    Lexer.cpp
    Parser.cpp
)
target_include_directories(sim-lang PRIVATE "../")
target_link_libraries(sim-lang PUBLIC sim-util)
